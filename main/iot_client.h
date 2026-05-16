#ifndef MAIN_IOT_CLIENT_H_
#define MAIN_IOT_CLIENT_H_

#include <stdint.h>
#include "esp_err.h"

typedef struct {
    char state[32];
    char reason[48];

    char target_version[16];
    int target_build;

    int progress;

    uint32_t flags;
    uint32_t required_flags;
    uint32_t missing_flags;
    char missing_flags_text[128];

    char message[128];
    char error[128];

    uint32_t uptime_ms;
} iot_ota_log_event_t;

void iot_client_init(void);

esp_err_t iot_client_log_ota_event(const iot_ota_log_event_t* event);

esp_err_t iot_client_log_ota(const char* state, const char* reason, const char* target_version,
                             int target_build, int progress, const char* message,
                             const char* error);
#endif