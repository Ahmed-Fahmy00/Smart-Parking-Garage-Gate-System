/*******************************************************************************
 * test_harness.h
 * Automated test harness for Smart Parking Garage Gate System
 *
 * Provides test case definitions and assertion utilities for verifying all
 * functional requirements and RTOS mechanisms.
 ******************************************************************************/
#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdint.h>
#include "gate_control.h"

/*
 * Build switch for MDK size-limited builds.
 * 0 = compile lightweight stubs only (default for MDK Lite)
 * 1 = compile full automated test suite (requires licensed Keil or larger target)
 */
#ifndef ENABLE_TEST_HARNESS
#define ENABLE_TEST_HARNESS 0
#endif

/* ========================================================================== */
/*  Test Case Definitions                                                     */
/* ========================================================================== */

typedef enum {
    TC_MANUAL_OPEN,              /* TC-01 */
    TC_MANUAL_CLOSE,             /* TC-02 */
    TC_MANUAL_FROM_MIDWAY,       /* TC-03 */
    TC_AUTO_OPEN,                /* TC-04 */
    TC_AUTO_CLOSE,               /* TC-05 */
    TC_AUTO_FROM_MIDWAY,         /* TC-06 */
    TC_OBSTACLE_REVERSE,         /* TC-07 */
    TC_OBSTACLE_OVERRIDE,        /* TC-08 */
    TC_OBSTACLE_IGNORED_OPENING, /* TC-09 */
    TC_OPEN_LIMIT,               /* TC-10 */
    TC_CLOSED_LIMIT,             /* TC-11 */
    TC_WRONG_LIMIT,              /* TC-12 */
    TC_SECURITY_PRIORITY_OPEN,   /* TC-13 */
    TC_SECURITY_PRIORITY_CLOSE,  /* TC-14 */
    TC_CONFLICT_DRIVER,          /* TC-15 */
    TC_CONFLICT_SECURITY,        /* TC-16 */
    TC_QUEUE_ROBUSTNESS,         /* TC-19 */
    TC_MUTEX_PROTECTION,         /* TC-20 */
    TC_SEMAPHORE_SIGNAL,         /* TC-21 */
    NUM_TEST_CASES
} TestCaseID_t;

/* ========================================================================== */
/*  Test Result Structure                                                     */
/* ========================================================================== */

typedef struct {
    TestCaseID_t id;
    const char *name;
    uint8_t passed;
    char message[128];
} TestResult_t;

/* ========================================================================== */
/*  Test Control Flags                                                        */
/* ========================================================================== */

extern volatile uint8_t gEnableTestMode;
extern volatile uint8_t gTestInProgress;
extern volatile TestResult_t gTestResults[NUM_TEST_CASES];

/* ========================================================================== */
/*  Test Entry Point                                                         */
/* ========================================================================== */

void TestTask(void *pvParameters);

#endif /* TEST_HARNESS_H */
