// Libraries
#include "SmartConnect.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_log.h"

void app_main(void)
{
    static const char* TAG = "My Program";
    uint8_t exec = SmartConnect(0,  "Two One Nine Two", 0,  3);
    printf("Exec Code: %d\n\r", exec);
}
