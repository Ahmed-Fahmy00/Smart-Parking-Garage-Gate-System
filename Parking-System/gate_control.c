/*******************************************************************************
 * gate_control.c
 * Implements the Finite State Machine for gate control.
 *
 * The task blocks on the button-event queue and processes each event
 * according to the current gate state.  All accesses to the shared state
 * variables (g_gateState, g_activeSource, g_autoMode) are protected by
 * xGateStateMutex.
 *
 * Key behaviours
 * -----------------------------------------------------------------------
 *  Manual mode      : gate moves while button is held; stops on release.
 *  One-touch auto   : a brief tap (<400 ms) keeps the gate moving until
 *                     the corresponding limit is reached.
 *  Security priority: security commands override driver commands.
 *  Conflict         : simultaneous OPEN+CLOSE on the same panel -> stop.
 *
 * Debug Output
 * -----------------------------------------------------------------------
 *  Gate state transitions are printed to serial (ADU) for real-time
 *  debugging and verification of FSM behavior.
 ******************************************************************************/
#include <stdio.h>
#include "gate_control.h"
#include "gpio_config.h"
#include "basic_io.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* ---- Shared state (owned by this file, declared extern in header) ---- */
volatile GateState_t   g_gateState    = GATE_IDLE_CLOSED;
volatile EventSource_t g_activeSource = SRC_NONE;
volatile uint8_t       g_autoMode     = 0;

/* ========================================================================== */
/*  Helpers                                                                   */
/* ========================================================================== */

/**
 * Convert gate state to human-readable string for debug output
 */
static const char * get_state_name(GateState_t state)
{
    switch (state)
    {
        case GATE_IDLE_CLOSED:      return "IDLE_CLOSED";
        case GATE_IDLE_OPEN:        return "IDLE_OPEN";
        case GATE_OPENING:          return "OPENING";
        case GATE_CLOSING:          return "CLOSING";
        case GATE_STOPPED_MIDWAY:   return "STOPPED_MIDWAY";
        case GATE_REVERSING:        return "REVERSING";
        default:                    return "UNKNOWN";
    }
}

/**
 * Convert source to human-readable string for debug output
 */
static const char * get_source_name(EventSource_t src)
{
    switch (src)
    {
        case SRC_NONE:      return "NONE";
        case SRC_DRIVER:    return "DRIVER";
        case SRC_SECURITY:  return "SECURITY";
        case SRC_LIMIT:     return "LIMIT";
        case SRC_SAFETY:    return "SAFETY";
        default:            return "UNKNOWN";
    }
}

/**
 * Print gate state transition to serial/ADU
 * Usage: print_state_transition(oldState, newState)
 */
static void print_state_transition(GateState_t oldState, GateState_t newState)
{
    extern volatile uint8_t gParkingOccupancy;
    #define MAX_PARKING_PLACES  5
    
    char msg[80];
    sprintf(msg, "[Gate] %s -> %s | Parking: %d/%d",
            get_state_name(oldState),
            get_state_name(newState),
            gParkingOccupancy,
            MAX_PARKING_PLACES);
    vPrintString(msg);
    vPrintString("\r\n");
}

/**
 * Returns 1 if @p newSrc is allowed to override the current active source.
 *   - Anyone may act when g_activeSource == SRC_NONE  (gate idle/stopped).
 *   - Security always overrides Driver.
 *   - Same source may update its own command.
 */
static uint8_t source_has_priority(EventSource_t newSrc)
{
    if (g_activeSource == SRC_NONE)                       return 1;
    if (newSrc == SRC_SECURITY && g_activeSource == SRC_DRIVER) return 1;
    if (newSrc == g_activeSource)                          return 1;
    return 0;
}

/* ========================================================================== */
/*  Gate Control Task                                                         */
/* ========================================================================== */

void GateControlTask(void *pvParameters)
{
    GateEvent_t ev;
    (void)pvParameters;

    for (;;)
    {
        /* Block until an event arrives on the queue */
        if (xQueueReceive(xButtonEventQueue, &ev, portMAX_DELAY) != pdTRUE)
            continue;

        /* ---------- Lock shared state ---------- */
        xSemaphoreTake(xGateStateMutex, portMAX_DELAY);

        GateState_t s = g_gateState;

        switch (ev.type)
        {
        /* ============================================================== */
        /*  OPEN button pressed                                           */
        /* ============================================================== */
        case EV_OPEN_PRESS:
            if (!source_has_priority(ev.source))
                break;

            extern volatile uint8_t gParkingOccupancy;
            #define MAX_PARKING_PLACES  5
            if ((s == GATE_IDLE_CLOSED) && (gParkingOccupancy >= MAX_PARKING_PLACES))
            {
                vPrintString("ENTRY FAILED: Parking is FULL - no spaces available\r\n");
                break;
            }

            if (s == GATE_IDLE_CLOSED  ||
                s == GATE_STOPPED_MIDWAY)
            {
                print_state_transition(s, GATE_OPENING);
                g_gateState    = GATE_OPENING;
                g_activeSource = ev.source;
                g_autoMode     = 0;             /* assume manual until release */
            }
            else if (s == GATE_CLOSING)
            {
                /* Reverse direction */
                print_state_transition(s, GATE_OPENING);
                g_gateState    = GATE_OPENING;
                g_activeSource = ev.source;
                g_autoMode     = 0;
            }
            /* IDLE_OPEN / OPENING / REVERSING -> ignore */
            break;

        /* ============================================================== */
        /*  OPEN button released                                          */
        /* ============================================================== */
        case EV_OPEN_RELEASE:
            if (ev.source != g_activeSource)
                break;                          /* not the controlling source */

            if (s == GATE_OPENING)
            {
                if (ev.duration_ms < MANUAL_HOLD_THRESHOLD_MS)
                {
                    /* Brief tap -> one-touch auto: keep moving to limit */
                    g_autoMode = 1;
                }
                else
                {
                    /* Manual hold -> stop gate */
                    print_state_transition(s, GATE_STOPPED_MIDWAY);
                    g_gateState    = GATE_STOPPED_MIDWAY;
                    g_activeSource = SRC_NONE;
                    g_autoMode     = 0;
                }
            }
            break;

        /* ============================================================== */
        /*  CLOSE button pressed                                          */
        /* ============================================================== */
        case EV_CLOSE_PRESS:
            if (!source_has_priority(ev.source))
                break;

            if (s == GATE_IDLE_OPEN    ||
                s == GATE_STOPPED_MIDWAY)
            {
                print_state_transition(s, GATE_CLOSING);
                g_gateState    = GATE_CLOSING;
                g_activeSource = ev.source;
                g_autoMode     = 0;
            }
            else if (s == GATE_OPENING)
            {
                /* Reverse direction */
                print_state_transition(s, GATE_CLOSING);
                g_gateState    = GATE_CLOSING;
                g_activeSource = ev.source;
                g_autoMode     = 0;
            }
            /* IDLE_CLOSED / CLOSING / REVERSING -> ignore */
            break;

        /* ============================================================== */
        /*  CLOSE button released                                         */
        /* ============================================================== */
        case EV_CLOSE_RELEASE:
            if (ev.source != g_activeSource)
                break;

            if (s == GATE_CLOSING)
            {
                if (ev.duration_ms < MANUAL_HOLD_THRESHOLD_MS)
                {
                    g_autoMode = 1;
                }
                else
                {
                    print_state_transition(s, GATE_STOPPED_MIDWAY);
                    g_gateState    = GATE_STOPPED_MIDWAY;
                    g_activeSource = SRC_NONE;
                    g_autoMode     = 0;
                }
            }
            break;

        /* ============================================================== */
        /*  Limit buttons                                                 */
        /* ============================================================== */
        case EV_OPEN_LIMIT:
            if (s == GATE_OPENING)
            {
                print_state_transition(s, GATE_IDLE_OPEN);
                g_gateState    = GATE_IDLE_OPEN;
                g_activeSource = SRC_NONE;
                g_autoMode     = 0;
                extern volatile uint8_t gParkingOccupancy;
                gParkingOccupancy++;
                vPrintString("ENTRY SUCCESSFUL: Vehicle entered parking\r\n");
            }
            /* TC-12: wrong limit (e.g. Open Limit while closing) -> ignore */
            break;

        case EV_CLOSED_LIMIT:
            if (s == GATE_CLOSING)
            {
                print_state_transition(s, GATE_IDLE_CLOSED);
                g_gateState    = GATE_IDLE_CLOSED;
                g_activeSource = SRC_NONE;
                g_autoMode     = 0;
                extern volatile uint8_t gParkingOccupancy;
                if (gParkingOccupancy > 0) gParkingOccupancy--;
                vPrintString("EXIT SUCCESSFUL: Vehicle left parking\r\n");
            }
            break;

        /* ============================================================== */
        /*  Obstacle (sent by Safety task via xQueueSendToFront)          */
        /* ============================================================== */
        case EV_OBSTACLE:
            /*  Obstacle handling is performed entirely by the SafetyTask
             *  (which holds the mutex during the reverse manoeuvre).
             *  If an EV_OBSTACLE leaks through here, ignore it.          */
            break;

        /* ============================================================== */
        /*  Conflicting inputs on the same panel (OPEN + CLOSE)           */
        /* ============================================================== */
        case EV_CONFLICT_STOP:
            if (s == GATE_OPENING || s == GATE_CLOSING)
            {
                print_state_transition(s, GATE_STOPPED_MIDWAY);
                g_gateState    = GATE_STOPPED_MIDWAY;
                g_activeSource = SRC_NONE;
                g_autoMode     = 0;
            }
            /* If already stopped or idle, do nothing */
            break;

        default:
            break;
        }

        /* ---------- Unlock shared state ---------- */
        xSemaphoreGive(xGateStateMutex);
    }
}
