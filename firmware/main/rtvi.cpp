// Adapted from pipecat-ai/pipecat-esp32 (MIT License), esp32-m5stack-cores3/src/rtvi.cpp.
// https://github.com/pipecat-ai/pipecat-esp32 @ e70e3b1

#include <cJSON.h>
#include <esp_log.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "main.h"

#define MAX_TYPE_LEN 32
#define MAX_ID_LEN 64

static int rtvi_id = 0;
static QueueHandle_t rtvi_queue;
static PeerConnection *peer_connection = NULL;
static rtvi_callbacks_t *rtvi_callbacks = NULL;

typedef struct {
  cJSON *msg;
} rtvi_msg_t;

// Simple hashing function so we can fake pattern matching and switch on strings
// as a constexpr so it gets evaluated in compile time for static strings
static constexpr unsigned int hash(const char *s, int off = 0) {
  return !s[off] ? 5381 : (hash(s, off + 1) * 33) ^ s[off];
}

static rtvi_msg_t *create_rtvi_message(const char *type) {
  cJSON *j_msg = cJSON_CreateObject();

  if (j_msg == NULL) {
    ESP_LOGE(LOG_TAG, "Unable to create RTVI message");
    return NULL;
  }
  if (cJSON_AddStringToObject(j_msg, "label", "rtvi-ai") == NULL) {
    cJSON_Delete(j_msg);
    ESP_LOGE(LOG_TAG, "Unable to create RTVI message");
    return NULL;
  }
  if (cJSON_AddStringToObject(j_msg, "type", type) == NULL) {
    cJSON_Delete(j_msg);
    ESP_LOGE(LOG_TAG, "Unable to create RTVI message");
    return NULL;
  }

  char id[MAX_ID_LEN];
  sprintf(id, "%d", rtvi_id++);
  if (cJSON_AddStringToObject(j_msg, "id", id) == NULL) {
    cJSON_Delete(j_msg);
    ESP_LOGE(LOG_TAG, "Unable to create RTVI message");
    return NULL;
  }

  rtvi_msg_t *msg = (rtvi_msg_t *)malloc(sizeof(rtvi_msg_t));
  msg->msg = j_msg;

  return msg;
}

static void destroy_rtvi_message(rtvi_msg_t *msg) {
  cJSON_Delete(msg->msg);
  free(msg);
}

static char *rtvi_message_to_string(rtvi_msg_t *msg) {
  if (msg == NULL || msg->msg == NULL) {
    return NULL;
  }

  char *msg_str = cJSON_Print(msg->msg);

  return msg_str;
}

static void rtvi_handle_message(const rtvi_msg_t *msg) {
  cJSON *j_type = cJSON_GetObjectItem(msg->msg, "type");
  if (j_type == NULL) {
    ESP_LOGE(LOG_TAG, "Unable to find `type` field in RTVI message");
    return;
  }

  switch (hash(j_type->valuestring)) {
    case hash("bot-started-speaking"):
      rtvi_callbacks->on_bot_started_speaking();
      break;
    case hash("bot-stopped-speaking"):
      rtvi_callbacks->on_bot_stopped_speaking();
      break;
    case hash("bot-tts-text"): {
      cJSON *j_data = cJSON_GetObjectItem(msg->msg, "data");
      cJSON *j_text = cJSON_GetObjectItem(j_data, "text");
      rtvi_callbacks->on_bot_tts_text(j_text->valuestring);
      break;
    }
    default:
      break;
  }
}

static void rtvi_task(void *pvParameter) {
  rtvi_msg_t msg;

  while (1) {
    if (xQueueReceive(rtvi_queue, &msg, portMAX_DELAY)) {
      rtvi_handle_message(&msg);
      cJSON_Delete(msg.msg);
    }
  }
}

void pipecat_init_rtvi(PeerConnection *connection,
                       rtvi_callbacks_t *callbacks) {
  peer_connection = connection;
  rtvi_callbacks = callbacks;

  rtvi_queue = xQueueCreate(10, sizeof(rtvi_msg_t));
  xTaskCreatePinnedToCore(rtvi_task, "RTVI Task", 4096, NULL, 2, NULL, 1);
}

void pipecat_rtvi_send_client_ready() {
  if (peer_connection == NULL) {
    ESP_LOGE(LOG_TAG, "Cannot send RTVI client-ready: no peer connection");
    return;
  }

  rtvi_msg_t *msg = create_rtvi_message("client-ready");
  if (msg == NULL) {
    return;
  }

  // A client-ready with no `version` still proceeds (the server just logs a
  // "Client version unknown" warning and sends back an error-response), but
  // declare the server's own RTVI.PROTOCOL_VERSION anyway to avoid that noise.
  // This client only ever parses bot-started-speaking / bot-stopped-speaking /
  // bot-tts-text, none of which differ between the legacy (1.x) and current
  // bot-output wire formats, so advertising the current protocol version is safe.
  cJSON *j_data = cJSON_AddObjectToObject(msg->msg, "data");
  if (j_data != NULL) {
    cJSON_AddStringToObject(j_data, "version", "2.1.0");
    cJSON *j_about = cJSON_AddObjectToObject(j_data, "about");
    if (j_about != NULL) {
      cJSON_AddStringToObject(j_about, "library", "pipecat-esp32");
      cJSON_AddStringToObject(j_about, "platform", "esp32s3");
    }
  }

  char *msg_str = rtvi_message_to_string(msg);
  if (msg_str == NULL) {
    destroy_rtvi_message(msg);
    return;
  }

  peer_connection_datachannel_send(peer_connection, msg_str, strlen(msg_str));
  ESP_LOGI(LOG_TAG, "Sent RTVI client-ready");

  cJSON_free(msg_str);

  destroy_rtvi_message(msg);
}

void pipecat_rtvi_handle_message(const char *msg) {
  cJSON *j_msg = cJSON_Parse(msg);
  if (j_msg == NULL) {
    ESP_LOGE(LOG_TAG, "Error parsing RTVI message");
    return;
  }

  rtvi_msg_t rtvi_msg = {.msg = j_msg};

  xQueueSend(rtvi_queue, &rtvi_msg, portMAX_DELAY);
}
