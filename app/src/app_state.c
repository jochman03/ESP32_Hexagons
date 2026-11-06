#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "platform_board.h"

#include "app_config.h"
#include "app_hex.h"
#include "app_state.h"

#define STATE_SAVE_DELAY_MS 30000

static const char* TAG = "APP_STATE";

typedef struct {
    uint8_t dummy;
} state_save_evt_t;

static QueueHandle_t state_save_queue = NULL;

static void state_save_task(void* arg);

void app_state_init(void) {
    state_save_queue = xQueueCreate(1, sizeof(state_save_evt_t));

    if (state_save_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create state_save_queue");
        return;
    }
    xTaskCreate(state_save_task, "state_save_task", 4096, NULL, 4, NULL);
}

void app_state_notify(void) {
    if (state_save_queue == NULL) {
        return;
    }

    state_save_evt_t evt = {0};

    xQueueOverwrite(state_save_queue, &evt);
}

static void state_save_task(void* arg) {
    state_save_evt_t evt;

    while (1) {
        if (xQueueReceive(state_save_queue, &evt, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        ESP_LOGI(TAG, "State changed, waiting before save");

        while (1) {
            if (xQueueReceive(state_save_queue, &evt, pdMS_TO_TICKS(STATE_SAVE_DELAY_MS)) ==
                pdTRUE) {
                ESP_LOGI(TAG, "State changed again, restarting save timer");
                continue;
            }

            ESP_LOGI(TAG, "Saving state to NVS");
            app_config_hex_t current_state;
            if (hex_get_state(&current_state)) {
                if (app_config_hex_save(&current_state)) {
                    ESP_LOGI(TAG, "State saved");

                    platform_board_status_led_set(true);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    platform_board_status_led_set(false);

                } else {
                    ESP_LOGE(TAG, "State save failed");
                }
            }
            break;
        }
    }
}