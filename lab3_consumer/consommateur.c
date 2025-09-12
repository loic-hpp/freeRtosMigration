/*
*********************************************************************************************************
*                                                 uC/OS-III
*                                          The Real-Time Kernel
*                                               PORT Windows
*
*
*            		          					Guy BOIS
*                                  Polytechnique Montreal, Qc, CANADA
*
*
*
*********************************************************************************************************
*/

#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/*
*********************************************************************************************************
*                                              CONSTANTS
*********************************************************************************************************
*/

// Attention ce programme loge � l'adresse 0x2000000 et possede une dimension de
// 0x1000000 V�rifier lsscript.ld

// Comme consommateur n est pas maitre (UCOS_AMP_MASTER doit etre a false)

// Attention ce programma va partager avec consommateur.c une zone de m�moire
const uint32_t BASEADDR = 0x3000000;

#define TASK_STK_SIZE 8192 // Size of each task's stacks (# of WORDs)

#define TASK_CONSUMER_PRIO 14 // Priorit� de TaskActive

/*
*********************************************************************************************************
*                                              VARIABLES
*********************************************************************************************************
*/

static StackType_t TaskConsumerSTK[TASK_STK_SIZE];
static StaticTask_t TaskConsumerTCB;
// MEM_SEG_INFO seg_info;

/*
 *********************************************************************************************************
 *                                         FUNCTION PROTOTYPES
 *********************************************************************************************************
 */

void TaskConsumer(void *data);

/*
*********************************************************************************************************
*                                                  MAIN
*********************************************************************************************************
*/

int main(void) {

  TaskHandle_t task_handler = xTaskCreateStatic(
      TaskConsumer, "TaskConsumer", TASK_STK_SIZE, NULL, TASK_CONSUMER_PRIO,
      TaskConsumerSTK, &TaskConsumerTCB);

  if (task_handler == NULL) {
    xil_printf("Error when creating the task\r\n");
    return -1;
  }

  Xil_DCacheDisable();
  vTaskStartScheduler();

  return 0; // Start multitasking
}

void TaskConsumer(void *p_arg) {
  xil_printf("UCOS - uC/OS Init Started.\r\n");
  xil_printf("UCOS - STDIN/STDOUT Device Initialized.\r\n");
  xil_printf("UCOS - UCOS init done\r\n");
  xil_printf("Programme initialise - \r\n");
  xil_printf("Frequence courante du tick d'horloge - %d\r\n",
             configTICK_RATE_HZ);

  // Variables partagees entre core 0 et core 1 (shared memory de 3 mots de 32
  // bits � partir de BASEADDR) Les 2 premiers mots servent � la synchronisation
  // et le dernier au data echange Attention ack de TaskProducteur doit �tre
  // jumel� avec req de TaskConsommateur
  //        et req de TaskProducteur doit �tre jumel� avec ack de
  //        TaskConsommateur

  volatile uint32_t *req =
      (uint32_t *)(BASEADDR +
                   0x4); // signal comme quoi on est pr�t � recevoir un data
  volatile uint32_t *ack =
      (uint32_t *)(BASEADDR + 0x0); // signal comme quoi on attend que le
                                    // producteur est ecrit un data
  volatile uint32_t *ptr = (uint32_t *)(BASEADDR + 0x8); // data partagee

  xil_printf("Task Consommateur sur core 0\r\n");

//   *req = 0;
//   *ack = 0;

  while (1) {

    *req = 1; // Je suis pret a consommer

    while (!*ack)
      ; // J'attends que le producteur est ecrit une donnee

    xil_printf("Je viens de recevoir la valeur de i: %d\n\r", *ptr);

    *req = 0; // J'ai fini de consommer

    while (*ack)
      ; // J'attends que le producteur me dise de passer � une prochaine valeur
        // � consommer

    vTaskDelay(pdMS_TO_TICKS(1000)); // Dans le lab 1, on peut
                                     // supposer que c'est le delai
                                     // pour vider les fifos...
  }
}
