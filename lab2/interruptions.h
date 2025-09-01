/* ************************************************
 *                INTERRUPTIONS
 **************************************************/

#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "xgpio.h"
#include "xtmrctr.h"
#include "xintc.h"
#include "xil_exception.h"
#include "xparameters.h"
#include <stdint.h>
#include "xscugic.h"



// ================== DEVICE IDs ==================
#define GIC_DEVICE_ID                0U
#define GPIO_BUTTON_DEVICE_ID        0
#define INTC_DEVICE_ID               0
#define GPIO_SWITCH_DEVICE_ID        1
#define TIMER_IRQ_ID                 4U
#define AXI_INTC_IRQ_ID              31


// ================== INTERRUPT IDs ==================
#define PL_INTC_IRQ_ID               XPS_IRQ_INT_ID
#define FIT_IRQ0_ID                  1U
#define FIT_IRQ1_ID                  0U
#define GPIO_BUTTON_IRQ_ID           XPAR_AXI_INTC_0_AXI_GPIO_0_IP2INTC_IRPT_INTR
#define GPIO_SWITCH_IRQ_ID           XPAR_AXI_INTC_0_AXI_GPIO_1_IP2INTC_IRPT_INTR

#define XPAR_AXI_TIMER_DEVICE_ID     0U

// ================== GPIO MASKS ==================
#define XGPIO_IR_MASK                0x3  /* Mask of all bits */

#define GPIO_BUTTONS_CHANNEL         1
#define GPIO_LEDS_CHANNEL            2

// ================== LED COLORS ==================
#define COLOR_DOUBLE_BLUE            0b100100
#define COLOR_DOUBLE_RED             0b001001
#define COLOR_DOUBLE_GREEN           0b010010
#define COLOR_DOUBLE_PURPLE          0b101101
#define COLOR_DOUBLE_YELLOW          0b011011

// ================== LED MACROS ==================
#define TurnLEDButton(color) XGpio_DiscreteWrite(&gpButton, GPIO_LEDS_CHANNEL, color)
#define TurnLEDSwitch(color) XGpio_DiscreteWrite(&gpSwitch, GPIO_LEDS_CHANNEL, color)

// ================== BUTTON MASKS ==================
#define BP0                          0b0001  // Start button
#define BP1                          0b0010  // Stop button
#define BP2 						 0b0100		// Bouton pressoir pour l arret complet

// ================== SWITCH MODES ==================
#define NO_STAT                      0b00
#define SWITCH1                      0b01  // Stat every 11 sec
#define SWITCH2                      0b10  // Stat every 20 sec
#define SWITCH1and2                  0b11

// ================== DEVICE INSTANCES ==================
extern XGpio gpButton;
extern XGpio gpSwitch;
extern XTmrCtr timer_dev;



void eanable_interruption();
int connect_AXI_INTC_to_GIC();


void fit_timer_isr0(void *p_int_arg, uint32_t source_cpu);
void fit_timer_isr1(void *p_int_arg, uint32_t source_cpu);

void timer_isr(void *p_int_arg, uint32_t source_cpu);


void gpio_isr0(void *p_int_arg, uint32_t source_cpu);
void gpio_isr1(void *p_int_arg, uint32_t source_cpu);

#endif /* INTERRUPTS_H */