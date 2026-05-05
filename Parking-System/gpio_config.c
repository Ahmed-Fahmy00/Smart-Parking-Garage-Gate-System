/*******************************************************************************
 * gpio_config.c
 * GPIO initialisation and helpers for the Smart Parking Gate.
 *
 * Uses direct register addresses so the project does NOT depend on TivaWare
 * driverlib.  Only the CMSIS device header (provided by Keil's DFP) is
 * needed for interrupt-vector naming - the GPIO registers are accessed via
 * the macros below.
 ******************************************************************************/
#include "gpio_config.h"

/* ========================================================================== */
/*  TM4C123GH6PM register definitions (APB aperture)                         */
/* ========================================================================== */

/* System Control */
#define SYSCTL_RCGCGPIO_R   (*((volatile uint32_t *)0x400FE608))
#define SYSCTL_PRGPIO_R     (*((volatile uint32_t *)0x400FEA08))

/* GPIO Port B (base 0x4000.5000) */
#define GPIO_PORTB_DATA_R   (*((volatile uint32_t *)0x400053FC))
#define GPIO_PORTB_DIR_R    (*((volatile uint32_t *)0x40005400))
#define GPIO_PORTB_AFSEL_R  (*((volatile uint32_t *)0x40005420))
#define GPIO_PORTB_AMSEL_R  (*((volatile uint32_t *)0x40005528))
#define GPIO_PORTB_PCTL_R   (*((volatile uint32_t *)0x4000552C))
#define GPIO_PORTB_DEN_R    (*((volatile uint32_t *)0x4000551C))
#define GPIO_PORTB_PUR_R    (*((volatile uint32_t *)0x40005510))
#define GPIO_PORTB_PDR_R    (*((volatile uint32_t *)0x40005514))

/* GPIO Port D (base 0x4000.7000) */
#define GPIO_PORTD_DATA_R   (*((volatile uint32_t *)0x400073FC))
#define GPIO_PORTD_DIR_R    (*((volatile uint32_t *)0x40007400))
#define GPIO_PORTD_AFSEL_R  (*((volatile uint32_t *)0x40007420))
#define GPIO_PORTD_AMSEL_R  (*((volatile uint32_t *)0x40007528))
#define GPIO_PORTD_PCTL_R   (*((volatile uint32_t *)0x4000752C))
#define GPIO_PORTD_DEN_R    (*((volatile uint32_t *)0x4000751C))
#define GPIO_PORTD_PDR_R    (*((volatile uint32_t *)0x40007514))

/* GPIO Port F (base 0x4002.5000) */
#define GPIO_PORTF_DATA_R   (*((volatile uint32_t *)0x400253FC))
#define GPIO_PORTF_DIR_R    (*((volatile uint32_t *)0x40025400))
#define GPIO_PORTF_AFSEL_R  (*((volatile uint32_t *)0x40025420))
#define GPIO_PORTF_AMSEL_R  (*((volatile uint32_t *)0x40025528))
#define GPIO_PORTF_PCTL_R   (*((volatile uint32_t *)0x4002552C))
#define GPIO_PORTF_DEN_R    (*((volatile uint32_t *)0x4002551C))
#define GPIO_PORTF_PUR_R    (*((volatile uint32_t *)0x40025510))
#define GPIO_PORTF_LOCK_R   (*((volatile uint32_t *)0x40025520))
#define GPIO_PORTF_CR_R     (*((volatile uint32_t *)0x40025524))

/* GPIO Port E and ADC0 for simulator slider input (PE2 / AIN1) */
#define GPIO_PORTE_DATA_R   (*((volatile uint32_t *)0x400243FC))
#define GPIO_PORTE_DIR_R    (*((volatile uint32_t *)0x40024400))
#define GPIO_PORTE_AFSEL_R  (*((volatile uint32_t *)0x40024420))
#define GPIO_PORTE_DEN_R    (*((volatile uint32_t *)0x4002451C))
#define GPIO_PORTE_AMSEL_R  (*((volatile uint32_t *)0x40024528))
#define GPIO_PORTE_PCTL_R   (*((volatile uint32_t *)0x4002452C))
#define GPIO_PORTE_PDR_R    (*((volatile uint32_t *)0x40024514))

#define ADC0_ACTSS_R        (*((volatile uint32_t *)0x40038000))
#define ADC0_RIS_R          (*((volatile uint32_t *)0x40038004))
#define ADC0_ISC_R          (*((volatile uint32_t *)0x4003800C))
#define ADC0_EMUX_R         (*((volatile uint32_t *)0x40038014))
#define ADC0_PSSI_R         (*((volatile uint32_t *)0x40038028))
#define ADC0_SSMUX3_R       (*((volatile uint32_t *)0x400380A0))
#define ADC0_SSCTL3_R       (*((volatile uint32_t *)0x400380A4))
#define ADC0_SSFIFO3_R      (*((volatile uint32_t *)0x400380A8))
#define ADC0_PC_R           (*((volatile uint32_t *)0x40038FC4))

#define SYSCTL_RCGCADC_R    (*((volatile uint32_t *)0x400FE638))

/* ========================================================================== */
/*  Initialisation                                                            */
/* ========================================================================== */

void GPIO_Init(void)
{
    volatile uint32_t delay;

        /* ---- Enable clocks for Port B, Port D, Port E, Port F, and ADC0 ---- */
        SYSCTL_RCGCGPIO_R |= (1U << 1) | (1U << 3) | (1U << 4) | (1U << 5);  /* B, D, E, F */
        SYSCTL_RCGCADC_R  |= (1U << 0);                          /* ADC0 */
    delay = SYSCTL_RCGCGPIO_R;                          /* stabilisation delay  */
        while ((SYSCTL_PRGPIO_R & ((1U << 1) | (1U << 3) | (1U << 4) | (1U << 5)))
            != ((1U << 1) | (1U << 3) | (1U << 4) | (1U << 5)))
    {
        /* wait until both ports are ready */
    }

    /* ================================================================ */
    /*  PORT F  -  LEDs (PF1-PF3 output)                               */
    /* ================================================================ */

    /* Unlock PF0 (NMI-protected) - not used, just standard setup */
    GPIO_PORTF_LOCK_R = 0x4C4F434BU;   /* unlock key              */
    GPIO_PORTF_CR_R  |= 0x1FU;         /* allow changes to PF0-4  */

    /* Direction: PF1,PF2,PF3 = output; PF4 = input */
    GPIO_PORTF_AMSEL_R &= ~((1U << 1) | (1U << 2) | (1U << 3) | (1U << 4));
    GPIO_PORTF_PCTL_R  &= ~0x000FFFF0U;
    GPIO_PORTF_AFSEL_R &= ~((1U << 1) | (1U << 2) | (1U << 3) | (1U << 4));
    GPIO_PORTF_DIR_R   |=  (1U << 1) | (1U << 2) | (1U << 3);
    GPIO_PORTF_DIR_R   &= ~(1U << 4);

    /* PF4 is active-low, so use pull-up */
    GPIO_PORTF_PUR_R   |=  (1U << 4);

    /* Digital enable PF1..PF4 */
    GPIO_PORTF_DEN_R   |=  (1U << 1) | (1U << 2) | (1U << 3) | (1U << 4);

    /* LEDs start OFF */
    GPIO_PORTF_DATA_R &= ~((1U << 1) | (1U << 2) | (1U << 3));

    /* ================================================================ */
    /*  PORT E  -  PE0/PE1 buttons and PE2 ADC input                    */
    /* ================================================================ */
    GPIO_PORTE_AMSEL_R &= ~((1U << 0) | (1U << 1));
    GPIO_PORTE_PCTL_R  &= ~0x000000FFU;
    GPIO_PORTE_AFSEL_R &= ~((1U << 0) | (1U << 1));
    GPIO_PORTE_DIR_R   &= ~((1U << 0) | (1U << 1));
    GPIO_PORTE_PDR_R   |=  ((1U << 0) | (1U << 1));
    GPIO_PORTE_DEN_R   |=  ((1U << 0) | (1U << 1));

    GPIO_PORTE_DIR_R   &= ~(1U << 2);
    GPIO_PORTE_AFSEL_R |=  (1U << 2);
    GPIO_PORTE_DEN_R   &= ~(1U << 2);
    GPIO_PORTE_AMSEL_R |=  (1U << 2);
    GPIO_PORTE_PCTL_R  &= ~(0xFU << 8);

    /* ADC0 sequencer 3: single sample from AIN1 (PE2) */
    ADC0_ACTSS_R &= ~(1U << 3);
    ADC0_EMUX_R  &= ~0x0000F000U;
    ADC0_SSMUX3_R = 1U;
    ADC0_SSCTL3_R = 0x6U;
    ADC0_PC_R     = 0x1U;
    ADC0_ACTSS_R |= (1U << 3);

    /* ================================================================ */
    /*  PORT B  -  PB0/PB1 buttons                                     */
    /* ================================================================ */
    GPIO_PORTB_AMSEL_R &= ~((1U << 0) | (1U << 1));
    GPIO_PORTB_PCTL_R  &= ~0x000000FFU;
    GPIO_PORTB_AFSEL_R &= ~((1U << 0) | (1U << 1));
    GPIO_PORTB_DIR_R   &= ~((1U << 0) | (1U << 1));
    GPIO_PORTB_PDR_R   |=  ((1U << 0) | (1U << 1));
    GPIO_PORTB_DEN_R   |=  ((1U << 0) | (1U << 1));

    /* ================================================================ */
    /*  PORT D  -  PD0/PD1 buttons                                     */
    /* ================================================================ */
    GPIO_PORTD_AMSEL_R &= ~((1U << 0) | (1U << 1));
    GPIO_PORTD_PCTL_R  &= ~0x000000FFU;
    GPIO_PORTD_AFSEL_R &= ~((1U << 0) | (1U << 1));
    GPIO_PORTD_DIR_R   &= ~((1U << 0) | (1U << 1));
    GPIO_PORTD_PDR_R   |=  ((1U << 0) | (1U << 1));
    GPIO_PORTD_DEN_R   |=  ((1U << 0) | (1U << 1));
}

/* ========================================================================== */
/*  LED helpers                                                               */
/* ========================================================================== */

void LED_Red_On(void)    { GPIO_PORTF_DATA_R |=  LED_RED_PIN;   }
void LED_Red_Off(void)   { GPIO_PORTF_DATA_R &= ~LED_RED_PIN;   }
void LED_Green_On(void)  { GPIO_PORTF_DATA_R |=  LED_GREEN_PIN; }
void LED_Green_Off(void) { GPIO_PORTF_DATA_R &= ~LED_GREEN_PIN; }
void LED_Blue_On(void)   { GPIO_PORTF_DATA_R |=  LED_BLUE_PIN;  }
void LED_Blue_Off(void)  { GPIO_PORTF_DATA_R &= ~LED_BLUE_PIN;  }

void LEDs_AllOff(void)
{
    GPIO_PORTF_DATA_R &= ~(LED_RED_PIN | LED_GREEN_PIN | LED_BLUE_PIN);
}

/* ========================================================================== */
/*  Button readers                                                           */
/* ========================================================================== */

uint8_t Btn_DriverOpen_Read(void)
{
    if (gSimDriverOpen)
        return BTN_PRESSED;

    if ((GPIO_PORTF_DATA_R & BTN_DRIVER_OPEN_PIN) == 0)
        return BTN_PRESSED;

    return BTN_RELEASED;
}

uint8_t Btn_DriverClose_Read(void)
{
    if (gSimDriverClose)
        return BTN_PRESSED;

    if ((GPIO_PORTE_DATA_R & BTN_DRIVER_CLOSE_PIN) != 0)
        return BTN_PRESSED;

    return BTN_RELEASED;
}

uint8_t Btn_SecurityOpen_Read(void)
{
        return gSimSecurityOpen ? BTN_PRESSED :
            (((GPIO_PORTE_DATA_R & BTN_SECURITY_OPEN_PIN) != 0) ? BTN_PRESSED : BTN_RELEASED);
}

uint8_t Btn_SecurityClose_Read(void)
{
    if (gSimSecurityClose)
        return BTN_PRESSED;

    return ((GPIO_PORTB_DATA_R & BTN_SECURITY_CLOSE_PIN) != 0) ? BTN_PRESSED : BTN_RELEASED;
}

uint8_t Btn_OpenLimit_Read(void)
{
    if (gSimOpenLimit)
        return BTN_PRESSED;

    return ((GPIO_PORTB_DATA_R & BTN_OPEN_LIMIT_PIN) != 0) ? BTN_PRESSED : BTN_RELEASED;
}

uint8_t Btn_ClosedLimit_Read(void)
{
        return gSimClosedLimit ? BTN_PRESSED :
            (((GPIO_PORTD_DATA_R & BTN_CLOSED_LIMIT_PIN) != 0) ? BTN_PRESSED : BTN_RELEASED);
}

uint8_t Btn_Obstacle_Read(void)
{
        return gSimObstacle ? BTN_PRESSED :
            (((GPIO_PORTD_DATA_R & BTN_OBSTACLE_PIN) != 0) ? BTN_PRESSED : BTN_RELEASED);
}

uint16_t ADC_Slider_Read(void)
{
    return 0;
}
