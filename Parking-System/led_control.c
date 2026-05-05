/*******************************************************************************
 * led_control.c
 * Medium-priority task that reads the shared gate state (mutex-protected)
 * and drives the Red and Green LEDs accordingly.
 *
 *  State              | Green LED | Red LED
 *  -------------------+-----------+--------
 *  GATE_OPENING       |    ON     |   OFF
 *  GATE_REVERSING     |    ON     |   OFF     (obstacle response)
 *  GATE_CLOSING       |    OFF    |   ON
 *  GATE_IDLE_OPEN     |    OFF    |   OFF
 *  GATE_IDLE_CLOSED   |    OFF    |   OFF
 *  GATE_STOPPED_MIDWAY|    OFF    |   OFF
 ******************************************************************************/
#include "led_control.h"
#include "gate_control.h"
#include "gpio_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define LED_POLL_PERIOD_MS  20

void LEDControlTask(void *pvParameters)
{
    (void)pvParameters;

    GateState_t current, previous = (GateState_t)0xFF;   /* force update on first run */

    for (;;)
    {
        /* Read the shared state under mutex protection */
        xSemaphoreTake(xGateStateMutex, portMAX_DELAY);
        current = g_gateState;
        xSemaphoreGive(xGateStateMutex);

        /* Only touch the LEDs when the state has actually changed */
        if (current != previous)
        {
            switch (current)
            {
            case GATE_OPENING:
            case GATE_REVERSING:
                LED_Red_Off();
                LED_Green_On();
                break;

            case GATE_CLOSING:
                LED_Green_Off();
                LED_Red_On();
                break;

            case GATE_IDLE_OPEN:
            case GATE_IDLE_CLOSED:
            case GATE_STOPPED_MIDWAY:
            default:
                LED_Red_Off();
                LED_Green_Off();
                break;
            }

            previous = current;
        }

        vTaskDelay(pdMS_TO_TICKS(LED_POLL_PERIOD_MS));
    }
}
