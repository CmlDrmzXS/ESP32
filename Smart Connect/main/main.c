// Libraries
#include "SmartConnect.h"

void app_main(void)
{
    uint8_t exec_1 = SmartConnect(0, "Empa  Elektronik");
    printf("Exec Code: %d\n\r", exec_1);
}
