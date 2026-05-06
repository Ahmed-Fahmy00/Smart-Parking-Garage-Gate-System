# Smart Parking Garage Gate System

**Course:** CSE411: Real-Time and Embedded Systems Design / CSE323: Advanced Embedded Systems Design  
**Semester:** Spring 2026  
**Project Type:** Real-time embedded system using FreeRTOS

---

## 1. Project Overview

This project implements a Smart Parking Garage Gate System using the Tiva-C **TM4C123GH6PM** microcontroller running **FreeRTOS**. The system simulates an automated parking gate that can be controlled from both a driver's panel and a security panel. It includes safety features such as obstacle detection and motion limits.

---

## 2. Project Objectives

- Apply real-time operating system concepts using FreeRTOS
- Design a multitasking embedded system
- Implement safe gate control using limit buttons
- Use RTOS mechanisms: tasks, queues, semaphores, and mutexes
- Develop a finite state machine for system behavior

---

## 3. Hardware Components

| Component                    | Description                                   |
| ---------------------------- | --------------------------------------------- |
| Tiva-C TM4C123GH6PM          | Main microcontroller                          |
| Green LED                    | Indicates gate moving UP (opening)            |
| Red LED                      | Indicates gate moving DOWN (closing)          |
| Push Button - Driver OPEN    | Driver requests gate to open                  |
| Push Button - Driver CLOSE   | Driver requests gate to close                 |
| Push Button - Security OPEN  | Security requests gate to open                |
| Push Button - Security CLOSE | Security requests gate to close               |
| Push Button - Open Limit     | Simulates gate reaching fully open position   |
| Push Button - Closed Limit   | Simulates gate reaching fully closed position |
| Push Button - Obstacle       | Simulates obstacle detection                  |

---

## 4. How Simulation Works

| Action            | Simulation                                                                                        |
| ----------------- | ------------------------------------------------------------------------------------------------- |
| Gate opening      | Green LED turns ON. Press Open Limit button when you want to simulate gate reaching fully open.   |
| Gate closing      | Red LED turns ON. Press Closed Limit button when you want to simulate gate reaching fully closed. |
| Obstacle detected | While gate is closing (Red LED ON), press Obstacle button to simulate an obstacle.                |

**Testing with Simulator Flags:**

All buttons support simulation via debug flags in the Keil debugger:

- `gSimDriverOpen` — Simulate driver OPEN press
- `gSimDriverClose` — Simulate driver CLOSE press
- `gSimSecurityOpen` — Simulate security OPEN press
- `gSimSecurityClose` — Simulate security CLOSE press
- `gSimOpenLimit` — Simulate open limit reached
- `gSimClosedLimit` — Simulate closed limit reached
- `gSimObstacle` — Simulate obstacle detected

**To use simulator flags:**

1. Halt execution in Keil debugger
2. Set flag value in Watch window: `gSimDriverOpen = 1`
3. Resume execution
4. Button event detected on next 10ms poll cycle

---

## 5. System Functional Requirements

### 5.1 Manual Mode

When the OPEN or CLOSE button is held, the gate moves only while the button remains pressed. Releasing the button stops the gate.

### 5.2 One-Touch Auto Mode

A brief press of the button causes the gate to move fully in that direction until the corresponding limit button is pressed (simulating gate reaching the limit).

### 5.3 Obstacle Protection

If an obstacle is detected while the gate is auto-closing:

- Gate stops immediately
- Gate reverses (opens) for 0.5 seconds
- Gate stops completely

### 5.4 Priority Control

Security panel commands take priority over driver panel commands when both are active simultaneously.

---

## 6. FreeRTOS Task Design

| Task              | Priority    | Description                                              |
| ----------------- | ----------- | -------------------------------------------------------- |
| Input Task        | High (4)    | Reads and debounces all buttons; sends events to a queue |
| Gate Control Task | Medium (3)  | Implements the state machine; decides gate actions       |
| LED Control Task  | Medium (3)  | Controls Red/Green LEDs based on gate movement direction |
| Safety Task       | Highest (5) | Monitors obstacle signals; enforces safety behavior      |
| Status Task       | Low (1)     | Displays system state using LEDs or UART (optional)      |
| Test Task         | Highest (9) | Automated test harness (compile-time optional)           |

---

## 7. Inter-Task Communication

| Mechanism            | Purpose                                                     |
| -------------------- | ----------------------------------------------------------- |
| Queue                | Send button events and commands between tasks               |
| Semaphore (Limit)    | Signal limit button activation                              |
| Semaphore (Obstacle) | Signal obstacle detection                                   |
| Mutex                | Protect shared gate state variable to avoid race conditions |

---

## 8. Gate State Machine

### States

| State          | Description                                              | LED Status |
| -------------- | -------------------------------------------------------- | ---------- |
| IDLE_CLOSED    | Gate is fully closed and stationary                      | All OFF    |
| IDLE_OPEN      | Gate is fully open and stationary                        | All OFF    |
| OPENING        | Gate is moving up (driver/security requested open)       | Green ON   |
| CLOSING        | Gate is moving down (driver/security requested close)    | Red ON     |
| STOPPED_MIDWAY | Gate stopped between limits (manual release or conflict) | All OFF    |
| REVERSING      | Gate reversing due to obstacle detection                 | Green ON   |

---

## 9. Project Structure

```
Smart-Parking-Garage-Gate-System/
├── README.md                          # This file
├── Parking-System/
│   ├── main.c                        # FreeRTOS init, task creation
│   ├── gate_control.c/h              # Gate FSM and state transitions
│   ├── gpio_config.c/h               # GPIO setup, button/LED readers
│   ├── led_control.c/h               # LED control logic
│   ├── test_harness.c/h              # Automated test suite
│   ├── tasks/
│   │   ├── input_task.c/h            # Button polling & debouncing
│   │   ├── safety_task.c/h           # Obstacle detection & reverse
│   │   ├── status_task.c/h           # LED status indicator
│   ├── Required_Files/
│   │   ├── basic_io.c/h              # Print functions (thread-safe)
│   │   ├── consoleprint.c            # ITM debug console
│   │   ├── retarget.c                # ARM I/O retargeting
│   ├── project1.uvprojx              # Keil uVision project file
```

---

## 10. Building and Running

### Build with Keil uVision:

```bash
cd Parking-System
"C:\Keil_v5\UV4\UV4.exe" -b project1.uvprojx -j0
```

### Debug:

1. Connect TM4C123GH6PM board to USB
2. Open `project1.uvprojx` in Keil uVision
3. Click **Debug** → View **ITM Console** for output
4. Use simulator flags to test button presses

### Run Tests:

To enable automated test suite:

1. Set `ENABLE_TEST_HARNESS = 1` in `test_harness.h`
2. Rebuild project
3. Debug → observe test output in ITM Console
4. Test suite runs all 21 test cases automatically and prints PASS/FAIL

---

## 11. Debug Output

All events are printed to ITM Debug Console via `vPrintString()`:

- **Startup:** `"Gate Status: IDLE_CLOSED"`
- **State transitions:** `"[Gate] IDLE_CLOSED -> OPENING"`
- **Obstacle:** `"Obstacle detected: CLOSING -> REVERSING"`
- **Auto-mode:** `"[Gate] OPENING -> IDLE_OPEN"` (on limit press)
- **Manual mode:** `"[Gate] OPENING -> STOPPED_MIDWAY"` (on button release)

---

## 12. Test Case Scenarios (21 Test Cases)

All test cases are implemented in `test_harness.c` and can be run automatically when enabled.

### Manual Mode Tests (TC-01 to TC-03)

| Test      | Action                                         | Expected Result                    |
| --------- | ---------------------------------------------- | ---------------------------------- |
| **TC-01** | Hold OPEN button, release before Open Limit    | Gate stops at STOPPED_MIDWAY state |
| **TC-02** | Hold CLOSE button, release before Closed Limit | Gate stops at STOPPED_MIDWAY state |
| **TC-03** | Control gate from STOPPED_MIDWAY               | Gate responds in both directions   |

### One-Touch Auto Mode Tests (TC-04 to TC-06)

| Test      | Action                                        | Expected Result                                            |
| --------- | --------------------------------------------- | ---------------------------------------------------------- |
| **TC-04** | Brief tap (hold <400ms) OPEN from IDLE_CLOSED | Green LED ON until Open Limit pressed; reaches IDLE_OPEN   |
| **TC-05** | Brief tap (hold <400ms) CLOSE from IDLE_OPEN  | Red LED ON until Closed Limit pressed; reaches IDLE_CLOSED |
| **TC-06** | Tap from STOPPED_MIDWAY state                 | Auto-completes to limit in either direction                |

### Obstacle Detection Tests (TC-07 to TC-09)

| Test      | Action                              | Expected Result                                               |
| --------- | ----------------------------------- | ------------------------------------------------------------- |
| **TC-07** | Obstacle pressed while CLOSING      | Gate REVERSING (Green ON) → 500ms delay → STOPPED_MIDWAY      |
| **TC-08** | Obstacle while holding CLOSE button | Obstacle safety priority overrides held button; gate reverses |
| **TC-09** | Obstacle pressed while OPENING      | Obstacle ignored; gate continues OPENING                      |

### Limit Button Tests (TC-10 to TC-12)

| Test      | Action                                   | Expected Result                         |
| --------- | ---------------------------------------- | --------------------------------------- |
| **TC-10** | Press Open Limit during OPENING          | Transitions to IDLE_OPEN; Green LED OFF |
| **TC-11** | Press Closed Limit during CLOSING        | Transitions to IDLE_CLOSED; Red LED OFF |
| **TC-12** | Press wrong limit (Closed while OPENING) | No effect; gate continues OPENING       |

### Security Priority Tests (TC-13 to TC-14)

| Test      | Action                                              | Expected Result                            |
| --------- | --------------------------------------------------- | ------------------------------------------ |
| **TC-13** | Driver CLOSE + Security OPEN pressed simultaneously | Security OPEN takes priority; gate opens   |
| **TC-14** | Driver OPEN + Security CLOSE pressed simultaneously | Security CLOSE takes priority; gate closes |

### Conflicting Input Tests (TC-15 to TC-16)

| Test      | Action                                               | Expected Result                     |
| --------- | ---------------------------------------------------- | ----------------------------------- |
| **TC-15** | Driver OPEN + CLOSE pressed simultaneously (stopped) | Gate stops safely at STOPPED_MIDWAY |
| **TC-16** | Security OPEN + CLOSE pressed simultaneously         | Gate stops safely at STOPPED_MIDWAY |

### RTOS Mechanism Tests (TC-19 to TC-21)

| Test      | Purpose                                   | Expected Result                           |
| --------- | ----------------------------------------- | ----------------------------------------- |
| **TC-19** | Rapid button presses (queue robustness)   | All events delivered without loss         |
| **TC-20** | Concurrent state reads (mutex protection) | Consistent gate state; no race conditions |
| **TC-21** | Limit button signaling (semaphore)        | Immediate gate stop; Blue LED flashes     |

---

## 13. Grading Criteria

| Criteria                                      | Marks  | Test Cases           |
| --------------------------------------------- | ------ | -------------------- |
| Manual Mode works (hold/release behavior)     | 2      | TC-01, TC-02, TC-03  |
| One-Touch Auto Mode works (tap → full motion) | 2      | TC-04, TC-05, TC-06  |
| Obstacle detection & reverse (0.5s duration)  | 2      | TC-07, TC-08, TC-09  |
| Limit buttons stop gate correctly             | 2      | TC-10, TC-11, TC-12  |
| Security priority over Driver commands        | 1      | TC-13, TC-14         |
| Conflicting inputs result in safe stop        | 1      | TC-15, TC-16         |
| Tasks created and scheduled correctly         | 2      | Code review          |
| Task priorities assigned properly             | 2      | Code review          |
| Semaphores/Mutexes for resource protection    | 2      | TC-20, TC-21         |
| Queues for inter-task communication           | 2      | TC-19                |
| Full Technical Report & Documentation         | 20     | Separate deliverable |
| **TOTAL**                                     | **20** |                      |

---

## 14. Key Features

✅ **Real-time state machine** with debounced button input (30ms debounce)  
✅ **Manual mode** — gate moves while button held, stops on release  
✅ **One-touch auto mode** — brief tap (<400ms) keeps gate moving to limit  
✅ **Obstacle safety** — automatic gate reversal with 500ms reverse duration  
✅ **Security priority** — security commands override driver commands  
✅ **Conflict detection** — simultaneous OPEN+CLOSE results in safe stop  
✅ **LED indicators** — Green (opening), Red (closing), Blue (events)  
✅ **FreeRTOS scheduling** — 5 concurrent tasks with assigned priorities  
✅ **Simulator support** — debug flags for headless testing  
✅ **Thread-safe printing** — ITM console output via critical sections  
✅ **Optional test suite** — 21 automated test cases (compile-time controlled)

---

## 15. Technical Specifications

- **Microcontroller:** Tiva-C TM4C123GH6PM (Cortex-M4, 12MHz)
- **RTOS:** FreeRTOS 11.2.0 with CMSIS RTOS2 wrapper
- **Build Environment:** Keil uVision V5.38.0, ARM Compiler V6.19
- **Debug:** ITM Debug Console (no semihosting delays)
- **Button Debounce:** 30ms (3 × 10ms poll cycles)
- **Obstacle Reverse Duration:** 500ms
- **Manual/Auto Threshold:** 400ms button hold time
- **Test Suite:** 21 test cases (optional, disabled by default via `ENABLE_TEST_HARNESS`)
