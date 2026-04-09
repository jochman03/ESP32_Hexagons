/*
 * wifi_app.h
 *
 *  Created on: 2 gru 2025
 *      Author: jakub
 */

#ifndef MAIN_WIFI_APP_H_
#define MAIN_WIFI_APP_H_

#include "esp_netif.h"
#include "esp_wifi_types_generic.h"
#include "portmacro.h"

#define WIFI_AP_SSID 			"ESP32_AP"
#define WIFI_AP_PASSWORD 		"password"
#define WIFI_AP_CHANNEL			1
#define WIFI_AP_SSID_HIDDEN 	1
#define WIFI_AP_MAX_CONNECTIONS 5
#define WIFI_AP_BEACON_INTERVAL 100 //ms
#define WIFI_AP_IP 				"192.168.0.1"
#define WIFI_AP_GATEWAY			"192.168.0.1"
#define WIFI_AP_NETMASK			"255.255.255.0"
#define WIFI_AP_BANDWIDTH		WIFI_BW_HT20 //bandwidth 20Mhz
#define WIFI_STA_POWER_SAVE		WIFI_PS_NONE //Power save not used
#define MAX_SSID_LENGTH			32
#define MAX_PASSWORD_LENGTH 	64
#define MAX_CONNECTION_RETRIES	5		//Retry number on disconnect

// Netif object for the station and access point
extern esp_netif_t* esp_netif_sta;
extern esp_netif_t* esp_netif_ap;

// Message IDs for the wifi application task
typedef enum wifi_app_message{
	WIFI_APP_MSG_START_HTTP_SERVER = 0,
	WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER,
	WIFI_APP_MESSAGE_STA_CONNECTED_GOT_IP
} wifi_app_message_e;

// Structure for the message queue
typedef struct widi_app_queue_message{
	wifi_app_message_e msgID;
} wifi_app_queue_message_t;

// Sends a message to the queue
// @param msgID message ID from wifi_app_message_e enum
// return pdTrue if an item was successfully sent to the queue, otherwise pdFalse
BaseType_t wifi_app_send_message(wifi_app_message_e msgID);

// Starts wifi RTOS Task
void wifi_app_start();

#endif /* MAIN_WIFI_APP_H_ */
