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
volatile uint8_t       ManualMode     = 1;

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

static const char * get_source_name(EventSource_t src)
{
    switch (src)
    {
        case SRC_DRIVER:    return "DRIVER";
        case SRC_SECURITY:  return "SECURITY";
        default:            return "UNKNOWN";
    }
}

/**
 * Print gate state transition to serial/ADU
 */
static void print_state_transition(GateState_t oldState, GateState_t newState)
{
    char msg[80];
    sprintf(msg, "[Gate] %s -> %s",
            get_state_name(oldState),
            get_state_name(newState));
    vPrintString(msg);
    vPrintString("\r\n");
}

/**
 * Helper: reset gate state to a new state with given source
 */
static void set_gate_state(GateState_t newState, EventSource_t newSource, uint8_t clearAuto)
{
    g_gateState    = newState;
    g_activeSource = newSource;
    g_autoMode     = clearAuto ? 0 : g_autoMode;
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
        {
            char btn_msg[64];
            sprintf(btn_msg, "[Button] %s OPEN pressed", get_source_name(ev.source));
            vPrintString(btn_msg);
            vPrintString("\r\n");

            if (!source_has_priority(ev.source))
            {
                char pri_msg[80];
                sprintf(pri_msg, "  [Priority] %s overrides %s",
                        get_source_name(g_activeSource),
                        get_source_name(ev.source));
                vPrintString(pri_msg);
                vPrintString("\r\n");
                break;
            }

            if (s == GATE_IDLE_CLOSED  ||
                s == GATE_STOPPED_MIDWAY  ||
                s == GATE_CLOSING)
            {
                print_state_transition(s, GATE_OPENING);
                set_gate_state(GATE_OPENING, ev.source, 1);
            }
            break;
        }

        /* ============================================================== */
        /*  OPEN button released                                          */
        /* ============================================================== */
        case EV_OPEN_RELEASE:
        {
            if (ev.source != g_activeSource)
                break;                          /* not the controlling source */

            if (s == GATE_OPENING)
            {
                if (ManualMode)
                {
                    /* Manual mode: stop as soon as the button is released. */
                    print_state_transition(s, GATE_STOPPED_MIDWAY);
                    set_gate_state(GATE_STOPPED_MIDWAY, SRC_NONE, 1);
                }
                else
                {
                    if (ev.duration_ms < MANUAL_HOLD_THRESHOLD_MS)
                    {
                        /* Auto mode: brief tap continues automatically. */
                        g_autoMode = 1;
                    }
                    else
                    {
                        /* Auto mode with long hold: stop on release. */
                        print_state_transition(s, GATE_STOPPED_MIDWAY);
                        set_gate_state(GATE_STOPPED_MIDWAY, SRC_NONE, 1);
                    }
                }
            }
            break;
        }
        /* ============================================================== */
        /*  CLOSE button pressed                                          */
        /* ============================================================== */
        case EV_CLOSE_PRESS:
        {
            char btn_msg[64];
            sprintf(btn_msg, "[Button] %s CLOSE pressed", get_source_name(ev.source));
            vPrintString(btn_msg);
            vPrintString("\r\n");

            if (!source_has_priority(ev.source))
            {
                char pri_msg[80];
                sprintf(pri_msg, "  [Priority] %s overrides %s",
                        get_source_name(g_activeSource),
                        get_source_name(ev.source));
                vPrintString(pri_msg);
                vPrintString("\r\n");
                break;
            }

            if (s == GATE_IDLE_OPEN  ||
                s == GATE_STOPPED_MIDWAY  ||
                s == GATE_OPENING)
            {
                print_state_transition(s, GATE_CLOSING);
                set_gate_state(GATE_CLOSING, ev.source, 1);
            }
            break;
        }

        /* ============================================================== */
        /*  CLOSE button released                                         */
        /* ============================================================== */
        case EV_CLOSE_RELEASE:
        {
            if (ev.source != g_activeSource)
                break;

            if (s == GATE_CLOSING)
            {
                if (ManualMode)
                {
                    /* Manual mode: stop as soon as the button is released. */
                    print_state_transition(s, GATE_STOPPED_MIDWAY);
                    set_gate_state(GATE_STOPPED_MIDWAY, SRC_NONE, 1);
                }
                else
                {
                    if (ev.duration_ms < MANUAL_HOLD_THRESHOLD_MS)
                    {
                        /* Auto mode: brief tap continues automatically. */
                        g_autoMode = 1;
                    }
                    else
                    {
                        /* Auto mode with long hold: stop on release. */
                        print_state_transition(s, GATE_STOPPED_MIDWAY);
                        set_gate_state(GATE_STOPPED_MIDWAY, SRC_NONE, 1);
                    }
                }
            }
            break;
        }

        /* ============================================================== */
        /*  Limit buttons                                                 */
        /* ============================================================== */
        case EV_OPEN_LIMIT:
            if (s == GATE_OPENING)
            {
                print_state_transition(s, GATE_IDLE_OPEN);
                set_gate_state(GATE_IDLE_OPEN, SRC_NONE, 1);
            }
            break;

        case EV_CLOSED_LIMIT:
            if (s == GATE_CLOSING)
            {
                print_state_transition(s, GATE_IDLE_CLOSED);
                set_gate_state(GATE_IDLE_CLOSED, SRC_NONE, 1);
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
                set_gate_state(GATE_STOPPED_MIDWAY, SRC_NONE, 1);
            }
            break;

        default:
            break;
        }

        /* ---------- Unlock shared state ---------- */
        xSemaphoreGive(xGateStateMutex);
    }
}
