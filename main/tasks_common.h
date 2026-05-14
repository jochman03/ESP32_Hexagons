/*
 * tasks_common.h
 *
 *  Created on: 9 mar 2026
 *      Author: jochman03
 */

#ifndef MAIN_TASKS_COMMON_H_
#define MAIN_TASKS_COMMON_H_

// LED strip task
#define HEX_TASK_STACK_SIZE 2048
#define HEX_TASK_PRIORITY   5
#define HEX_TASK_CORE_ID    1

// Wifi application task
#define WIFI_APP_TASK_STACK_SIZE 4096
#define WIFI_APP_TASK_PRIORITY   5
#define WIFI_APP_TASK_CORE_ID    0

// HTTP Server task
#define HTTP_SERVER_TASK_STACK_SIZE 8192
#define HTTP_SERVER_TASK_PRIORITY   4
#define HTTP_SERVER_TASK_CORE_ID    0

// HTTP Server monitor task
#define HTTP_SERVER_MONITOR_STACK_SIZE 4096
#define HTTP_SERVER_MONITOR_PRIORITY   3
#define HTTP_SERVER_MONITOR_CORE_ID    0

#endif /* MAIN_TASKS_COMMON_H_ */
