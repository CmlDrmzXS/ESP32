/*
File Name: SmartConnect.c
File Description: This file is the source file of Smart Connect library.
Date:      13.10.2022
Author:    Cemal Durmaz
*/

// Libraries to import
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
#include "SmartConnect.h"

// FreeRTOS Event Group
EventGroupHandle_t smartconnect_wifi_event_group; /* //  BIT 0  //    BIT 1   //     BIT 2      */
                                                  /* // CONNECT // DISCONNECT // ESPTOUCH DONE */
EventGroupHandle_t smartconnect_task_event_group; /* //  BIT 0 //  BIT 1 */
                                                  /* //  FAIL  //  FUNC  */

// Max AP Scan Number
#define MAXIMUM_AP 30

// WiFi Credentials
#define MAX_RETRY 4
static int my_trial_number;
static int my_force;

// LOG Tags
static const char *SMART = "Smart Connect";

// Variables
static int my_retry_num = 0;
static uint8_t myssid[32] = {0};
static uint8_t mypassword[64] = {0};
static char *nvs_found_ssid;
static char *nvs_found_pass;
static char *my_aes_key;
static wifi_ap_record_t wifi_records[MAXIMUM_AP];
static uint16_t ap_count = 0;
static uint8_t my_flag = 0;
static bool str_comp_flag = 0;
static bool smart = 0;

// Prototypes
static void my_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data);
static void nvs_write_value(nvs_handle_t handle, const char *key, char *cert);
static void my_smartconfig_start(int aes_enab, char *aes_key);
static char *nvs_load_value_if_exist(nvs_handle_t handle, const char *key);

// Functions
static void my_event_group_init(void)
{
    smartconnect_wifi_event_group = xEventGroupCreate();
    smartconnect_task_event_group = xEventGroupCreate();
}

static void my_wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_LOGI(SMART, "WiFi Initialization is finished !");
}

static void my_wifi_start(void)
{
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &my_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &my_event_handler, NULL);
    esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &my_event_handler, NULL);

    wifi_config_t wifi_config = {};

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(SMART, "WiFi is started !");
}

static void my_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGE(SMART, "Reason: %d\n", event->reason);

        if (my_retry_num < MAX_RETRY)
        {
            if (my_flag == 1)
            {
                ESP_LOGI(SMART, "Smart Config is disabled now !");

                my_flag = 0;
            }
            ESP_ERROR_CHECK(esp_wifi_disconnect());
            vTaskDelay(pdMS_TO_TICKS(5000));
            ESP_ERROR_CHECK(esp_wifi_connect());
            ESP_LOGW(SMART, "Retrying to connect to the AP...");
            my_retry_num++;
        }
        else
        {
            xEventGroupSetBits(smartconnect_wifi_event_group, BIT1);
            vTaskDelay(pdMS_TO_TICKS(100));
            xEventGroupSetBits(smartconnect_task_event_group, BIT0);
            ESP_LOGE(SMART, "Failed to connect to the AP !");

            //if (my_force == 1)
            //{
            //    // PROGRAM SÜREKLİ BAŞA DÖNÜP DENEYECEK
            //}
            //else
            //{
            //    // PROGRAM DENEME TEKRARI KADAR DENEYECEK
            //}
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        my_retry_num = 0;
        xEventGroupSetBits(smartconnect_wifi_event_group, BIT0);

        if (smart)
        {
            nvs_handle_t my_handle;
            ESP_ERROR_CHECK(nvs_open("check", NVS_READWRITE, &my_handle));
            nvs_write_value(my_handle, "ssid", (char *)myssid);
            nvs_write_value(my_handle, "password", (char *)mypassword);
            nvs_close(my_handle);
        }

        ESP_LOGI(SMART, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(SMART, "Connection Established !");

        xEventGroupSetBits(smartconnect_task_event_group, BIT1);
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE)
    {
        ESP_LOGI(SMART, "Scan done !");
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL)
    {
        ESP_LOGI(SMART, "Channel found !");
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD)
    {
        ESP_LOGI(SMART, "Got SSID and Password !");

        smart = 1;

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true)
        {
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }
        memcpy(myssid, (evt)->ssid, sizeof(evt->ssid));
        memcpy(mypassword, evt->password, sizeof(evt->password));
        ESP_LOGI(SMART, "SSID:%s", myssid);
        ESP_LOGI(SMART, "PASSWORD:%s", mypassword);

        ESP_ERROR_CHECK(esp_wifi_disconnect());
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE)
    {
        xEventGroupSetBits(smartconnect_wifi_event_group, BIT2);
    }
}

static void my_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
}

static void my_wifi_scan(void)
{
    uint16_t number = MAXIMUM_AP;
    memset(wifi_records, 0, sizeof(wifi_records));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, wifi_records));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

    ESP_LOGI(SMART, "WiFi scan is completed ! Number of APs found nearby: %d", ap_count);

    for (int i = 0; i < ap_count; i++)
    {
        ESP_LOGI(SMART, "SSID: %s", (char *)wifi_records[i].ssid);
    }
}

static void nvs_write_value(nvs_handle_t handle, const char *key, char *cert)
{
    esp_err_t err;

    err = nvs_set_str(handle, key, cert);
    if (err == ESP_OK)
        ESP_LOGI(SMART, "OK !");
    else
        ESP_LOGE(SMART, "Nope !");

    err = nvs_commit(handle);
    if (err == ESP_OK)
        ESP_LOGI(SMART, "Committed !");
    else
        ESP_LOGE(SMART, "Failed to commit !");
}

static char *nvs_load_value_if_exist(nvs_handle_t handle, const char *key)
{
    size_t value_size;
    if (nvs_get_str(handle, key, NULL, &value_size) != ESP_OK)
    {
        ESP_LOGE(SMART, "Failed to get size of key: %s", key);
        return NULL;
    }

    char *value = malloc(value_size);
    if (nvs_get_str(handle, key, value, &value_size) != ESP_OK)
    {
        ESP_LOGE(SMART, "Failed to load key: %s", key);
        return NULL;
    }

    return value;
}

static void my_smartconfig_start(int aes_enab, char *aes_key)
{
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    vTaskDelay(pdMS_TO_TICKS(5000));

    if (aes_enab == 1 && aes_key != NULL)
    {
        ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_V2));
        ESP_LOGI(SMART, "Smart Config is started as V2.");
        smartconfig_start_config_t cfg =
            {.enable_log = 0,
             .esp_touch_v2_enable_crypt = aes_enab,
             .esp_touch_v2_key = aes_key};
        ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
    }
    else if (aes_enab == 0)
    {
        ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
        ESP_LOGI(SMART, "Smart Config is started as V1.");
        smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
    }

    while (1)
    {
        EventBits_t uxBits = xEventGroupWaitBits(smartconnect_wifi_event_group, BIT0 | BIT1 | BIT2,
                                                 true, false, portMAX_DELAY);

        if (uxBits & BIT0)
        {
            ESP_LOGI(SMART, "WiFi connected to the AP !");
        }
        if (uxBits & BIT2)
        {
            ESP_ERROR_CHECK(esp_smartconfig_stop());
            ESP_LOGI(SMART, "SmartConfig is over !");
            break;
        }
        if (uxBits & BIT1)
        {
            ESP_ERROR_CHECK(esp_smartconfig_stop());
            ESP_LOGE(SMART, "Smart Config has errors !");
            break;
        }
    }
}

static void my_network_changer(void)
{
    ESP_ERROR_CHECK(esp_wifi_stop());
    wifi_config_t wifi_config = {};
    strcpy((char *)(wifi_config.sta.ssid), nvs_found_ssid);
    strcpy((char *)(wifi_config.sta.password), nvs_found_pass);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void my_program_delete(void)
{
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &my_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &my_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(SC_EVENT, ESP_EVENT_ANY_ID, &my_event_handler));
    ESP_ERROR_CHECK(esp_event_loop_delete_default());

    vEventGroupDelete(smartconnect_task_event_group);
    vEventGroupDelete(smartconnect_wifi_event_group);

    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_stop());
    wifi_init_config_t cfg = {0};
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

static void my_program_create(void)
{
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &my_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &my_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &my_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    smartconnect_task_event_group = xEventGroupCreate();
    smartconnect_wifi_event_group = xEventGroupCreate();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

void my_app_main(int aes_e, char *aes_k, int force, int trial_number)
{
    my_aes_key = aes_k;
    my_trial_number = trial_number;
    my_force = force;

    my_nvs_init();
    my_event_group_init();
    my_wifi_init();
    my_wifi_scan();
    my_wifi_start();

    nvs_handle_t my_handle;
    ESP_ERROR_CHECK(nvs_open("check", NVS_READWRITE, &my_handle));
    nvs_found_ssid = nvs_load_value_if_exist(my_handle, "ssid");
    nvs_found_pass = nvs_load_value_if_exist(my_handle, "password");
    nvs_close(my_handle);

    if (nvs_found_ssid == NULL)
    {
        ESP_LOGE(SMART, "Read Value: NULL");
        my_smartconfig_start(aes_e, aes_k);
    }
    else
    {
        ESP_LOGI(SMART, "found SSID: %s", nvs_found_ssid);
        ESP_LOGI(SMART, "found PASS: %s", nvs_found_pass);
        for (int i = 0; i < ap_count; i++)
        {
            if (!(strcmp(nvs_found_ssid, (char *)wifi_records[i].ssid)))
            {
                ESP_LOGI(SMART, "Match !");
                str_comp_flag = 1;
                break;
            }
            else
            {
                str_comp_flag = 0;
            }
        }

        if (str_comp_flag == 1)
        {
            my_network_changer();
        }
        else
        {
            my_smartconfig_start(aes_e, aes_k);
        }
    }

    while (1)
    {
        EventBits_t FunctionBits = xEventGroupWaitBits(smartconnect_task_event_group, BIT0 | BIT1,
                                                       pdTRUE, pdFALSE, portMAX_DELAY);

        if (FunctionBits & BIT1)
        {
            ESP_LOGI(SMART, "Smart Connect function is executed without any errors !");
            return 1;
        }
        if (FunctionBits & BIT0)
        {
            ESP_LOGW(SMART, "Smart Connect function is encountered errors !");
            return 0;
        }
    }
}

int SmartConnect(int aes_e, char *aes_k, int force, int trial_number)
{
    my_app_main(aes_e, aes_k, force, trial_number);
}