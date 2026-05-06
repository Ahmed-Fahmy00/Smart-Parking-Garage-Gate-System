/*******************************************************************************
 * main.c
 * Smart Parking Garage Gate System  –  FreeRTOS on TM4C123GH6PM
 *
 * This file:
 *   1. Initialises the GPIO hardware.
 *   2. Creates the RTOS inter-task communication objects:
 *        - xButtonEventQueue   (Queue  – 20 deep)
 *        - xLimitSemaphore     (Binary Semaphore)
 *        - xObstacleSemaphore  (Binary Semaphore)
 *        - xGateStateMutex     (Mutex)
 *   3. Creates all five application tasks with correct priorities:
 *        Task              Priority   Purpose
 *        ──────────────────────────────────────────────
 *        SafetyTask          5        Obstacle response (highest)
 *        InputTask           4        Button polling & debouncing
 *        GateControlTask     3        FSM / gate logic
 *        LEDControlTask      3        LED output
 *        StatusTask          1        Limit-LED feedback (lowest)
 *   4. Starts the FreeRTOS scheduler.
 ******************************************************************************/
#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "gpio_config.h"
#include "gate_control.h"
#include "input_task.h"
#include "led_control.h"
#include "safety_task.h"
#include "status_task.h"
#include "basic_io.h"

/* ========================================================================== */
/*  Global RTOS handles  (declared extern in gate_control.h)                 */
/* ========================================================================== */
QueueHandle_t       xButtonEventQueue  = NULL;
SemaphoreHandle_t   xLimitSemaphore    = NULL;
SemaphoreHandle_t   xObstacleSemaphore = NULL;
SemaphoreHandle_t   xGateStateMutex    = NULL;

/* ========================================================================== */
/*  Debug button simulation flags                                             */
/* ========================================================================== */
volatile uint8_t gSimDriverOpen   = 0;
volatile uint8_t gSimDriverClose  = 0;
volatile uint8_t gSimSecurityOpen = 0;
volatile uint8_t gSimSecurityClose = 0;
volatile uint8_t gSimOpenLimit    = 0;
volatile uint8_t gSimClosedLimit  = 0;
volatile uint8_t gSimObstacle     = 0;

/* ========================================================================== */
/*  FreeRTOS hook functions                                                   */
/* ========================================================================== */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    /* Halt – turn on all LEDs as a distress signal */
    taskDISABLE_INTERRUPTS();
    for (;;);
}

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;);
}

/* ========================================================================== */
/*  main                                                                      */
/* ========================================================================== */

int main(void)
{
    /* 1 = Manual (stop on release), 0 = Auto (short tap continues) */
    ManualMode = 0;

    /* ---- 1. Hardware init ---- */
    GPIO_Init();
    
    /* ---- 2. Create RTOS communication objects ---- */

    /* Queue: carries GateEvent_t structs from Input → GateControl          */
    xButtonEventQueue = xQueueCreate(20, sizeof(GateEvent_t));

    /* Binary semaphore: Input task gives on limit-button press;
       StatusTask takes to flash the Blue LED.                              */
    xLimitSemaphore = xSemaphoreCreateBinary();

    /* Binary semaphore: Input task gives on obstacle-button press;
       SafetyTask takes to trigger the reverse manoeuvre.                   */
    xObstacleSemaphore = xSemaphoreCreateBinary();

    /* Mutex: protects g_gateState / g_activeSource / g_autoMode across
       GateControl, Safety, and LED tasks.                                  */
    xGateStateMutex = xSemaphoreCreateMutex();

    /* Sanity check – halt if any object failed to create */
    if (!xButtonEventQueue || !xLimitSemaphore || !xObstacleSemaphore || !xGateStateMutex)
    {
        while (1);
    }

    /* ---- 3. Create application tasks ---- */
    xTaskCreate(SafetyTask,       "Safety",   256, NULL,  5,  NULL);
    xTaskCreate(InputTask,        "Input",    256, NULL,  4,  NULL);
    xTaskCreate(GateControlTask,  "GateCtrl", 256, NULL,  3,  NULL);
    xTaskCreate(LEDControlTask,   "LEDCtrl",  128, NULL,  3,  NULL);
    xTaskCreate(StatusTask,       "Status",   128, NULL,  1,  NULL);

    /* ---- 5. Print startup status ---- */
    vPrintString("========================================\r\n");
    vPrintString("PARKING SYSTEM STARTED\r\n");
    if (ManualMode)
        vPrintString("Mode: MANUAL\r\n");
    else
        vPrintString("Mode: AUTO\r\n");
    vPrintString("Gate Status: IDLE_CLOSED\r\n");
    vPrintString("========================================\r\n");

    /* ---- 6. Start the scheduler (never returns) ---- */
    vTaskStartScheduler();

    /* Should never reach here */
    while (1);
}
