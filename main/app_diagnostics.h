#ifndef MAIN_APP_DIAGNOSTICS_H_
#define MAIN_APP_DIAGNOSTICS_H_

#include <stdbool.h>
#include <stdint.h>

#define APP_DIAG_CONFIG_OK (1 << 0)
#define APP_DIAG_WIFI_OK   (1 << 1)
#define APP_DIAG_HTTP_OK   (1 << 2)
#define APP_DIAG_LED_OK    (1 << 3)

#define APP_DIAG_REQUIRED_MASK                                                                     \
    (APP_DIAG_CONFIG_OK | APP_DIAG_WIFI_OK | APP_DIAG_HTTP_OK | APP_DIAG_LED_OK)

void app_diagnostics_init(void);

void app_diagnostics_set(uint32_t flag);
void app_diagnostics_clear(uint32_t flag);

uint32_t app_diagnostics_get_flags(void);
bool app_diagnostics_is_healthy(void);

void app_diagnostics_start_ota_self_test(void);

#endif