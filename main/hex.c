/*
 * hex.c
 *
 *  Created on: 9 mar 2026
 *      Author: jochman03
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tasks_common.h"
#include "led_strip.h"
#include "esp_log.h"
#include "hex.h"

static const char *TAG = "HEX";
led_strip_handle_t strip;

// Color buffer
uint8_t target_color_buffer[HEX_COUNT * 3];

// Color buffer Semaphores
SemaphoreHandle_t buffer_mutex;
SemaphoreHandle_t mode_mutex;
SemaphoreHandle_t speed_mutex;


typedef struct hex{
    bool active;
    uint8_t hold;
    uint8_t current_color[3];
} hex_t;

// Function Prototypes
static void HEX_task(void *pvParameter);

hex_mode_t animation_mode = STATIC;
hex_mode_t animation_mode_buffer = STATIC;
uint8_t animation_speed = 100;
uint8_t animation_speed_buffer = 100;

uint8_t starlight_timer = 0;
const uint8_t starlight_timer_max = 50;
const uint8_t starlight_hold_min = 40;
const uint8_t starlight_hold_max = 100;

const uint16_t fade_timer_max = 2000/20;
uint16_t fade_timer = 0;
uint8_t fade_index = 0;
bool fade_dir = 0;

hex_t hexes[HEX_COUNT];

static bool color_equals(const uint8_t a[3], const uint8_t b[3]){
    return (a[0] == b[0]) && (a[1] == b[1]) && (a[2] == b[2]);
}

static bool step_color_towards(uint8_t current[3], const uint8_t target[3]){
    bool target_is_black = (target[0] == 0 && target[1] == 0 && target[2] == 0);

    if (color_equals(current, target)) {
        return true;
    }

    if (target_is_black) {
        uint8_t maxc = current[0];
        if (current[1] > maxc) maxc = current[1];
        if (current[2] > maxc) maxc = current[2];

        if (maxc <= 4) {
            current[0] = 0;
            current[1] = 0;
            current[2] = 0;
            return true;
        }

        uint8_t step = (maxc > 2) ? 2 : 1;
        step = (step * animation_speed_buffer) / 100;
        if (step == 0) {
            step = 1;
        }

        uint16_t new_max = (maxc > step) ? (maxc - step) : 0;

        current[0] = (uint8_t)((((uint16_t)current[0] * new_max) + (maxc / 2)) / maxc);
        current[1] = (uint8_t)((((uint16_t)current[1] * new_max) + (maxc / 2)) / maxc);
        current[2] = (uint8_t)((((uint16_t)current[2] * new_max) + (maxc / 2)) / maxc);

        uint8_t new_maxc = current[0];
        if (current[1] > new_maxc) new_maxc = current[1];
        if (current[2] > new_maxc) new_maxc = current[2];

        if (new_maxc <= 2) {
            current[0] = 0;
            current[1] = 0;
            current[2] = 0;
            return true;
        }
    }
    else {
        for (uint8_t j = 0; j < 3; j++) {
            int16_t diff = (int16_t)target[j] - (int16_t)current[j];

            if (diff != 0) {
                int16_t step = diff / 32;

                step = (step * animation_speed_buffer) / 100;
                if (step > 2) step = 2;
                if (step < -2) step = -2;

                if (step == 0) step = (diff > 0) ? 1 : -1;

                int16_t next = (int16_t)current[j] + step;

                if (next < 0) next = 0;
                if (next > 255) next = 255;

                current[j] = (uint8_t)next;
            }
        }
    }

    return color_equals(current, target);
}

void hex_init(){

    led_strip_config_t strip_config = {
        .strip_gpio_num = STRIP_GPIO,
        .max_leds = HEX_COUNT,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };

    for(uint8_t i = 0; i < HEX_COUNT; ++i){
        hexes[i].active = false;
        hexes[i].hold = 0;
        for(uint8_t j = 0; j < 3; ++j){
            hexes[i].current_color[j] = 0;
        }
    }

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip));
	ESP_ERROR_CHECK(led_strip_clear(strip));
    buffer_mutex = xSemaphoreCreateMutex();
    mode_mutex = xSemaphoreCreateMutex();
    speed_mutex = xSemaphoreCreateMutex();

    ESP_LOGI(TAG, "init done");
	xTaskCreatePinnedToCore(&HEX_task, "HEX_task", HEX_TASK_STACK_SIZE, NULL, HEX_TASK_PRIORITY, NULL, HEX_TASK_CORE_ID);
}

void hex_setColor(int hex_index, uint8_t r, uint8_t g, uint8_t b){
    xSemaphoreTake(buffer_mutex, portMAX_DELAY);

	target_color_buffer[hex_index * 3] = r;
	target_color_buffer[hex_index * 3 + 1] = g;
	target_color_buffer[hex_index * 3 + 2] = b;

    xSemaphoreGive(buffer_mutex);
}

hex_mode_t hex_getMode(){
    xSemaphoreTake(mode_mutex, portMAX_DELAY);
    hex_mode_t mode = animation_mode;
    xSemaphoreGive(mode_mutex);
    return mode;
}

void hex_setMode(hex_mode_t mode){
    xSemaphoreTake(mode_mutex, portMAX_DELAY);
    animation_mode = mode;
    xSemaphoreGive(mode_mutex);
}

void hex_setSpeed(uint8_t speed){
    xSemaphoreTake(speed_mutex, portMAX_DELAY);
    animation_speed = speed;
    xSemaphoreGive(speed_mutex);
}

uint8_t hex_getSpeed(){
    uint8_t speed = 0;
    xSemaphoreTake(speed_mutex, portMAX_DELAY);
    speed = animation_speed;
    xSemaphoreGive(speed_mutex);
    return speed;
}

uint8_t hex_getColor_r(int hex_index){
    if(hex_index < 0 || hex_index >= HEX_COUNT) {
        ESP_LOGE(TAG, "Invalid LED index @ getTargetHexColor_r");
        return 0;
    }
    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    uint8_t color = target_color_buffer[hex_index * 3];
    xSemaphoreGive(buffer_mutex);
    return color;
}	
uint8_t hex_getColor_g(int hex_index){
    if(hex_index < 0 || hex_index >= HEX_COUNT) {
        ESP_LOGE(TAG, "Invalid LED index @ getTargetHexColor_g");
        return 0;
    }
    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    uint8_t color = target_color_buffer[hex_index * 3 + 1];
    xSemaphoreGive(buffer_mutex);
    return color;
}	
uint8_t hex_getColor_b(int hex_index){
    if(hex_index < 0 || hex_index >= HEX_COUNT) {
        ESP_LOGE(TAG, "Invalid LED index @ getTargetHexColor_b");
        return 0;
    }
    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    uint8_t color = target_color_buffer[hex_index * 3 + 2];
    xSemaphoreGive(buffer_mutex);
    return color;
}	

void getTargetHexColor(int hex_index, uint8_t* color_buffer){
    if(hex_index < 0 || hex_index >= HEX_COUNT) {
        ESP_LOGE(TAG, "Invalid LED index @ getTargetHexColor");
        return;
    }
    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    color_buffer[0] = target_color_buffer[hex_index * 3];
    color_buffer[1] = target_color_buffer[hex_index * 3 + 1];
    color_buffer[2] = target_color_buffer[hex_index * 3 + 2];
    xSemaphoreGive(buffer_mutex);
}

static void HEX_animate(){

    if (xSemaphoreTake(mode_mutex, portMAX_DELAY) == pdTRUE) {
        animation_mode_buffer = animation_mode;
        xSemaphoreGive(mode_mutex);
    }
    if (xSemaphoreTake(speed_mutex, portMAX_DELAY) == pdTRUE) {
        animation_speed_buffer = animation_speed;
        xSemaphoreGive(speed_mutex);
    }
    switch(animation_mode_buffer){
        case STATIC:
            for(uint8_t i = 0; i < HEX_COUNT; ++i){
                uint8_t target_color[3];
                getTargetHexColor(i, target_color);
                step_color_towards(hexes[i].current_color, target_color);

                led_strip_set_pixel(strip, i, hexes[i].current_color[0], hexes[i].current_color[1],
                    hexes[i].current_color[2]);

            }
            break;
        case STARLIGHT:
            starlight_timer++;
            if(starlight_timer >= (starlight_timer_max*animation_speed_buffer)/100){
                starlight_timer = 0;
                if(rand()%5 != 0){
                    int led = rand() % HEX_COUNT;
                    if(!hexes[led].active){
                        hexes[led].current_color[0] = 0;
                        hexes[led].current_color[1] = 0;
                        hexes[led].current_color[2] = 0;
                        hexes[led].hold = ((starlight_hold_min + rand()%starlight_hold_max)*100)/animation_speed_buffer;
                        hexes[led].active = true;
                    }
                }
            }

            for(uint8_t i = 0; i < HEX_COUNT; ++i){
                uint8_t target_color[3];

                if(!hexes[i].active){
                    target_color[0] = 0;
                    target_color[1] = 0;
                    target_color[2] = 0;
                }
                else{
                    getTargetHexColor(i, target_color);
                }
                bool reached_target = step_color_towards(hexes[i].current_color, target_color);

                if(reached_target == 1){
                    if(hexes[i].hold > 0){
                        hexes[i].hold--;
                    }
                    else{
                        hexes[i].active = false;
                    }
                }
                led_strip_set_pixel(strip, i, hexes[i].current_color[0], hexes[i].current_color[1],
                     hexes[i].current_color[2]);
            }
            break;
        case FADE:
            fade_timer++;
            if(fade_timer >= (fade_timer_max*100)/animation_speed_buffer){
                fade_index++;   
                fade_timer = 0;
                if(fade_index >= HEX_COUNT){
                    fade_index = 0;
                    fade_dir = !fade_dir;
                }
            }
            for(uint8_t i = 0; i < HEX_COUNT; ++i){
                uint8_t target_color[3];
                switch(fade_dir){
                    case 0:
                        if (i <= fade_index){
                            getTargetHexColor(i, target_color);
                        }
                        else{
                            for(int j = 0; j < 3; ++j){
                                target_color[j] = 0;
                            } 
                        }
                        break;
                    case 1:
                        if(i <= fade_index){
                            for(int j = 0; j < 3; ++j){
                                target_color[j] = 0;
                            }                         
                        }
                        else{
                            getTargetHexColor(i, target_color);

                        }
                        break;
                }
                step_color_towards(hexes[i].current_color, target_color);
                
                led_strip_set_pixel(strip, i, hexes[i].current_color[0], hexes[i].current_color[1],
                     hexes[i].current_color[2]);

            }
            break;
    }
    led_strip_refresh(strip);

}

static void static_test(){
    for(int k = 0; k < 2; ++k){
        animation_mode = STATIC;
        for(int i = 0; i < HEX_COUNT * 3; ++i){
            target_color_buffer[i] = (i % 2) ? 255 : 0;
        }
        for(int i = 0; i < 250; ++i){
            HEX_animate();
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        for(int i = 0; i < HEX_COUNT * 3; ++i){
            target_color_buffer[i] = 0;
        }
        for(int i = 0; i < 250; ++i){
            HEX_animate();
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}
static void starlight_test(){
        animation_mode = STARLIGHT;
        for(int j = 0; j < 2; ++j){
            for(int i = 0; i < HEX_COUNT; ++i){
                target_color_buffer[i * 3 + 2] = 150;
            }
            for(int i = 0; i < 250; ++i){
                HEX_animate();
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            for(int i = 0; i < HEX_COUNT; ++i){
                target_color_buffer[i * 3 + 1] = 100;
            }
            for(int i = 0; i < 250; ++i){
                HEX_animate();
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        }
}

static void fade_test(){
        animation_mode = FADE;
        for(int j = 0; j < 2; ++j){
            for(int i = 0; i < HEX_COUNT; ++i){
                target_color_buffer[i * 3 + 1] = 150;
            }
            for(int i = 0; i < 250; ++i){
                HEX_animate();
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            for(int i = 0; i < HEX_COUNT; ++i){
                target_color_buffer[i * 3 + 2] = 100;
            }
            for(int i = 0; i < 250; ++i){
                HEX_animate();
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        }
}


static void HEX_task(void *pvParameter){
    while(1){
        HEX_animate();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}


