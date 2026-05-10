/*******************************************************************************
 * main.c
 * Smart Parking Garage Gate System  –  FreeRTOS on TM4C123GH6PM
 *
 * This file:
 *   1. Initialises the GPIO hardware.
 *   2. Creates the RTOS inter-task communication objects:
 *        - xButtonEventQueue   (Queue  – 50 deep, prevents event loss)
 *        - xLimitSemaphore     (Binary Semaphore, signals limit activation)
 *        - xObstacleSemaphore  (Binary Semaphore, signals obstacle)
 *        - xGateStateMutex     (Mutex, protects shared state)
 *   3. Creates all application tasks with correct priorities:
 *        Task              Priority   Purpose
 *        ──────────────────────────────────────────────
 *        SafetyTask          5        Obstacle response (highest)
 *        InputTask           4        Button polling & debouncing
 *        GateControlTask     3        FSM / gate logic
 *        LEDControlTask      3        LED output
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

     /* Queue: carries GateEvent_t structs from Input -> GateControl
         TC-19: Depth increased to 50 to prevent event loss under rapid input */
     xButtonEventQueue = xQueueCreate(50, sizeof(GateEvent_t));
     if (!xButtonEventQueue)
     {
          vPrintString("[Init] xButtonEventQueue creation failed\r\n");
          while (1);
     }

     /* Binary semaphore: Input task gives on limit-button press;
         GateControlTask consumes it to stop gate immediately (TC-21)       */
     xLimitSemaphore = xSemaphoreCreateBinary();
     if (!xLimitSemaphore)
     {
          vPrintString("[Init] xLimitSemaphore creation failed\r\n");
          while (1);
     }

     /* Binary semaphore: Input task gives on obstacle-button press;
         SafetyTask takes to trigger the reverse manoeuvre.                  */
     xObstacleSemaphore = xSemaphoreCreateBinary();
     if (!xObstacleSemaphore)
     {
          vPrintString("[Init] xObstacleSemaphore creation failed\r\n");
          while (1);
     }

     /* Mutex: protects g_gateState / g_activeSource / g_autoMode across
         GateControl, Safety, and LED tasks.                                 */
     xGateStateMutex = xSemaphoreCreateMutex();
     if (!xGateStateMutex)
     {
          vPrintString("[Init] xGateStateMutex creation failed\r\n");
          while (1);
     }

    /* ---- 3. Create application tasks ---- */
    xTaskCreate(SafetyTask,       "Safety",   256, NULL,  5,  NULL);
    xTaskCreate(InputTask,        "Input",    256, NULL,  4,  NULL);
    xTaskCreate(GateControlTask,  "GateCtrl", 256, NULL,  3,  NULL);
    xTaskCreate(LEDControlTask,   "LEDCtrl",  128, NULL,  3,  NULL);

    /* ---- 5. Print startup status ---- */
    vPrintString("========================================\r\n");
    vPrintString("PARKING SYSTEM STARTED\r\n");
    vPrintString("Gate Status: IDLE_CLOSED\r\n");
    vPrintString("========================================\r\n");

    /* ---- 6. Start the scheduler (never returns) ---- */
    vTaskStartScheduler();

    /* Should never reach here */
    while (1);
}
