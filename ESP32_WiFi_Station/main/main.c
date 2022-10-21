/* Libraries */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

/* WiFi Credentials*/
#define MY_WIFI_SSID ("EXAMPLE_SSID")
#define MY_WIFI_PASS ("EXAMPLE_PASS")
#define MY_WIFI_MAX_RETRY ("EXAMPLE_MAX_RETRY")



void app_main(void)
{

}