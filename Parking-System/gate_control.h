/*******************************************************************************
 * gate_control.h
 * Gate state machine types, shared state, and RTOS handle declarations.
 ******************************************************************************/
#ifndef GATE_CONTROL_H
#define GATE_CONTROL_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

/* ========================================================================== */
/*  Gate State Machine                                                        */
/* ========================================================================== */
typedef enum {
    GATE_IDLE_CLOSED,       /* Gate fully closed, both LEDs OFF          */
    GATE_IDLE_OPEN,         /* Gate fully open,  both LEDs OFF           */
    GATE_OPENING,           /* Gate moving UP,   Green LED ON            */
    GATE_CLOSING,           /* Gate moving DOWN, Red   LED ON            */
    GATE_STOPPED_MIDWAY,    /* Gate stopped between limits, LEDs OFF     */
    GATE_REVERSING          /* Obstacle response: reversing, Green ON    */
} GateState_t;

/* ========================================================================== */
/*  Event definitions (passed through the button-event queue)                 */
/* ========================================================================== */
typedef enum {
    SRC_NONE = 0,
    SRC_DRIVER,
    SRC_SECURITY,
    SRC_LIMIT,
    SRC_SAFETY
} EventSource_t;

typedef enum {
    EV_OPEN_PRESS,          /* OPEN button pressed                       */
    EV_OPEN_RELEASE,        /* OPEN button released                      */
    EV_CLOSE_PRESS,         /* CLOSE button pressed                      */
    EV_CLOSE_RELEASE,       /* CLOSE button released                     */
    EV_OPEN_LIMIT,          /* Gate reached fully-open limit             */
    EV_CLOSED_LIMIT,        /* Gate reached fully-closed limit           */
    EV_OBSTACLE,            /* Obstacle detected (from Safety task)      */
    EV_CONFLICT_STOP        /* Conflicting buttons on same panel         */
} EventType_t;

typedef struct {
    EventType_t   type;
    EventSource_t source;
    uint32_t      duration_ms;  /* For *_RELEASE events: press duration  */
} GateEvent_t;

/* ========================================================================== */
/*  Timing constants                                                          */
/* ========================================================================== */

/* Gate operating mode selector:
 *   1 = Manual mode (stop on button release)
 *   0 = Auto mode   (short tap continues automatically)
 */
extern volatile uint8_t ManualMode;

/* If a button is released within this window it counts as a "tap" and
   triggers one-touch-auto mode.  A longer hold is manual mode.             */
#define MANUAL_HOLD_THRESHOLD_MS    400

/* Duration the gate reverses (opens) after an obstacle is detected.        */
#define REVERSE_DURATION_MS         500

/* ========================================================================== */
/*  Global RTOS handles  (created in main.c)                                 */
/* ========================================================================== */
extern QueueHandle_t       xButtonEventQueue;   /* 20-deep event queue      */
extern SemaphoreHandle_t   xLimitSemaphore;     /* signals limit activation */
extern SemaphoreHandle_t   xObstacleSemaphore;  /* signals obstacle detect  */
extern SemaphoreHandle_t   xGateStateMutex;     /* protects shared state    */

/* ========================================================================== */
/*  Shared state variables  (protected by xGateStateMutex)                   */
/* ========================================================================== */
extern volatile GateState_t   g_gateState;
extern volatile EventSource_t g_activeSource;   /* who started the move     */
extern volatile uint8_t       g_autoMode;       /* 1 = one-touch auto       */

/* ========================================================================== */
/*  Task entry point                                                         */
/* ========================================================================== */
void GateControlTask(void *pvParameters);

#endif /* GATE_CONTROL_H */
