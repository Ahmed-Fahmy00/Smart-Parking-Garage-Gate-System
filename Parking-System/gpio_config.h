/*******************************************************************************
 * gpio_config.h
 * GPIO pin definitions and hardware abstraction for the Smart Parking Gate.
 *
 * Pin map - Updated Configuration:
 * -------------------------------------------------------
 *  PF1  -  Red   LED            (output)  Gate closing
 *  PF2  -  Blue  LED            (output)  Status indicator
 *  PF3  -  Green LED            (output)  Gate opening
 *  PD0  -  Driver OPEN          (input)   active-high
 *  PD1  -  Driver CLOSE         (input)   active-high
 *  PB0  -  Security OPEN        (input)   active-high
 *  PB1  -  Security CLOSE       (input)   active-high
 *  PE0  -  Open Limit           (input)   active-high
 *  PE2  -  Open Limit (aux)     (input)   active-high
 *  PE1  -  Closed Limit         (input)   active-high
 *  PF4  -  Obstacle             (input)   active-high
 ******************************************************************************/
#ifndef GPIO_CONFIG_H
#define GPIO_CONFIG_H

#include <stdint.h>

/* ---- LED bit-masks (Port F) ---- */
#define LED_RED_PIN         (1U << 1)   /* PF1 */
#define LED_BLUE_PIN        (1U << 2)   /* PF2 */
#define LED_GREEN_PIN       (1U << 3)   /* PF3 */

/* ---- Button bit-masks ---- */
#define BTN_DRIVER_OPEN_PIN     (1U << 0)   /* PD0 */
#define BTN_DRIVER_CLOSE_PIN    (1U << 1)   /* PD1 */
#define BTN_SECURITY_OPEN_PIN   (1U << 0)   /* PB0 */
#define BTN_SECURITY_CLOSE_PIN  (1U << 1)   /* PB1 */
#define BTN_OPEN_LIMIT_PIN_PE0   (1U << 0)   /* PE0 */
#define BTN_OPEN_LIMIT_PIN_PE2   (1U << 2)   /* PE2 (was ADC AIN1) */
#define BTN_CLOSED_LIMIT_PIN    (1U << 1)   /* PE1 */
#define BTN_OBSTACLE_PIN        (1U << 4)   /* PF4 */
#define BTN_SW2_PIN             (1U << 0)   /* PF0 (SW2) - runtime mode toggle */

/* Logic levels (active-HIGH: pressed = 1) */
#define BTN_PRESSED     1
#define BTN_RELEASED    0

/* Debug simulation flags defined in main.c.
 * If the code detects the Keil simulator, these flags are used instead of
 * raw GPIO reads so the buttons do not appear pressed by default. */
extern volatile uint8_t gSimDriverOpen;
extern volatile uint8_t gSimDriverClose;
extern volatile uint8_t gSimSecurityOpen;
extern volatile uint8_t gSimSecurityClose;
extern volatile uint8_t gSimOpenLimit;
extern volatile uint8_t gSimClosedLimit;
extern volatile uint8_t gSimObstacle;

/* ---- Public API ---- */
void    GPIO_Init(void);

/* LED helpers */
void    LED_Red_On(void);
void    LED_Red_Off(void);
void    LED_Green_On(void);
void    LED_Green_Off(void);
void    LED_Blue_On(void);
void    LED_Blue_Off(void);
void    LEDs_AllOff(void);

/* Button readers - each returns BTN_PRESSED (1) or BTN_RELEASED (0) */
uint8_t Btn_DriverOpen_Read(void);
uint8_t Btn_DriverClose_Read(void);
uint8_t Btn_SecurityOpen_Read(void);
uint8_t Btn_SecurityClose_Read(void);
uint8_t Btn_OpenLimit_Read(void);
uint8_t Btn_ClosedLimit_Read(void);
uint8_t Btn_Obstacle_Read(void);
uint8_t Btn_SW2_Read(void);

uint16_t ADC_Slider_Read(void);

#endif /* GPIO_CONFIG_H */
