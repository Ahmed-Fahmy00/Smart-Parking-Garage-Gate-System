/*******************************************************************************
 * input_task.c
 * High-priority task that polls all seven buttons every 10 ms, debounces
 * them, and produces events for the Gate Control task.
 *
 * Debounce strategy
 * -----------------
 * A change in the raw reading resets a per-button counter.  Once the raw
 * reading has been stable for DEBOUNCE_CYCLES consecutive polls the new
 * state is accepted.  With a 10 ms poll period and DEBOUNCE_CYCLES = 2 the
 * effective debounce window is 20 ms.
 *
 * Conflict detection
 * ------------------
 * If both OPEN and CLOSE on the *same* panel are stable-pressed at the
 * same time, an EV_CONFLICT_STOP event is sent (once) and individual
 * events are suppressed until the conflict clears.
 *
 * Manual vs. One-Touch
 * --------------------
 * On each button release the press duration is included in the event so
 * that the Gate Control task can decide manual vs. auto mode.
 *
 * Debug Output
 * ------------------
 ******************************************************************************/
#include "input_task.h"
#include "gate_control.h"
#include "gpio_config.h"
#include "basic_io.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* ---- Tuning ---- */
#define POLL_PERIOD_MS      10
#define DEBOUNCE_CYCLES     2       /* x POLL_PERIOD_MS = 20 ms */

/* ---- Internal button indices ---- */
enum {
    B_DRV_OPEN = 0,
    B_DRV_CLOSE,
    B_SEC_OPEN,
    B_SEC_CLOSE,
    B_OPEN_LIM,
    B_CLOSED_LIM,
    B_OBSTACLE,
    B_SW2,
    NUM_BTNS
};

/* Function-pointer table for reading each button */
typedef uint8_t (*BtnReader_t)(void);

static const BtnReader_t btn_read[NUM_BTNS] = {
    Btn_DriverOpen_Read,
    Btn_DriverClose_Read,
    Btn_SecurityOpen_Read,
    Btn_SecurityClose_Read,
    Btn_OpenLimit_Read,
    Btn_ClosedLimit_Read,
    Btn_Obstacle_Read,
    Btn_SW2_Read
};

/* ---- Local helpers ---- */
static void send_event(EventType_t type, EventSource_t src, uint32_t dur)
{
    GateEvent_t ev;
    ev.type        = type;
    ev.source      = src;
    ev.duration_ms = dur;
    if (xQueueSend(xButtonEventQueue, &ev, pdMS_TO_TICKS(10)) != pdTRUE)
    {
        /* Queue full - event may be dropped */
    }
}

static void send_release_event(uint8_t buttonIdx,
                               EventType_t releaseType,
                               EventSource_t source,
                               TickType_t now,
                               const TickType_t *press_tick)
{
    uint32_t dur = (uint32_t)(now - press_tick[buttonIdx]) * portTICK_PERIOD_MS;
    send_event(releaseType, source, dur);
}

static void send_limit_event(EventType_t type, const char *msg)
{
    GateEvent_t lev = { .type = type, .source = SRC_LIMIT, .duration_ms = 0 };
    vPrintString(msg);
    /* Send limit event to the front so GateControl sees it immediately */
    xQueueSendToFront(xButtonEventQueue, &lev, pdMS_TO_TICKS(10));
    xSemaphoreGive(xLimitSemaphore);
}

/* ========================================================================== */
/*  Input Task                                                                */
/* ========================================================================== */
void InputTask(void *pvParameters)
{
    (void)pvParameters;

    /* Per-button state */
    uint8_t    raw_prev[NUM_BTNS];
    uint8_t    stable[NUM_BTNS];
    uint8_t    deb_cnt[NUM_BTNS];
    TickType_t press_tick[NUM_BTNS];

    /* Conflict flags (one-shot: send conflict event only once) */
    uint8_t driver_conflict   = 0;
    uint8_t security_conflict = 0;

    /* Initialise everything to "released" */
    uint8_t i;
    for (i = 0; i < NUM_BTNS; i++)
    {
        raw_prev[i]   = BTN_RELEASED;
        stable[i]     = BTN_RELEASED;
        deb_cnt[i]    = 0;
        press_tick[i] = 0;
    }

    TickType_t xLastWake = xTaskGetTickCount();

    for (;;)
    {
        TickType_t now = xTaskGetTickCount();

        /* ---- Flags set during debounce ---- */
        uint8_t ev_pressed[NUM_BTNS]  = {0};
        uint8_t ev_released[NUM_BTNS] = {0};

        /* ============================================================== */
        /*  1. Read & debounce every button                               */
        /* ============================================================== */
        for (i = 0; i < NUM_BTNS; i++)
        {
            uint8_t raw = btn_read[i]();

            if (raw != raw_prev[i])
            {
                deb_cnt[i] = 0;                     /* reset on change      */
            }
            else
            {
                if (deb_cnt[i] < DEBOUNCE_CYCLES)
                {
                    deb_cnt[i]++;
                    if (deb_cnt[i] == DEBOUNCE_CYCLES && raw != stable[i])
                    {
                        /* State has stabilised to a new value */
                        stable[i] = raw;
                        if (raw == BTN_PRESSED)
                        {
                            ev_pressed[i] = 1;
                            press_tick[i] = now;
                        }
                        else
                        {
                            ev_released[i] = 1;
                        }
                    }
                }
            }
            raw_prev[i] = raw;
        }

        /* ============================================================== */
        /*  2. Conflict detection (same-panel OPEN+CLOSE)                 */
        /* ============================================================== */

        /* --- Driver panel --- */
        if (stable[B_DRV_OPEN] == BTN_PRESSED &&
            stable[B_DRV_CLOSE] == BTN_PRESSED)
        {
            if (!driver_conflict)
            {
                send_event(EV_CONFLICT_STOP, SRC_DRIVER, 0);
                driver_conflict = 1;
            }
            /* Suppress individual press/release while conflict is active */
            ev_pressed[B_DRV_OPEN]   = 0;
            ev_pressed[B_DRV_CLOSE]  = 0;
            ev_released[B_DRV_OPEN]  = 0;
            ev_released[B_DRV_CLOSE] = 0;
        }
        else
        {
            driver_conflict = 0;
        }

        /* --- Security panel --- */
        if (stable[B_SEC_OPEN] == BTN_PRESSED &&
            stable[B_SEC_CLOSE] == BTN_PRESSED)
        {
            if (!security_conflict)
            {
                send_event(EV_CONFLICT_STOP, SRC_SECURITY, 0);
                security_conflict = 1;
            }
            ev_pressed[B_SEC_OPEN]   = 0;
            ev_pressed[B_SEC_CLOSE]  = 0;
            ev_released[B_SEC_OPEN]  = 0;
            ev_released[B_SEC_CLOSE] = 0;
        }
        else
        {
            security_conflict = 0;
        }

        /* ============================================================== */
        /*  3. Generate events for direction buttons                      */
        /* ============================================================== */

        /* -- Driver OPEN -- */
        if (ev_pressed[B_DRV_OPEN])
            send_event(EV_OPEN_PRESS, SRC_DRIVER, 0);
        if (ev_released[B_DRV_OPEN])
            send_release_event(B_DRV_OPEN, EV_OPEN_RELEASE, SRC_DRIVER, now, press_tick);

        /* -- Driver CLOSE -- */
        if (ev_pressed[B_DRV_CLOSE])
            send_event(EV_CLOSE_PRESS, SRC_DRIVER, 0);
        if (ev_released[B_DRV_CLOSE])
            send_release_event(B_DRV_CLOSE, EV_CLOSE_RELEASE, SRC_DRIVER, now, press_tick);

        /* -- Security OPEN -- */
        if (ev_pressed[B_SEC_OPEN])
            send_event(EV_OPEN_PRESS, SRC_SECURITY, 0);
        if (ev_released[B_SEC_OPEN])
            send_release_event(B_SEC_OPEN, EV_OPEN_RELEASE, SRC_SECURITY, now, press_tick);

        /* -- Security CLOSE -- */
        if (ev_pressed[B_SEC_CLOSE])
            send_event(EV_CLOSE_PRESS, SRC_SECURITY, 0);
        if (ev_released[B_SEC_CLOSE])
            send_release_event(B_SEC_CLOSE, EV_CLOSE_RELEASE, SRC_SECURITY, now, press_tick);

        /* ============================================================== */
        /*  4. Limit buttons  →  queue event + semaphore signal           */
        /* ============================================================== */
        if (ev_pressed[B_OPEN_LIM])
            send_limit_event(EV_OPEN_LIMIT, "[Button] Open Limit pressed\r\n");
        if (ev_pressed[B_CLOSED_LIM])
            send_limit_event(EV_CLOSED_LIMIT, "[Button] Closed Limit pressed\r\n");

        /* ============================================================== */
        /*  5. Obstacle button  →  semaphore (consumed by Safety task)    */
        /* ============================================================== */
        if (ev_pressed[B_OBSTACLE])
        {
            xSemaphoreGive(xObstacleSemaphore);
        }

        /* ---- SW2: toggle Manual/Auto mode on press ---- */
        if (ev_pressed[B_SW2])
        {
            ManualMode = !ManualMode;
            if (ManualMode)
                vPrintString("Mode toggled: MANUAL\r\n");
            else
                vPrintString("Mode toggled: AUTO\r\n");
        }

        /* ---- Fixed-rate delay ---- */
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(POLL_PERIOD_MS));
    }
}
