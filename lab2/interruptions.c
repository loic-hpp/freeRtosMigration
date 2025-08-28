#include "interruptions.h"
#include <xil_printf.h>

// ================== DEVICE INSTANCES ==================
XIntc  axi_intc;
XGpio  gpButton;
XGpio  gpSwitch;
XTmrCtr timer_dev;

// ================== GPIO INITIALIZATION ==================

void initialize_gpio0(void)
{
    if (XGpio_Initialize(&gpButton, GPIO_BUTTON_DEVICE_ID) == XST_DEVICE_NOT_FOUND)
        xil_printf("Erreur init gpio0\n");

    XGpio_SetDataDirection(&gpButton, 1, 0x1);
    XGpio_SetDataDirection(&gpButton, 2, 0x0);

    XGpio_InterruptGlobalEnable(&gpButton);
    XGpio_InterruptEnable(&gpButton, XGPIO_IR_MASK);
}

void initialize_gpio1(void)
{
    if (XGpio_Initialize(&gpSwitch, GPIO_SWITCH_DEVICE_ID) == XST_DEVICE_NOT_FOUND)
        xil_printf("Erreur init gpio1\n");

    XGpio_SetDataDirection(&gpSwitch, 1, 0x1);
    XGpio_SetDataDirection(&gpSwitch, 2, 0x0);

    XGpio_InterruptGlobalEnable(&gpSwitch);
    XGpio_InterruptEnable(&gpSwitch, XGPIO_IR_MASK);
}

// ================== TIMER INITIALIZATION ==================

void initialize_timer(void)
{
    XTmrCtr_Config timerConfig;

    timerConfig.BaseAddress = XPAR_AXI_TIMER_0_BASEADDR;
    timerConfig.SysClockFreqHz = XPAR_CPU_CORE_CLOCK_FREQ_HZ;

    XTmrCtr_CfgInitialize(&timer_dev, &timerConfig, timerConfig.BaseAddress);

    XTmrCtr_SetResetValue(&timer_dev, 0, 0xFA56EA00);
    XTmrCtr_SetOptions(&timer_dev, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION | XTC_DOWN_COUNT_OPTION);
}

// ================== INTC INITIALIZATION ==================

int initialize_axi_intc(void)
{
    return XIntc_Initialize(&axi_intc, 0);
}

// ================== INTERRUPT CONNECTION ==================

// int connect_fit_timer_irq0(void)
// {
//     int status = XIntc_Connect(&axi_intc, XPAR_FABRIC_AXI_GPIO_0_INTR, (XInterruptHandler)fit_timer_isr0, NULL);
//     if (status == XST_SUCCESS) XIntc_Enable(&axi_intc, XPAR_FABRIC_AXI_GPIO_0_INTR);
//     return status;
// }

// int connect_fit_timer_irq1(void)
// {
//     int status = XIntc_Connect(&axi_intc, XPAR_FABRIC_AXI_GPIO_1_INTR, (XInterruptHandler)fit_timer_isr1, NULL);
//     if (status == XST_SUCCESS) XIntc_Enable(&axi_intc, XPAR_FABRIC_AXI_GPIO_1_INTR);
//     return status;
// }

// int connect_timer_irq(void)
// {
//     int status = XIntc_Connect(&axi_intc, XPAR_FABRIC_AXI_TIMER_0_INTERRUPT_INTR, (XInterruptHandler)timer_isr, NULL);
//     if (status == XST_SUCCESS) XIntc_Enable(&axi_intc, XPAR_FABRIC_AXI_TIMER_0_INTERRUPT_INTR);
//     return status;
// }

int connect_gpio_irq0(void)
{
    int status = XIntc_Connect(&axi_intc, XPAR_FABRIC_AXI_GPIO_0_INTR, (XInterruptHandler)gpio_isr0, &gpButton);
    if (status == XST_SUCCESS) XIntc_Enable(&axi_intc, XPAR_FABRIC_AXI_GPIO_0_INTR);
    return status;
}

int connect_gpio_irq1(void)
{
    int status = XIntc_Connect(&axi_intc, XPAR_FABRIC_AXI_GPIO_1_INTR, (XInterruptHandler)gpio_isr1, &gpSwitch);
    if (status == XST_SUCCESS) XIntc_Enable(&axi_intc, XPAR_FABRIC_AXI_GPIO_1_INTR);
    return status;
}

// ================== AXI CONNECTION ==================

void connect_axi(void)
{
    int status = 0;
    status |= connect_gpio_irq0();
    status |= connect_gpio_irq1();
    // status |= connect_fit_timer_irq0();
    // status |= connect_fit_timer_irq1();
    // status |= connect_timer_irq();

    XIntc_Start(&axi_intc, XIN_REAL_MODE);

    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
                                 (Xil_ExceptionHandler)XIntc_InterruptHandler,
                                 &axi_intc);
    Xil_ExceptionEnable();
    if(status != XST_SUCCESS)
        xil_printf("ERREUR: Erreur lors de la connexion des interruptions");
}

// ================== CLEANUP ==================

void cleanup(void)
{
    XIntc_Disconnect(&axi_intc, XPAR_FABRIC_AXI_GPIO_0_INTR);
    XIntc_Disconnect(&axi_intc, XPAR_FABRIC_AXI_GPIO_1_INTR);
    // XIntc_Disconnect(&axi_intc, XPAR_FABRIC_AXI_TIMER_0_INTERRUPT_INTR);
}

void eanable_interruption(){
    initialize_gpio0();
	// initialize_gpio1();
	// initialize_timer();
	initialize_axi_intc();
	connect_axi();
}
