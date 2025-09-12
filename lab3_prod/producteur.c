/*
*********************************************************************************************************
*                                                 uC/OS-III
*                                          The Real-Time Kernel
*                                               PORT Windows
*
*
*            		          					Guy BOIS
*                                  Polytechnique Montreal, Qc, CANADA
*                                                  07/2020
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

// Attention ce programme loge � l'adresse 0x1000000 et possede une dimension de
// 0x1000000 V�rifier lsscript.ld

// Comme producteur est maitre (UCOS_AMP_MASTER doit etre a true dans le BSP
// setting)

// Attention ce programma va partager avec consommateur.c une zone de m�moire
const uint32_t BASEADDR = 0x3000000;

#define TASK_STK_SIZE 8192 // Size of each task's stacks (# of WORDs)

#define TASK_PRODUCER_PRIO 15 // Priorit� de TaskActive

/*
*********************************************************************************************************
*                                              VARIABLES
*********************************************************************************************************
*/

static StackType_t Task_ProducerSTK[TASK_STK_SIZE];
static StaticTask_t TaskProducerTCB;

/*
 *********************************************************************************************************
 *                                         FUNCTION PROTOTYPES
 *********************************************************************************************************
 */

void TaskProducer(void *data);

/*
*********************************************************************************************************
*                                                  MAIN
*********************************************************************************************************
*/

int main(void) {
    TaskHandle_t task_handler =
        xTaskCreateStatic(TaskProducer, "TaskProducer", TASK_STK_SIZE, NULL,
                          TASK_PRODUCER_PRIO, Task_ProducerSTK,
                          &TaskProducerTCB);

  if (task_handler == NULL) {
    xil_printf("Error when creating the task\r\n");
    return -1;
  }

  Xil_DCacheDisable();
  vTaskStartScheduler();

  return 0; // Start multitasking
}

void TaskProducer(void *p_arg) {

  // Variables partagees entre core 0 et core 1 (shared memory de 3 mots de 32
  // bits � partir de BASEADDR) Les 2 premiers mots servent � la synchronisation
  // et le dernier au data echange Attention ack de TaskProducer doit �tre
  // jumel� avec req de TaskConsommateur
  //        et req de TaskProducer doit �tre jumel� avec ack de
  //        TaskConsommateur

  xil_printf("UCOS - UCOS init done\r\n");
  xil_printf("Programme initialise - \r\n");

  volatile uint32_t *req =
      (uint32_t *)(BASEADDR +
                   0x0); // signal comme quoi on est pr�t � envoyer un data
  volatile uint32_t *ack =
      (uint32_t *)(BASEADDR + 0x4); // signal comme quoi on attend que le
                                    // consommateur soit pret
  volatile uint32_t *ptr = (uint32_t *)(BASEADDR + 0x8); // data partagee

  xil_printf("Task Producteur sur core 1\r\n");

  // le producteur initialise le protocole
  *req = 0;
  *ack = 0;
  int i = 0;

  while (1) {

    *ptr = i++; // On ecrie i++

    // vTaskDelay(pdMS_TO_TICKS(
    //     1000)); // on put supposer que c'est le delai pour produire
                // d ailleurs ca aurait pu �tre une atente active...

    xil_printf("Je viens de deposer la valeur de i: %d\n\r", *ptr);

    while (!*ack)
      ; // J'attends le signal du consommateur

    *req = 1; // J'avertie le consommateur qu un data est pret

    while (*ack)
      ; // J'attends que le consommateur est fini de consommer

    *req = 0; // On passe � la prochaine valeur � consommer
  }
}
