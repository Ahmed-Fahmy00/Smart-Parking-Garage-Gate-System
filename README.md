# Smart Parking Garage Gate System

A real-time embedded system built with FreeRTOS for controlling an automated parking garage gate. The system manages gate operations, monitors occupancy, detects obstacles, and enforces parking limits using multiple input buttons and LED indicators.

## System Overview

- **MCU:** TM4C123GH6PM (Cortex-M4, 12MHz)
- **RTOS:** FreeRTOS 11.2.0 (CMSIS RTOS2)
- **Build Environment:** Keil uVision V5.38.0 with ARM Compiler V6.19
- **Debug Output:** ITM Debug Console
- **Capacity:** 5 parking spaces

---

## Button Configuration

All buttons are read via the `Btn_*_Read()` functions in `gpio_config.c`. Each button supports simulator testing via debug flags that can be set in the debugger.

### 1. **Driver OPEN Button** — Port D, Pin 0 (PD0)

- **Pin Logic:** Active-HIGH (pressed = 1, released = 0)
- **Function:** Driver requests gate to open
- **Debounce:** 30ms (3 × 10ms poll cycles)
- **Simulator Flag:** `gSimDriverOpen` (set to 1 to simulate press)
- **Result:** Signals gate to move from CLOSED state toward OPEN

### 2. **Driver CLOSE Button** — Port D, Pin 1 (PD1)

- **Pin Logic:** Active-HIGH (pressed = 1, released = 0)
- **Function:** Driver requests gate to close
- **Debounce:** 30ms (3 × 10ms poll cycles)
- **Simulator Flag:** `gSimDriverClose` (set to 1 to simulate press)
- **Result:** Signals gate to move from OPEN state toward CLOSED

### 3. **Security OPEN Button** — Port B, Pin 0 (PB0)

- **Pin Logic:** Active-HIGH (pressed = 1, released = 0)
- **Function:** Security personnel can force gate open (override)
- **Debounce:** 30ms (3 × 10ms poll cycles)
- **Simulator Flag:** `gSimSecurityOpen` (set to 1 to simulate press)
- **Result:** Force gate to OPEN state regardless of parking status

### 4. **Security CLOSE Button** — Port B, Pin 1 (PB1)

- **Pin Logic:** Active-HIGH (pressed = 1, released = 0)
- **Function:** Security personnel can force gate closed (override)
- **Debounce:** 30ms (3 × 10ms poll cycles)
- **Simulator Flag:** `gSimSecurityClose` (set to 1 to simulate press)
- **Result:** Force gate to CLOSED state regardless of parking status

### 5. **Open Limit Switch** — Port E, Pin 1 (PE1)

- **Pin Logic:** Active-HIGH (triggered = 1, idle = 0)
- **Function:** Detects when gate reaches fully OPEN position
- **Debounce:** 30ms (3 × 10ms poll cycles)
- **Simulator Flag:** `gSimOpenLimit` (set to 1 to simulate limit reached)
- **Result:** Notifies gate control that opening operation is complete

### 6. **Closed Limit Switch** — Port E, Pin 2 (PE2)

- **Pin Logic:** Active-HIGH (triggered = 1, idle = 0)
- **Function:** Detects when gate reaches fully CLOSED position
- **Debounce:** 30ms (3 × 10ms poll cycles)
- **Simulator Flag:** `gSimClosedLimit` (set to 1 to simulate limit reached)
- **Result:** Notifies gate control that closing operation is complete

### 7. **Obstacle Sensor** — Port F, Pin 4 (PF4)

- **Pin Logic:** Active-HIGH (obstacle detected = 1, clear = 0)
- **Function:** Safety sensor that detects obstacles in gate path
- **Debounce:** 30ms (3 × 10ms poll cycles)
- **Simulator Flag:** `gSimObstacle` (set to 1 to simulate obstacle)
- **Result:** Triggers automatic gate reversal for safety

---

## LED Indicators

### Red LED — Port F, Pin 1 (PF1)

- **State:** ON when gate is CLOSING

### Green LED — Port F, Pin 3 (PF3)

- **State:** ON when gate is OPENING

### Blue LED — Port F, Pin 2 (PF2)

- **State:** Flashes when limit button is activated or status events occur

---

## Gate State Machine

| State               | Meaning                                        | LED Status         |
| ------------------- | ---------------------------------------------- | ------------------ |
| IDLE_CLOSED         | Gate fully closed, ready for entry             | All OFF            |
| OPENING             | Gate moving open (driver pressed OPEN)         | Green ON           |
| IDLE_OPEN           | Gate fully open, parked vehicle inside         | All OFF            |
| CLOSING             | Gate moving closed (driver pressed CLOSE)      | Red ON             |
| GATE_STOPPED_MIDWAY | Gate stopped mid-operation (obstacle detected) | All OFF            |
| GATE_REVERSING      | Gate reversing due to obstacle                 | Red ON (reversing) |

---

## Occupancy Tracking

- **Capacity:** 5 parking spaces (hardcoded)
- **Entry Trigger:** Open Limit Switch activates (vehicle entered)
- **Exit Trigger:** Closed Limit Switch activates (vehicle exited)
- **Full Parking:** When occupancy = 5, Driver OPEN requests are blocked with debug message

---

## Debug/Testing with Simulator Flags

To simulate button presses in Keil debugger without physical hardware:

1. **Halt execution** (Pause in debugger)
2. **Set simulation flag** in Watch window:
   - `gSimDriverOpen = 1`
   - `gSimObstacle = 1`
   - etc.
3. **Resume execution** (Run)
4. Button event will be detected on next 10ms poll cycle

**Example:** To simulate a vehicle attempting entry when parking is full:

```
Set gSimDriverOpen = 1       → System detects OPEN request
→ Occupancy check: 5/5 (FULL)
→ Print: "Parking full - entry denied"
→ Gate remains CLOSED
```

---

## Project Structure

```
Smart-Parking-Garage-Gate-System/
├── main.c                      # FreeRTOS initialization, task creation
├── gate_control.c/h           # Gate FSM and state transitions
├── input_task.c/h             # Button polling & debouncing (10ms)
├── status_task.c/h            # LED control & indicators
├── safety_task.c/h            # Obstacle detection & emergency reverse
├── gpio_config.c/h            # GPIO setup, button/LED reader functions
├── Required_Files/
│   ├── basic_io.c/h          # Print functions (thread-safe)
│   └── consoleprint.c        # ITM debug console output
├── project1.uvprojx           # Keil uVision project file
└── README.md                  # This file
```

---

## Debug Output

All events are printed to ITM Debug Console via `vPrintString()`:

- **Startup:** `"System initialized | Capacity: 5 | Occupied: 0 | Available: 5"`
- **State transitions:** `"[Gate] IDLE_CLOSED -> OPENING | Parking: 0/5"`
- **Obstacle:** `"Obstacle detected: reversing"`
- **Occupancy updates:** `"Parking: 1/5"` (after vehicle entry)
- **Full parking:** `"Parking full - entry denied"`

---

## Building and Running

**Build:**

```bash
cd Parking-System
"C:\Keil_v5\UV4\UV4.exe" -b project1.uvprojx -j0
```

**Debug:**

1. Connect TM4C123GH6PM board to USB
2. Open `project1.uvprojx` in Keil uVision
3. Click **Debug** → View ITM console for output
4. Use simulator flags to test button presses

---

## Key Features

✅ **Real-time state machine** with debounced button input  
✅ **Occupancy tracking** prevents over-parking  
✅ **Safety obstacle detection** with automatic gate reversal  
✅ **Security overrides** for maintenance/emergency  
✅ **Debug printing** via ITM console (no semihosting delays)  
✅ **Thread-safe printing** using FreeRTOS scheduler suspension
