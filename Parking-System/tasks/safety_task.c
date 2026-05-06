/*******************************************************************************
 * safety_task.c
 * Highest-priority task that monitors the obstacle semaphore.
 *
 * When the obstacle semaphore is given (by the Input task on obstacle
 * button press) and the gate is currently CLOSING:
 *
 *   1.  Immediately change state to GATE_REVERSING  (Green LED turns ON
 *       via the LED Control task).
 *   2.  Hold the REVERSING state for REVERSE_DURATION_MS (500 ms).
 *   3.  Transition to GATE_STOPPED_MIDWAY           (both LEDs OFF).
 *
 * If the gate is NOT closing when the semaphore is given, the obstacle
 * signal is ignored (per TC-09).
 *
 * Because the Safety task has the highest priority, it pre-empts all
 * other tasks, ensuring the obstacle response is immediate.  The shared
 * state is protected with xGateStateMutex, which is released during the
 * 500 ms delay so that lower-priority tasks (e.g. LED Control) can read
 * the state and update the LEDs.
 *
 * Debug Output
 * -----------------------------------------------------------------------
 *  Obstacle detection and state transitions are printed to serial (ADU).
 ******************************************************************************/
#include "safety_task.h"
#include "gate_control.h"
#include "gpio_config.h"
#include "basic_io.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

void SafetyTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        /* Block until the Input task signals an obstacle */
        if (xSemaphoreTake(xObstacleSemaphore, portMAX_DELAY) != pdTRUE)
            continue;

        /* ---- Acquire state mutex ---- */
        xSemaphoreTake(xGateStateMutex, portMAX_DELAY);

        if (g_gateState == GATE_CLOSING)
        {
            /* ---- Step 1: stop and reverse ---- */
            g_gateState    = GATE_REVERSING;
            g_activeSource = SRC_SAFETY;
            g_autoMode     = 0;
            
            vPrintString("Obstacle detected: CLOSING -> REVERSING\r\n");

            /* Release mutex so LED task can see REVERSING and turn on
               the Green LED while we wait. */
            xSemaphoreGive(xGateStateMutex);

            /* ---- Step 2: hold reverse for 500 ms ---- */
            vTaskDelay(pdMS_TO_TICKS(REVERSE_DURATION_MS));

            /* ---- Step 3: stop completely ---- */
            xSemaphoreTake(xGateStateMutex, portMAX_DELAY);

            /* Only finalise if no other high-priority event changed the
               state while we were delaying (defensive). */
            if (g_gateState == GATE_REVERSING)
            {
                g_gateState    = GATE_STOPPED_MIDWAY;
                g_activeSource = SRC_NONE;
                vPrintString("Obstacle cleared: REVERSING -> STOPPED_MIDWAY\r\n");
            }

            xSemaphoreGive(xGateStateMutex);
        }
        else
        {
            xSemaphoreGive(xGateStateMutex);
        }
    }
}
