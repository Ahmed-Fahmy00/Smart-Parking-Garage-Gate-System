/*******************************************************************************
 * status_task.c
 * Low-priority task that demonstrates the Limit Semaphore usage.
 *
 * Each time a limit button is pressed the Input task gives xLimitSemaphore.
 * This task wakes up, flashes the on-board Blue LED (PF2) for 200 ms as
 * visual confirmation that a limit has been reached, then goes back to
 * waiting.
 *
 * This task fulfils the project requirement of using a semaphore to signal
 * limit-button activation, and provides an easy visual cue during demos.
 ******************************************************************************/
#include "status_task.h"
#include "gate_control.h"
#include "gpio_config.h"
#include "basic_io.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define BLUE_FLASH_DURATION_MS   200

void StatusTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        /* Block until a limit button is pressed */
        if (xSemaphoreTake(xLimitSemaphore, portMAX_DELAY) == pdTRUE)
        {
            /* Flash Blue LED */
            LED_Blue_On();
            vTaskDelay(pdMS_TO_TICKS(BLUE_FLASH_DURATION_MS));
            LED_Blue_Off();
            vPrintString("Limit button handled\r\n");
        }
    }
}
