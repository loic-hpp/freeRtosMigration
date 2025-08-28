#include "interruptions.h"
#include <xil_printf.h>

// ================== DEVICE INSTANCES ==================
XIntc axi_intc;
XGpio gpButton;
XGpio gpSwitch;
XTmrCtr timer_dev;

XScuGic gic_intc;
static XScuGic_Config *gic_config;

// ================== GPIO INITIALIZATION ==================

void initialize_gpio0(void) {
  int status = XGpio_Initialize(&gpButton, XPAR_AXI_GPIO_0_BASEADDR);
  if (status != XST_SUCCESS) {
    xil_printf("XGpio_Initialize failed\r\n");
    return;
  }

  XGpio_SetDataDirection(&gpButton, 1, 0x1);
  XGpio_SetDataDirection(&gpButton, 2, 0x0);

  XGpio_InterruptEnable(&gpButton, XGPIO_IR_MASK);
  XGpio_InterruptGlobalEnable(&gpButton);
}

void initialize_gpio1(void) {
  int status = XGpio_Initialize(&gpSwitch, XPAR_AXI_GPIO_1_BASEADDR);
  if (status != XST_SUCCESS) {
    xil_printf("Erreur init gpio1\n");
    return;
  }

  XGpio_SetDataDirection(&gpSwitch, 1, 0x1);
  XGpio_SetDataDirection(&gpSwitch, 2, 0x0);

  XGpio_InterruptEnable(&gpSwitch, XGPIO_IR_MASK);
  XGpio_InterruptGlobalEnable(&gpSwitch);
}

// ================== TIMER INITIALIZATION ==================

void initialize_timer(void) {
  XTmrCtr_Config timerConfig;

  timerConfig.BaseAddress = XPAR_AXI_TIMER_0_BASEADDR;
  timerConfig.SysClockFreqHz = XPAR_CPU_CORE_CLOCK_FREQ_HZ;

  XTmrCtr_CfgInitialize(&timer_dev, &timerConfig, timerConfig.BaseAddress);

  XTmrCtr_SetResetValue(&timer_dev, 0, 0xFA56EA00);
  XTmrCtr_SetOptions(&timer_dev, 0,
                     XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION |
                         XTC_DOWN_COUNT_OPTION);
}

// ================== INTC INITIALIZATION ==================

int initialize_axi_intc(void) {
  int status = XIntc_Initialize(&axi_intc, XPAR_AXI_INTC_0_BASEADDR);
  if (status != XST_SUCCESS) {
    xil_printf("XIntc_Initialize failed\r\n");
    return XST_FAILURE;
  }
  return status;
}

// ================== INTERRUPT CONNECTION ==================

// int connect_fit_timer_irq0(void)
// {
//     int status = XIntc_Connect(&axi_intc, XPAR_FABRIC_AXI_GPIO_0_INTR,
//     (XInterruptHandler)fit_timer_isr0, NULL); if (status == XST_SUCCESS)
//     XIntc_Enable(&axi_intc, XPAR_FABRIC_AXI_GPIO_0_INTR); return status;
// }

// int connect_fit_timer_irq1(void)
// {
//     int status = XIntc_Connect(&axi_intc, XPAR_FABRIC_AXI_GPIO_1_INTR,
//     (XInterruptHandler)fit_timer_isr1, NULL); if (status == XST_SUCCESS)
//     XIntc_Enable(&axi_intc, XPAR_FABRIC_AXI_GPIO_1_INTR); return status;
// }

// int connect_timer_irq(void)
// {
//     int status = XIntc_Connect(&axi_intc,
//     XPAR_FABRIC_AXI_TIMER_0_INTERRUPT_INTR, (XInterruptHandler)timer_isr,
//     NULL); if (status == XST_SUCCESS) XIntc_Enable(&axi_intc,
//     XPAR_FABRIC_AXI_TIMER_0_INTERRUPT_INTR); return status;
// }

int connect_gpio_irq0(void) {
  int status = XIntc_Connect(&axi_intc, XPAR_FABRIC_AXI_GPIO_0_INTR,
                             (XInterruptHandler)gpio_isr0, &gpButton);
  if (status == XST_SUCCESS)
    XIntc_Enable(&axi_intc, XPAR_FABRIC_AXI_GPIO_0_INTR);
  return status;
}

int connect_gpio_irq1(void) {
  int status = XIntc_Connect(&axi_intc, XPAR_FABRIC_AXI_GPIO_1_INTR,
                             (XInterruptHandler)gpio_isr1, &gpSwitch);
  if (status == XST_SUCCESS)
    XIntc_Enable(&axi_intc, XPAR_FABRIC_AXI_GPIO_1_INTR);
  return status;
}

// ================== AXI CONNECTION ==================

void connect_axi(void) {
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
  if (status != XST_SUCCESS) {
    xil_printf("ERREUR: Erreur lors de la connexion des interruptions");
    return;
  }

  status |= connect_AXI_INTC_to_GIC();
  if (status != XST_SUCCESS) {
    xil_printf("Axi cnnexion to GIC FAILED\r\n");
    return;
  }
}

// ================== CLEANUP ==================

void cleanup(void) {
  XIntc_Disconnect(&axi_intc, XPAR_FABRIC_AXI_GPIO_0_INTR);
  XIntc_Disconnect(&axi_intc, XPAR_FABRIC_AXI_GPIO_1_INTR);
  // XIntc_Disconnect(&axi_intc, XPAR_FABRIC_AXI_TIMER_0_INTERRUPT_INTR);
}

void eanable_interruption() {
  initialize_gpio0();
  initialize_gpio1();
  // initialize_timer();
  initialize_axi_intc();
  connect_axi();
}

int connect_AXI_INTC_to_GIC() {
  int Status;

  // ---- Init GIC Interrupt Controller ----
  gic_config = XScuGic_LookupConfig(XPAR_SCUGIC_SINGLE_DEVICE_ID);
  if (gic_config == NULL) {
    xil_printf("GIC LookupConfig failed\r\n");
    return XST_FAILURE;
  }

  Status =
      XScuGic_CfgInitialize(&gic_intc, gic_config, gic_config->CpuBaseAddress);
  if (Status != XST_SUCCESS) {
    xil_printf("GIC CfgInitialize failed\r\n");
    return XST_FAILURE;
  }

  // Register GIC interrupt handler
  Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
                               (Xil_ExceptionHandler)XScuGic_InterruptHandler,
                               &gic_intc);

  Xil_ExceptionEnable();

  Status = XScuGic_Connect(&gic_intc, AXI_INTC_IRQ_ID,
                           (Xil_ExceptionHandler)XIntc_InterruptHandler,
                           (void *)&axi_intc);
  if (Status != XST_SUCCESS) {
    xil_printf("GIC Connect failed\r\n");
    return XST_FAILURE;
  }

  // Enable the interrupt in GIC
  XScuGic_Enable(&gic_intc, AXI_INTC_IRQ_ID);

  return XST_SUCCESS;
}
