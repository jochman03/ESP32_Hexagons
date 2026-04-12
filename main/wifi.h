/*
 * wifi.h
 *
 *  Created on: 14 apr 2026
 *      Author: jochman03
 */

#ifndef MAIN_WIFI_H_
#define MAIN_WIFI_H_

#include "config.h"

#define STA_CONNECTION_RETRY_COUNT 5

void wifi_init(void);
void wifi_start(const app_wifi_config_full_t* cfg);







#endif /* MAIN_WIFI_H_ */
