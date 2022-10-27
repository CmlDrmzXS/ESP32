/*
File Name: SmartConnect.h
File Description: This file is the header file of Smart Connect library.
Date:      27.10.2022
Author:    Cemal Durmaz
*/
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"

int SmartConnect(int, char *);

typedef struct
{
    int aes_enab;
    char *aes_key;
} smart_config_t;

