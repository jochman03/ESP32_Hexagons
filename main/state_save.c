#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "hex.h"
#include "state_save.h"

#define STATE_SAVE_DELAY_MS 30000
#define USER_LED_GPIO       GPIO_NUM_2

static const char* TAG = "STATE_SAVE";

typedef struct {
    uint8_t dummy;
} state_save_evt_t;

static QueueHandle_t state_save_queue = NULL;

static void state_save_task(void* arg);
static void user_led_blink(void);

void state_save_init(void) {
    state_save_queue = xQueueCreate(1, sizeof(state_save_evt_t));

    if (state_save_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create state_save_queue");
        return;
    }

    gpio_config_t led_cfg = {
        .pin_bit_mask = 1ULL << USER_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&led_cfg);
    gpio_set_level(USER_LED_GPIO, 0);

    xTaskCreate(state_save_task, "state_save_task", 4096, NULL, 4, NULL);
}

void state_save_notify(void) {
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
            app_hex_config_t current_state;
            if (hex_get_state(&current_state)) {
                if (config_save_hex_state(&current_state)) {
                    ESP_LOGI(TAG, "State saved");
                    user_led_blink();
                } else {
                    ESP_LOGE(TAG, "State save failed");
                }
            }
            break;
        }
    }
}

static void user_led_blink(void) {
    gpio_set_level(USER_LED_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(USER_LED_GPIO, 0);
}