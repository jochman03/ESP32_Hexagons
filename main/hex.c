#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tasks_common.h"
#include "led_strip.h"
#include "esp_log.h"

#include "hex.h"
#include "state_save.h"

static const char* TAG = "HEX";
led_strip_handle_t gStrip;

// Semaphore
static SemaphoreHandle_t gStateMutex;

// Hex struct
typedef struct {
    bool active;
    uint8_t hold;
    uint8_t currentColor[3];
} Hex_T;

// Function Prototypes
static void hex_task(void* pvParameter);
static bool hex_make_snapshot(void);
static bool step_color(uint8_t currentColor[3], const uint8_t targetColor[3]);

// Global animation variables
static HexMode_T gAnimationMode = STATIC;
static uint8_t gAnimationSpeed = 100;
static bool gOutputEnabled = true;

static HexMode_T gAnimationModeBuffer = STATIC;
static uint8_t gAnimationSpeedBuffer = 100;
static bool gOutputEnabledBuffer = true;

static uint8_t gTargetColorBuffer[HEX_COUNT * 3];
static uint8_t gTargetColorFrame[HEX_COUNT * 3];

// Global starlight effect variables
uint8_t gStarlightTimer = 0;
uint8_t gStarlightTimerMax = 50;
uint8_t gStarlightHoldMin = 40;
uint8_t gStarlightHoldMax = 100;

// Global fade effect variables
const uint16_t gFadeTimerMax = 100;
uint16_t gFadeTimer = 0;
uint8_t gFadeIndex = 0;
bool gFadeDir = 0;

Hex_T gHexes[HEX_COUNT];

static bool color_equals(const uint8_t a[3], const uint8_t b[3]) {
    return (a[0] == b[0]) && (a[1] == b[1]) && (a[2] == b[2]);
}

void hex_init() {
    led_strip_config_t hStripConfig = {
        .strip_gpio_num = STRIP_GPIO,
        .max_leds = HEX_COUNT,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t hRmtConfig = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };

    for (uint8_t i = 0; i < HEX_COUNT; ++i) {
        gHexes[i].active = false;
        gHexes[i].hold = 0;
        for (uint8_t j = 0; j < 3; ++j) {
            gHexes[i].currentColor[j] = 0;
        }
    }

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&hStripConfig, &hRmtConfig, &gStrip));
    ESP_ERROR_CHECK(led_strip_clear(gStrip));
    gStateMutex = xSemaphoreCreateMutex();
    if (gStateMutex == NULL) {
        ESP_LOGE(TAG, "Failed to create HEX state mutex");
        return;
    }

    memset(gTargetColorBuffer, 0, sizeof(gTargetColorBuffer));
    memset(gTargetColorFrame, 0, sizeof(gTargetColorFrame));

    ESP_LOGI(TAG, "init done");
    xTaskCreatePinnedToCore(&hex_task, "HEX_task", HEX_TASK_STACK_SIZE, NULL, HEX_TASK_PRIORITY,
                            NULL, HEX_TASK_CORE_ID);
}

void hex_set_color(int hexIndex, uint8_t r, uint8_t g, uint8_t b) {
    if (hexIndex < 0 || hexIndex >= HEX_COUNT) {
        ESP_LOGE(TAG, "Invalid LED index @ hex_set_color: %d", hexIndex);
        return;
    }

    if (xSemaphoreTake(gStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        gTargetColorBuffer[hexIndex * 3] = r;
        gTargetColorBuffer[hexIndex * 3 + 1] = g;
        gTargetColorBuffer[hexIndex * 3 + 2] = b;
        xSemaphoreGive(gStateMutex);
    }

    state_save_notify();
}

HexMode_T hex_get_mode(void) {
    HexMode_T mode = STATIC;

    if (xSemaphoreTake(gStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        mode = gAnimationMode;
        xSemaphoreGive(gStateMutex);
    }

    return mode;
}

void hex_set_mode(HexMode_T mode) {
    if (xSemaphoreTake(gStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        gAnimationMode = mode;
        xSemaphoreGive(gStateMutex);
    }

    state_save_notify();
}

void hex_set_speed(uint8_t speed) {
    if (speed == 0) {
        speed = 1;
    }

    if (xSemaphoreTake(gStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        gAnimationSpeed = speed;
        xSemaphoreGive(gStateMutex);
    }

    state_save_notify();
}

uint8_t hex_get_speed(void) {
    uint8_t speed = 100;

    if (xSemaphoreTake(gStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        speed = gAnimationSpeed;
        xSemaphoreGive(gStateMutex);
    }

    return speed;
}

void hex_set_enabled(bool enabled) {
    if (xSemaphoreTake(gStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        gOutputEnabled = enabled;
        xSemaphoreGive(gStateMutex);
        state_save_notify();
    }
}

bool hex_is_enabled(void) {
    bool enabled = true;

    if (xSemaphoreTake(gStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        enabled = gOutputEnabled;
        xSemaphoreGive(gStateMutex);
    }

    return enabled;
}

uint8_t hex_get_color_r(int hexIndex) {
    if (hexIndex < 0 || hexIndex >= HEX_COUNT) {
        ESP_LOGE(TAG, "Invalid LED index @ hex_get_color_r");
        return 0;
    }
    uint8_t color = 0;
    if (xSemaphoreTake(gStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        color = gTargetColorBuffer[hexIndex * 3];
        xSemaphoreGive(gStateMutex);
    }

    return color;
}
uint8_t hex_get_color_g(int hexIndex) {
    if (hexIndex < 0 || hexIndex >= HEX_COUNT) {
        ESP_LOGE(TAG, "Invalid LED index @ hex_get_color_g");
        return 0;
    }
    uint8_t color = 0;
    if (xSemaphoreTake(gStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        color = gTargetColorBuffer[hexIndex * 3 + 1];
        xSemaphoreGive(gStateMutex);
    }
    return color;
}
uint8_t hex_get_color_b(int hexIndex) {
    if (hexIndex < 0 || hexIndex >= HEX_COUNT) {
        ESP_LOGE(TAG, "Invalid LED index @ hex_get_color_b");
        return 0;
    }
    uint8_t color = 0;
    if (xSemaphoreTake(gStateMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        color = gTargetColorBuffer[hexIndex * 3 + 2];
        xSemaphoreGive(gStateMutex);
    }
    return color;
}

bool hex_get_state(app_hex_config_t* state) {
    if (state == NULL) {
        return false;
    }

    if (xSemaphoreTake(gStateMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }

    state->version = HEX_STATE_VERSION;
    state->valid = true;
    state->mode = (uint8_t)gAnimationMode;
    state->speed = gAnimationSpeed;
    state->enabled = gOutputEnabled;

    memcpy(state->colors, gTargetColorBuffer, sizeof(state->colors));

    xSemaphoreGive(gStateMutex);

    return true;
}

bool hex_apply_state(const app_hex_config_t* state) {
    if (state == NULL) {
        return false;
    }

    if (!state->valid || state->version != HEX_STATE_VERSION) {
        return false;
    }

    if (xSemaphoreTake(gStateMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }

    gAnimationMode = (HexMode_T)state->mode;
    gAnimationSpeed = state->speed;
    gOutputEnabled = state->enabled;

    if (gAnimationSpeed == 0) {
        gAnimationSpeed = 1;
    }

    memcpy(gTargetColorBuffer, state->colors, sizeof(gTargetColorBuffer));

    xSemaphoreGive(gStateMutex);

    return true;
}

static void get_target_hex_color(int hexIndex, uint8_t* color_buffer) {
    if (hexIndex < 0 || hexIndex >= HEX_COUNT) {
        ESP_LOGE(TAG, "Invalid LED index @ get_target_hex_color");
        color_buffer[0] = 0;
        color_buffer[1] = 0;
        color_buffer[2] = 0;
        return;
    }

    color_buffer[0] = gTargetColorFrame[hexIndex * 3];
    color_buffer[1] = gTargetColorFrame[hexIndex * 3 + 1];
    color_buffer[2] = gTargetColorFrame[hexIndex * 3 + 2];
}

static void hex_animate() {
    if (!hex_make_snapshot()) {
        return;
    }

    if (!gOutputEnabledBuffer) {
        for (uint8_t i = 0; i < HEX_COUNT; ++i) {
            uint8_t black[3] = {0, 0, 0};

            step_color(gHexes[i].currentColor, black);

            led_strip_set_pixel(gStrip, i, gHexes[i].currentColor[0], gHexes[i].currentColor[1],
                                gHexes[i].currentColor[2]);
        }

        led_strip_refresh(gStrip);
        return;
    }
    switch (gAnimationModeBuffer) {
        case STATIC:
            for (uint8_t i = 0; i < HEX_COUNT; ++i) {
                uint8_t targetColor[3];
                get_target_hex_color(i, targetColor);
                step_color(gHexes[i].currentColor, targetColor);

                led_strip_set_pixel(gStrip, i, gHexes[i].currentColor[0], gHexes[i].currentColor[1],
                                    gHexes[i].currentColor[2]);
            }
            break;
        case STARLIGHT:
            gStarlightTimer++;
            if (gStarlightTimer >= (gStarlightTimerMax * 100) / gAnimationSpeedBuffer) {
                gStarlightTimer = 0;
                if (rand() % 5 != 0) {
                    int led = rand() % HEX_COUNT;
                    if (!gHexes[led].active) {
                        gHexes[led].currentColor[0] = 0;
                        gHexes[led].currentColor[1] = 0;
                        gHexes[led].currentColor[2] = 0;
                        gHexes[led].hold =
                            ((gStarlightHoldMin + rand() % gStarlightHoldMax) * 100) /
                            gAnimationSpeedBuffer;
                        gHexes[led].active = true;
                    }
                }
            }

            for (uint8_t i = 0; i < HEX_COUNT; ++i) {
                uint8_t targetColor[3];

                if (!gHexes[i].active) {
                    targetColor[0] = 0;
                    targetColor[1] = 0;
                    targetColor[2] = 0;
                } else {
                    get_target_hex_color(i, targetColor);
                }
                bool reached_target = step_color(gHexes[i].currentColor, targetColor);

                if (reached_target == 1) {
                    if (gHexes[i].hold > 0) {
                        gHexes[i].hold--;
                    } else {
                        gHexes[i].active = false;
                    }
                }
                led_strip_set_pixel(gStrip, i, gHexes[i].currentColor[0], gHexes[i].currentColor[1],
                                    gHexes[i].currentColor[2]);
            }
            break;
        case FADE:
            gFadeTimer++;
            if (gFadeTimer >= (gFadeTimerMax * 100) / gAnimationSpeedBuffer) {
                gFadeIndex++;
                gFadeTimer = 0;
                if (gFadeIndex >= HEX_COUNT) {
                    gFadeIndex = 0;
                    gFadeDir = !gFadeDir;
                }
            }
            for (uint8_t i = 0; i < HEX_COUNT; ++i) {
                uint8_t targetColor[3];
                switch (gFadeDir) {
                    case 0:
                        if (i <= gFadeIndex) {
                            get_target_hex_color(i, targetColor);
                        } else {
                            for (int j = 0; j < 3; ++j) {
                                targetColor[j] = 0;
                            }
                        }
                        break;
                    case 1:
                        if (i <= gFadeIndex) {
                            for (int j = 0; j < 3; ++j) {
                                targetColor[j] = 0;
                            }
                        } else {
                            get_target_hex_color(i, targetColor);
                        }
                        break;
                }
                step_color(gHexes[i].currentColor, targetColor);

                led_strip_set_pixel(gStrip, i, gHexes[i].currentColor[0], gHexes[i].currentColor[1],
                                    gHexes[i].currentColor[2]);
            }
            break;
    }
    led_strip_refresh(gStrip);
}

static void hex_task(void* pvParameter) {
    while (1) {
        hex_animate();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static bool step_color(uint8_t currentColor[3], const uint8_t targetColor[3]) {
    bool bTargetIsBlack = (targetColor[0] == 0 && targetColor[1] == 0 && targetColor[2] == 0);

    if (color_equals(currentColor, targetColor)) {
        return true;
    }

    if (bTargetIsBlack) {
        uint8_t maxColor = currentColor[0];
        if (currentColor[1] > maxColor) {
            maxColor = currentColor[1];
        }
        if (currentColor[2] > maxColor) {
            maxColor = currentColor[2];
        }

        if (maxColor <= 4) {
            currentColor[0] = 0;
            currentColor[1] = 0;
            currentColor[2] = 0;
            return true;
        }

        uint8_t step = (maxColor > 2) ? 2 : 1;
        step = (step * gAnimationSpeedBuffer) / 100;
        if (step == 0) {
            step = 1;
        }

        uint16_t difference = (maxColor > step) ? (maxColor - step) : 0;

        currentColor[0] =
            (uint8_t)((((uint16_t)currentColor[0] * difference) + (maxColor / 2)) / maxColor);
        currentColor[1] =
            (uint8_t)((((uint16_t)currentColor[1] * difference) + (maxColor / 2)) / maxColor);
        currentColor[2] =
            (uint8_t)((((uint16_t)currentColor[2] * difference) + (maxColor / 2)) / maxColor);

        uint8_t newMaxColor = currentColor[0];
        if (currentColor[1] > newMaxColor) {
            newMaxColor = currentColor[1];
        }
        if (currentColor[2] > newMaxColor) {
            newMaxColor = currentColor[2];
        }
        if (newMaxColor <= 2) {
            currentColor[0] = 0;
            currentColor[1] = 0;
            currentColor[2] = 0;
            return true;
        }
    } else {
        for (uint8_t j = 0; j < 3; j++) {
            int16_t difference = (int16_t)targetColor[j] - (int16_t)currentColor[j];
            if (difference != 0) {
                int16_t step = difference / 32;
                step = (step * gAnimationSpeedBuffer) / 100;
                if (step > 2) {
                    step = 2;
                }
                if (step < -2) {
                    step = -2;
                }
                if (step == 0) {
                    step = (difference > 0) ? 1 : -1;
                }

                int16_t updatedColor = (int16_t)currentColor[j] + step;
                if (updatedColor < 0)
                    updatedColor = 0;
                if (updatedColor > 255)
                    updatedColor = 255;
                currentColor[j] = (uint8_t)updatedColor;
            }
        }
    }
    return color_equals(currentColor, targetColor);
}

static bool hex_make_snapshot(void) {
    if (xSemaphoreTake(gStateMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }

    gAnimationModeBuffer = gAnimationMode;
    gAnimationSpeedBuffer = gAnimationSpeed;
    gOutputEnabledBuffer = gOutputEnabled;

    memcpy(gTargetColorFrame, gTargetColorBuffer, sizeof(gTargetColorFrame));

    xSemaphoreGive(gStateMutex);

    if (gAnimationSpeedBuffer == 0) {
        gAnimationSpeedBuffer = 1;
    }

    return true;
}