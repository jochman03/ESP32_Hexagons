/*
 *  network_self_test.h
 *
 *  Created on: 15 May 2026
 *  Author: jochman03
 */

#ifndef NETWORK_SELF_TEST_H
#define NETWORK_SELF_TEST_H

#include <stdbool.h>
#include <stdint.h>

#define SELF_TEST_CONFIG_OK (1 << 0)
#define SELF_TEST_WIFI_OK   (1 << 1)
#define SELF_TEST_HTTP_OK   (1 << 2)
#define SELF_TEST_LED_OK    (1 << 3)

#define SELF_TEST_REQUIRED_MASK                                                                    \
    (SELF_TEST_CONFIG_OK | SELF_TEST_WIFI_OK | SELF_TEST_HTTP_OK | SELF_TEST_LED_OK)

void self_test_init(void);

void self_test_set_flags(uint32_t flag);
void self_test_clear_flags(uint32_t flag);

uint32_t self_test_get_flags(void);
bool self_test_passed(void);

void self_test_start_ota(void);

#endif // NETWORK_SELF_TEST_H