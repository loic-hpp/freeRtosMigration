/*
*********************************************************************************************************
*                                                 uC/OS-III
*                                          The Real-Time Kernel
*                                               PORT Windows
*
*
*            		          					Guy BOIS
*                                  Polytechnique Montreal, Qc, CANADA
*                                                  07/2025
*
*
*********************************************************************************************************
*/

#include "routeur.h"

#include <inttypes.h>
#include <projdefs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <xparameters.h>

#include <xil_printf.h>

#include <stdio.h>

#include <xgpio.h>
#include <xil_exception.h>
#include <xintc.h>

#define LLONG_MAX 9223372036854775807
uint64_t freq_hz;

uint64_t max_delay_video = 0L;
uint64_t max_delay_audio = 0L;
uint64_t max_delay_autre = 0L;

// uint64_t ts_delta_mutex_acc = 0L;
// uint64_t ts_delta_sem_acc = 0L;

float max_delay_video_float;
float max_delay_audio_float;
float max_delay_autre_float;

float average_blocking_mutex = 0L;
float average_blocking_mutex_float;
float average_blocking_sem = 0L;
float average_blocking_sem_float;

// � utiliser pour suivre le remplissage et le vidage des fifos
// Mettre en commentaire et utiliser la fonction vide suivante si vous ne voulez
// pas de trace

#if FULL_TRACE == 1

#define safeprintf(fmt, ...)                                                   \
  do {                                                                         \
    if (mutPrint != NULL) {                                                    \
      if (xSemaphoreTake(mutPrint, portMAX_DELAY) == pdTRUE) {                 \
        xil_printf(fmt, ##__VA_ARGS__);                                        \
        xSemaphoreGive(mutPrint);                                              \
      }                                                                        \
    }                                                                          \
  } while (0)
#else
#define safeprintf(fmt, ...)                                                   \
  do {                                                                         \
  } while (0)
#endif

/*
*********************************************************************************************************
*                                                  MAIN
*********************************************************************************************************
*/

int main(void) {

  create_application();

  vTaskStartScheduler();

  while (1)
    ;
  return 0; // Start multitasking
}

void create_application() {
  int error;

  error = create_events();
  if (error != 0)
    xil_printf("Error %d while creating events\n", error);

  error = create_tasks();
  if (error != 0)
    xil_printf("Error %d while creating tasks\n", error);
}

int taskCreationErrorCheck(TaskHandle_t handle) {
  if (handle == NULL) {
    return -1;
  }

  return 0;
}

int create_tasks() {

  StartupTaskHandler = xTaskCreateStatic(
      StartupTask,         // Fonction de la tâche
      "StartUp Task",      // Nom (à des fins de debug)
      TASK_STK_SIZE,       // Taille de la pile (en mots, pas octets)
      NULL,                // Paramètre (équivalent à (void*)0)
      MaxTaskPrio,         // Priorité
      &StartupTaskStk[0u], // Pointeur vers la pile
      &StartupTaskTCB      // Pointeur vers le TCB statique
  );

  return taskCreationErrorCheck(StartupTaskHandler);
}

int create_events() {
  // Creation des semaphores
  Sem_MemBlock = xSemaphoreCreateCounting(10000, // Valeur maximale
                                          10000  // Valeur initiale
  );
  Sem = xSemaphoreCreateCounting(1, 0);
  vSemaphoreCreateBinary(SemTaskComputing);

  // Creation des mutex
  mutPrint = xSemaphoreCreateMutex();
  mutAlloc = xSemaphoreCreateMutex();
  mutTaskComputing = xSemaphoreCreateMutex();

  // Creation des files externes - va servir � la manipulation 2

  source_errQ = xQueueCreate(1024, sizeof(Packet *));
  crc_errQ = xQueueCreate(1024, sizeof(Packet *));
  TaskQueueingQ = xQueueCreate(1024, sizeof(Packet *));
  TaskStatsQ = xQueueCreate(1024, sizeof(Packet *));
  for (int i = 0; i < NB_FIFO; i++) {
    TaskComputingQ[i] = xQueueCreate(1024, sizeof(Packet *));
    TaskOutputPortQ[i] = xQueueCreate(1024, sizeof(Packet *));
  }

  RouterStatus = xEventGroupCreate();

  return 0;
}

void Update_TS(Packet *packet) {

  uint64_t delay;

  delay = xTaskGetTickCount() -
          packet->timestamp; // Valeur courante - valeur initiale

  if (delay < 0) {
    xil_printf("Attention overflow\n");
  }

  else {

    switch (packet->type) {
    case PACKET_VIDEO:
      if (delay > max_delay_video) {
        max_delay_video = delay;
      }
      break;

    case PACKET_AUDIO:
      if (delay > max_delay_audio) {
        max_delay_audio = delay;
      }
      break;

    case PACKET_AUTRE:
      if (delay > max_delay_autre) {
        max_delay_autre = delay;
      }
      break;

    default:
      break;
    }
  }
}

/*
 *********************************************************************************************************
 *											  Routine
 *d'interruptions
 *
 *********************************************************************************************************
 */

void fit_timer_isr0(void *p_int_arg, uint32_t source_cpu) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

#if DEBUG_ISR == 1
  xil_printf("------------------ FIT TIMER 0 -------------------\n");
#endif
  if (routerIsOn & !routerIsOnPause)
   xEventGroupSetBitsFromISR(
      RouterStatus,
      TASK_STATS_PRINT,
      &xHigherPriorityTaskWoken
  );

  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void fit_timer_isr1(void *p_int_arg, uint32_t source_cpu) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

#if DEBUG_ISR == 1
  xil_printf("------------------ FIT TIMER 1 -------------------\n");
#endif
}

void timer_isr(void *p_int_arg, uint32_t source_cpu) {
  xil_printf("---------------timer_isr---------------\n");
  // réinitialiser le timer
  XTmrCtr_Reset(&timer_dev, XPAR_AXI_TIMER_DEVICE_ID);
}

void gpio_isr0(void *p_int_arg, uint32_t source_cpu) {
#if DEBUG_ISR == 1
  xil_printf("---------------gpio_isr0---------------\n");
#endif

  int button_data = 0;
  u32 status = XGpio_InterruptGetStatus(&gpButton);
  button_data = XGpio_DiscreteRead(&gpButton, GPIO_BUTTONS_CHANNEL);
  TurnLEDButton(button_data);

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (button_data == BP0)
    xEventGroupSetBitsFromISR(RouterStatus, TASK_RESET_RDY,
                              &xHigherPriorityTaskWoken);
  else if (button_data == BP1)
    xEventGroupSetBitsFromISR(RouterStatus, TASK_STOP_RDY,
                              &xHigherPriorityTaskWoken);
  else if (button_data == BP2)
    xEventGroupSetBitsFromISR(RouterStatus, TASK_SHUTDOWN,
                              &xHigherPriorityTaskWoken);

  XGpio_InterruptClear(&gpButton, status);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void gpio_isr1(void *p_int_arg, uint32_t source_cpu) {
#if DEBUG_ISR == 1
  xil_printf("---------------gpio_isr1---------------\n");
#endif

  int switch_data = 0;
  u32 status = XGpio_InterruptGetStatus(&gpSwitch);
  switch_data = XGpio_DiscreteRead(&gpSwitch, GPIO_SWITCH_DEVICE_ID);
  TurnLEDSwitch(switch_data);
  XGpio_InterruptClear(&gpSwitch, status);
}

///////////////////////////////////////////////////////////////////////////////////////
//									TASKS
///////////////////////////////////////////////////////////////////////////////////////

/*
 *********************************************************************************************************
 *											  TaskGeneratePacket
 *  - G�n�re des paquets et les envoie dans la InputQ.
 *
 *
 *********************************************************************************************************
 */

void TaskGenerate(void *data) {
  srand(42);
  uint64_t ts;
  bool isGenPhase =
      false; // Indique si on est dans la phase de generation ou non
  int nb_rafales = 0;
  int packGenQty = (rand() % 255);
  while (true) {
    xEventGroupWaitBits(
        RouterStatus,      // Event group handle
        TASK_GENERATE_RDY, // Bits à attendre
        pdFALSE,      // Efface les bits après détection (comportement µC/OS)
        pdTRUE,       // Attendre que TOUS les bits soient SET
        portMAX_DELAY // Blocage infini (équivalent timeout = 0 µC/OS)
    );
    if (isGenPhase) {
      // Nouveau paquet

      Packet *packet = (Packet *)pvPortMalloc(sizeof(Packet));

      if (packet == NULL) {
        xil_printf("\nTaskGenerate: attention packet no %d est un NULL Pointer",
                   nbPacketCrees);
      };

      packet->src = rand() * (UINT32_MAX / RAND_MAX);
      packet->dst = rand() * (UINT32_MAX / RAND_MAX);
      packet->type = rand() % NB_PACKET_TYPE;

      for (int i = 0; i < ARRAY_SIZE(packet->data); ++i)
        packet->data[i] = (unsigned int)rand();

#if PERFORMANCE_TRACE == 1
      packet->timestamp = xTaskGetTickCount();
#endif

      packet->data[0] = ++nbPacketCrees;

      // Calcul du CRC avec injection de fautes
      packet->crc = 0;
      packet->crc = computeCRC((uint16_t *)(packet), sizeof(*packet));
      if (nbPacketCrees % 100 == 0)
        packet->crc++;

#if FULL_TRACE == 1
      xSemaphoreTake(mutPrint, portMAX_DELAY);
      xil_printf(
          "\nTaskGenerate : ********Generation du Paquet # %d ******** \n",
          nbPacketCrees);
      xil_printf("ADD %x \n", packet);
      xil_printf("	** id : %d \n", packet->data[0]);
      xil_printf("	** src : %x \n", packet->src);
      xil_printf("	** dst : %x \n", packet->dst);
      xil_printf("	** type : %d \n", packet->type);
      xSemaphoreGive(mutPrint);
#endif

      BaseType_t result;

      result = xQueueSendToBack(TaskQueueingQ, &packet,
                                0 // Timeout 0 : pas d'attente si queue pleine
      );

      if (result != pdPASS) {

        safeprintf("\nTaskGenerate: Paquet rejete du FIFO de TaskQueuing !\n");

#if FULL_TRACE == 1
        xQueueSendToBack(TaskStatsQ, &packet, 0);

#else
        vPortFree((void *)packet);
#endif
        nbPacketFIFOpleine++;
      }

      else
        safeprintf(
            "\nTaskGenenerate: nb de paquets dans fifo de TaskQueueing - "
            "apres production: %d \n",
            uxQueueMessagesWaiting(TaskQueueingQ));

      if ((nbPacketCrees % packGenQty) ==
          0) // On gen�re au maximum 255 paquets par phase de g�neration
      {
        safeprintf("\n***** TaskGenerate: FIN DE LA RAFALE No %d \n\n",
                   nb_rafales);
        isGenPhase = false;
      }
    } else {
      // vTaskDelay(delai_pour_vider_les_fifos_msec);
      isGenPhase = true;
      do {
        packGenQty = (rand() % 255);
      } while (packGenQty == 0);

      safeprintf("\n***** TaskeGenerate: RAFALE No %d DE %d PAQUETS DURANT LES "
                 "%d PROCHAINES MILLISECONDES\n\n",
                 ++nb_rafales, packGenQty, packGenQty);
      safeprintf("\n***** TaskGenerate: DEMARRAGE \n\n");
    }
  }
}

/*
 *********************************************************************************************************
 *											  TaskReset
 *
 *********************************************************************************************************
 */
void TaskReset(void *data) {
  xil_printf("--------------------- Task Reset --------------------\n");

  while (true) {
    xEventGroupWaitBits(RouterStatus,   // Event group handle
                        TASK_RESET_RDY, // Bits à attendre
                        pdTRUE,         // Efface les bits après détection
                        pdTRUE,         // Attendre que TOUS les bits soient SET
                        portMAX_DELAY   // Blocage infini
    );
    if (routerIsOnPause) {
      xil_printf("--------------------- System Resume --------------------\n");
      routerIsOnPause = 0;
    } else {
      xil_printf("--------------------- System Start --------------------\n");
      routerIsOn = 1;
      routerIsOnPause = 0;
    }

    xEventGroupSetBits(RouterStatus, TASKS_ROUTER);
  }
}

void TaskStop(void *data) {
  
  while (true) {
    xEventGroupWaitBits(RouterStatus,   // Event group handle
                        TASK_STOP_RDY, // Bits à attendre
                        pdTRUE,         // Efface les bits après détection
                        pdTRUE,         // Attendre que TOUS les bits soient SET
                        portMAX_DELAY   // Blocage infini
    );
    if (!routerIsOnPause && routerIsOn == 1) {
			routerIsOnPause = 1;
    xEventGroupSetBits(RouterStatus, TASK_STATS_PRINT);
			xil_printf("-------- System Temporarly Suspended ---------\n");
		}
  }
}

/*
 *********************************************************************************************************
 *                                            computeCRC
 * -Calcule la check value d'un pointeur quelconque (cyclic redudancy check)
 * -Retourne 0 si le CRC est correct, une autre valeur sinon.
 *********************************************************************************************************
 */

unsigned int computeCRC(uint16_t *w, int nleft) {

  unsigned int sum = 0;
  unsigned int nb_calls = 0;
  unsigned int Nb_of_ticks_in_CRC_init = 0;

  uint16_t answer = 0;

  // Code � compl�ter pour le calcul du nombre de ticks dans la manipulation 1

  // Adding words of 16 bits
  while (nleft > 1) {
    sum += *w++;
    nleft -= 2;
  }

  // Handling the last byte
  if (nleft == 1) {
    *(unsigned char *)(&answer) = *(const unsigned char *)w;
    sum += answer;
  }

  // Handling overflow
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);

  answer = ~sum;

  // Code � compl�ter pour le calcul du nombre de ticks dans la manipulation 1

  return answer;
}

/*
 *********************************************************************************************************
 *											  TaskQueeing
 *
 *********************************************************************************************************
 */
void TaskQueueing(void *pdata) {
  uint64_t ts;
  BaseType_t result;
  Packet *packet = NULL;
  uint64_t actualticks = 0;
  while (true) {
    xEventGroupWaitBits(RouterStatus, TASK_QUEUING_RDY, pdFALSE, pdTRUE,
                        portMAX_DELAY);
    xQueueReceive(TaskQueueingQ, &packet, portMAX_DELAY);
    safeprintf("\nTaskQueuing: nb de paquets apres consommation du fifo: %d \n",
               uxQueueMessagesWaiting(TaskQueueingQ));

    if (computeCRC((uint16_t *)packet, sizeof(*packet)) != 0) {

      safeprintf("\nTaskQueuing: Mauvais CRC !\n");

#if FULL_TRACE == 1
      xQueueSendToBack(TaskStatsQ, &packet, 0);
#else
      vPortFree((void *)packet);
#endif
      ++nbPacketMauvaisCRC;
    }

    else {

      // Dispatche les paquets selon leur type
      switch (packet->type) {
      case PACKET_VIDEO:
        result = xQueueSendToBack(TaskComputingQ[PACKET_VIDEO], &packet, 0);
        safeprintf("\nTaskQueueing: nb de paquets dans TaskComputing HIGHQ "
                   "apres production : %d \n",
                   uxQueueMessagesWaiting(TaskComputingQ[PACKET_VIDEO]));
        break;

      case PACKET_AUDIO:
        result = xQueueSendToBack(TaskComputingQ[PACKET_AUDIO], &packet, 0);
        safeprintf("\nTaskQueueing: nb de paquets dans TaskComputing MEDIUMQ "
                   "apres production : %d \n",
                   uxQueueMessagesWaiting(TaskComputingQ[PACKET_AUDIO]));
        break;

      case PACKET_AUTRE:
        result = xQueueSendToBack(TaskComputingQ[PACKET_AUTRE], &packet, 0);
        safeprintf("\nTaskQueueing: nb de paquets dans TaskComputing LOWQ "
                   "apres production: %d \n",
                   uxQueueMessagesWaiting(TaskComputingQ[PACKET_AUTRE]));
        break;

      default:
        safeprintf("\nTaskQueueing: Erreur sur la priorite du paquet - %d \n",
                   packet->data[0]);
#if FULL_TRACE == 1
        xQueueSendToBack(TaskStatsQ, &packet, 0);
#else
        vPortFree((void *)packet);
#endif
        nbPacketMauvaisePriorite++;
        break;
      }

      if (result != pdPASS) {
        safeprintf(
            "\nTaskQueueing: Paquet rejete de FIFO no %d de TaskComputing\n",
            packet->type);
#if FULL_TRACE == 1
        xQueueSendToBack(TaskStatsQ, &packet, 0);
#else
        vPortFree((void *)packet);
#endif
        nbPacketFIFOpleine++;
      }
    }
  }
}

/*
 *********************************************************************************************************
 *											  TaskComputing
 *  -V�rifie si les paquets sont conformes i.e. qu on emule un CRC et on verifie
 *l espace addresse -Dispatche les paquets dans des files (HIGH,MEDIUM,LOW)
 *
 *********************************************************************************************************
 */
void TaskComputing(void *pdata) {
  uint64_t ts;
  uint64_t t1, t2;
  BaseType_t result;
  Packet *packet = NULL;
  uint64_t actualticks = 0;
  Info_FIFO info = *(Info_FIFO *)pdata;
  int WAITFORTICKS;
  float nombre;
  while (true) {
    xEventGroupWaitBits(RouterStatus, TASK_COMPUTING_RDY, pdFALSE, pdTRUE,
                        portMAX_DELAY);

    xQueueReceive(TaskComputingQ[info.id], &packet, portMAX_DELAY);

    safeprintf(
        "\nTaskComputing %s: nb de paquets apres consommation du fifo: %d \n",
        info.name, uxQueueMessagesWaiting(TaskComputingQ[info.id]));

    // Verification de l'espace d'addressage
    if ((packet->src > REJECT_LOW1 && packet->src < REJECT_HIGH1) ||
        (packet->src > REJECT_LOW2 && packet->src < REJECT_HIGH2) ||
        (packet->src > REJECT_LOW3 && packet->src < REJECT_HIGH3)) {

      safeprintf("\nTaskComputing %s: paquet mauvaise source\n", info.name);

#if FULL_TRACE == 1
      xQueueSendToBack(TaskStatsQ, &packet, 0);
#else
      vPortFree((void *)packet);

#endif
      nbPacketMauvaiseSource++;

    }

    else { // we can start processing and forwarding

      // we may emulate a certain time for the processing and also emulate
      // priority inheritance

      switch (packet->type) {

      case PACKET_VIDEO:
#if MUTEX == 1
        t1 = xTaskGetTickCount();
        xSemaphoreTake(mutTaskComputing, portMAX_DELAY);
        t2 = xTaskGetTickCount();
        average_blocking_mutex = average_blocking_mutex + (t2 - t1);
        ++nbPacketTraites_Video;
#elif SEMAPHORE == 1
        t1 = xTaskGetTickCount();
        xSemaphoreTake(SemTaskComputing, portMAX_DELAY) t2 =
            xTaskGetTickCount();
        average_blocking_sem = average_blocking_sem + (t2 - t1);
        ++nbPacketTraites_Video;
#endif

#if DELAI_0_1 == 1
        WAITFORTICKS = (rand() % 2);
        actualticks = xTaskGetTickCount();
        while (WAITFORTICKS + actualticks > xTaskGetTickCount())
          ;
#elif DELAI_1_2 == 1
        do {
          WAITFORTICKS = (rand() % 3);
        } while (WAITFORTICKS == 0);
        actualticks = xTaskGetTickCount();
        while (WAITFORTICKS + actualticks > xTaskGetTickCount())
          ;
#endif

#if MUTEX == 1
        xSemaphoreGive(mutTaskComputing);
#elif SEMAPHORE == 1
        xSemaphoreGive(SemTaskComputing)
#endif
        break;

      case PACKET_AUDIO:

#if DELAI_0_1 == 1
        WAITFORTICKS = (rand() % 2);
        actualticks = xTaskGetTickCount();
        while (WAITFORTICKS + actualticks > xTaskGetTickCount())
          ;
#elif DELAI_1_2 == 1
        do {
          WAITFORTICKS = (rand() % 3);
        } while (WAITFORTICKS == 0);
        actualticks = xTaskGetTickCount();
        while (WAITFORTICKS + actualticks > xTaskGetTickCount())
          ;
#endif
        break;

      case PACKET_AUTRE:
#if MUTEX == 1
        xSemaphoreTake(mutTaskComputing, portMAX_DELAY);
#elif SEMAPHORE == 1
        xSemaphoreTake(SemTaskComputing, portMAX_DELAY)
#endif

#if DELAI_0_1 == 1
        WAITFORTICKS = (rand() % 2);
        actualticks = xTaskGetTickCount();
        while (WAITFORTICKS + actualticks > xTaskGetTickCount())
          ;
#elif DELAI_1_2 == 1
            do {
          WAITFORTICKS = (rand() % 3);
        }
        while (WAITFORTICKS == 0)
          ;
        actualticks = xTaskGetTickCount();
        while (WAITFORTICKS + actualticks > xTaskGetTickCount())
          ;
#endif

#if MUTEX == 1
        xSemaphoreGive(mutTaskComputing);
#elif SEMAPHORE == 1
        xSemaphoreGive(SemTaskComputing)
#endif
        break;

      default:
        xil_printf("\nATTENTION Paquet non identifi/ dans TaskComputing \n");
        break;
      };

      /* Test sur la destination du paquet */
      if (packet->dst >= INT1_LOW && packet->dst <= INT1_HIGH) {

        safeprintf("\nTaskComputing %s: paquet envoye dans interface 1\n",
                   info.name);
#if PERFORMANCE_TRACE == 1
        Update_TS(packet);
#endif
        result = xQueueSendToBack(TaskOutputPortQ[PACKET_VIDEO], &packet, 0);
      } else {
        if (packet->dst >= INT2_LOW && packet->dst <= INT2_HIGH) {
          safeprintf("\nTaskComputing %s: paquet envoye dans interface 2\n",
                     info.name);
#if PERFORMANCE_TRACE == 1
          Update_TS(packet);
#endif
          result = xQueueSendToBack(TaskOutputPortQ[PACKET_AUDIO], &packet, 0);
        } else {
          if (packet->dst >= INT3_LOW && packet->dst <= INT3_HIGH) {
            safeprintf("\nTaskComputing %s: paquet envoye dans interface 3\n",
                       info.name);
#if PERFORMANCE_TRACE == 1
            Update_TS(packet);
#endif
            result =
                xQueueSendToBack(TaskOutputPortQ[PACKET_AUTRE], &packet, 0);
          } else {
            if (packet->dst >= INT_BC_LOW && packet->dst <= INT_BC_HIGH) {
              Packet *others[2];
              int i;
              for (i = 0; i < ARRAY_SIZE(others); ++i) {
                others[i] = (Packet *)pvPortMalloc(sizeof(Packet));
                memcpy(others[i], packet, sizeof(Packet));
              }
              safeprintf("\nTaskComputing %s: paquet BC arrive dans tous les "
                         "interfaces\n",
                         info.name);
#if PERFORMANCE_TRACE == 1
              Update_TS(packet);
              for (i = 0; i < ARRAY_SIZE(others); ++i) {
                Update_TS(others[i]);
              }
#endif
              result =
                  xQueueSendToBack(TaskOutputPortQ[PACKET_VIDEO], &packet, 0);
              result = xQueueSendToBack(TaskOutputPortQ[PACKET_AUDIO],
                                        &others[0], 0);
              result = xQueueSendToBack(TaskOutputPortQ[PACKET_AUTRE],
                                        &others[1], 0);
            }
          }
        }
      }

      if (result != pdPASS) {
        /*Destruction du paquet si la mailbox de destination est pleine*/

        safeprintf("\nTaskComputing %s: paquet rejete d'un des fifo de "
                   "TaskOutputPort!\n",
                   info.name);
#if FULL_TRACE == 1
        xQueueSendToBack(TaskStatsQ, &packet, 0);
#else
        vPortFree((void *)packet);
#endif
        nbPacketFIFOpleine++;
      } else {
        ++nbPacketTraites;
      }
    }
  }
}

/*
 *********************************************************************************************************
 *											  TaskPrint
 *  -Affiche les infos des paquets arriv�s � destination et libere la m�moire
 *allou�e
 *********************************************************************************************************
 */
void TaskOutputPort(void *data) {
  uint64_t ts;
  Packet *packet = NULL;
  Info_Port info = *(Info_Port *)data;

  while (1) {
    xEventGroupWaitBits(RouterStatus, TASK_OUTPUTPORT_RDY, pdFALSE, pdTRUE,
                        portMAX_DELAY);

    /*Attente d'un paquet*/
    xQueueReceive(TaskOutputPortQ[info.id], &packet, portMAX_DELAY);

#if FULL_TRACE == 1
    xSemaphoreTake(mutPrint, portMAX_DELAY);
    xil_printf("\nPaquet recu en %d \n", info.id);
    xil_printf("	** id : %d \n", packet->data[0]);
    xil_printf("    >> src : %x \n", packet->src);
    xil_printf("    >> dst : %x \n", packet->dst);
    xil_printf("    >> type : %d \n", packet->type);
    xSemaphoreGive(mutPrint);
#endif
    /*Lib�ration de la m�moire*/
    vPortFree((void *)packet);
  }
}

/*
 *********************************************************************************************************
 *                                              TaskStats
 *  -Est d�clench�e lorsque le gpio_isr() lib�re le s�maphore
 *  -Lorsque d�clench�e, imprime les statistiques du routeur � cet instant
 *********************************************************************************************************
 */
void TaskStats(void *pdata) {
  uint64_t ts;
  Packet *packet = NULL;

  while (1) {
    xEventGroupWaitBits(RouterStatus, TASK_STATS_RDY, pdFALSE, pdTRUE,
                        portMAX_DELAY);
    xEventGroupWaitBits(RouterStatus, TASK_STATS_PRINT, pdTRUE, pdTRUE,
                        portMAX_DELAY);                   

    xSemaphoreTake(mutPrint, portMAX_DELAY);

    xil_printf("\n------------------ Affichage des statistiques "
               "------------------\n\n");
    xil_printf("Delai pour vider les fifos msec: %d\n",
               delai_pour_vider_les_fifos_msec);
    xil_printf("Frequence du systeme: %d\n", configTICK_RATE_HZ);

#if MUTEX == 1
    xil_printf("Mode mutex ");
#elif SEMAPHORE == 1
    xil_printf("Mode semaphore ");
#else
    xil_printf("Pas de section critique ");
#endif

    xil_printf("\r\n");

#if DELAI_0_1 == 1
    xil_printf("DELAI_0_1");
#elif DELAI_1_2 == 1
    xil_printf("DELAI_1_2");
#else
    xil_printf("Pas d attente active");
#endif
    xil_printf("\r\n");

    xil_printf("1 - Nb de packets total crees : %d\n", nbPacketCrees);
    xil_printf("2 - Nb de packets total traites : %d\n", nbPacketTraites);

    nbPacketRejetes = nbPacketMauvaisCRC + nbPacketMauvaiseSource +
                      nbPacketFIFOpleine + nbPacketMauvaisePriorite;
    xil_printf("3 - Nb de packets rejetes pour mauvaise source : %d\n",
               nbPacketMauvaiseSource);
    xil_printf("4 - Nb de packets rejetes pour mauvaise source total: %d\n",
               nbPacketMauvaiseSourceTotal);
    xil_printf("5 - Nb de packets rejetes pour mauvais CRC : %d\n",
               nbPacketMauvaisCRC);
    xil_printf("6 - Nb de packets rejetes pour mauvais CRC total : %d\n",
               nbPacketMauvaisCRCTotal);
    xil_printf("7 - Nb de paquets rejetes fifo : %d\n", nbPacketFIFOpleine);
    xil_printf("8 - Nb de paquets rejetes fifo total : %d\n",
               nbPacketFIFOpleineTotal);
    xil_printf("9 - Nb de paquets rejetes mauvaise priorites : %d\n",
               nbPacketMauvaisePriorite);
    xil_printf("10 - Nb de paquets rejetes mauvaise priorites total : %d\n",
               nbPacketMauvaisePrioriteTotal);
    xil_printf("11 - Nb de paquets maximum dans le fifo de Queueing : %d \n",
               uxQueueMessagesWaiting(TaskQueueingQ));
    xil_printf(
        "12 - Nb de paquets maximum dans le fifo HIGHQ de TaskComputing: %d \n",
        uxQueueMessagesWaiting(TaskComputingQ[PACKET_VIDEO]));
    xil_printf(
        "13 - Nb de paquets maximum dans fifo MEDIUMQ de TaskComputing: %d \n",
        uxQueueMessagesWaiting(TaskComputingQ[PACKET_AUDIO]));
    xil_printf(
        "14 - Nb de paquets maximum dans fifo LOWQ de TaskComputing: %d \n",
        uxQueueMessagesWaiting(TaskComputingQ[PACKET_AUTRE]));
    xil_printf("15 - Nb de paquets maximum dans port0 : %d \n",
               uxQueueMessagesWaiting(TaskOutputPortQ[0]));
    xil_printf("16 - Nb de paquets maximum dans port1 : %d \n",
               uxQueueMessagesWaiting(TaskOutputPortQ[1]));
    xil_printf("17 - Nb de paquets maximum dans port2 : %d \n\n",
               uxQueueMessagesWaiting(TaskOutputPortQ[2]));
    // xil_printf("18- Message free : %d \n", OSMsgPool.NbrFree);
    // xil_printf("19- Message used : %d \n", OSMsgPool.NbrUsed);
    // xil_printf("20- Message used max : %d \n", OSMsgPool.NbrUsedMax);
    xil_printf("21- Nombre de ticks depuis le d�but de l'execution %d \n",
               xTaskGetTickCount());

    xSemaphoreGive(mutPrint);
#if PERFORMANCE_TRACE == 1

    max_delay_video_float = (float)max_delay_video / (float)freq_hz;
    double val = (double)max_delay_video / (double)freq_hz;
    int int_part = (int)val;
    int frac_part = (int)((val - int_part) * 1000000000.0);
    if (frac_part < 0)
      frac_part = -frac_part;
    xil_printf("22- Pire temps video  ");
    xil_printf("'%d.%09d'", int_part, frac_part);
    xil_printf("\r\n");

    max_delay_audio_float = (float)max_delay_audio / (float)freq_hz;
    val = (double)max_delay_audio / (double)freq_hz;
    int_part = (int)val;
    frac_part = (int)((val - int_part) * 1000000000.0);
    if (frac_part < 0)
      frac_part = -frac_part;
    xil_printf("23- Pire temps audio  ");
    xil_printf("'%d.%09d'", int_part, frac_part);
    xil_printf("\r\n");

    max_delay_autre_float = (float)max_delay_autre / (float)freq_hz;
    val = (double)max_delay_autre / (double)freq_hz;
    int_part = (int)val;
    frac_part = (int)((val - int_part) * 1000000000.0);
    if (frac_part < 0)
      frac_part = -frac_part;
    xil_printf("24- Pire temps autre  ");
    xil_printf("'%d.%08d'", int_part, frac_part);
    xil_printf("\r\n");

#if MUTEX == 1
    xil_printf("25- Attente de blocage moyen pour le mutex :");
    average_blocking_mutex_float = (float)average_blocking_mutex;
    average_blocking_mutex_float =
        average_blocking_mutex_float / (float)nbPacketTraites_Video;
    val = (double)average_blocking_mutex_float / (double)freq_hz;
    int_part = (int)val;
    frac_part = (int)((val - int_part) * 1000000000.0);
    if (frac_part < 0)
      frac_part = -frac_part;
    xil_printf("'%d.%09d' pour %d packets videos traites", int_part, frac_part,
               nbPacketTraites_Video);
    xil_printf("\r\n");
#elif SEMAPHORE == 1
    xil_printf("25- Attente de blocage moyen pour le semaphore :");
    average_blocking_sem_float = (float)average_blocking_sem;
    average_blocking_sem_float =
        average_blocking_sem_float / (float)nbPacketTraites_Video;
    val = (double)average_blocking_sem_float / (double)freq_hz;
    int_part = (int)val;
    frac_part = (int)((val - int_part) * 1000000000.0);
    if (frac_part < 0)
      frac_part = -frac_part;
    xil_printf("'%d.%09d' pour %d packets videos traites", int_part, frac_part,
               nbPacketTraites_Video);
    xil_printf("\r\n");
#endif

#endif
    // On vide la fifo des paquets rejet�s et on imprime si l option est
    // demandee
#if FULL_TRACE == 1

    while (1) {
      xQueueReceive(TaskStatsQ, &packet, 0);

      if (packet == NULL) {
        break;
      } else {

        if (print_paquets_rejetes) {
          xSemaphoreTake(mutPrint, portMAX_DELAY);
          xil_printf("    >> paquet rejete # : %d \n", packet->data[0]);
          xil_printf("    >> src : %x \n", packet->src);
          xil_printf("    >> dst : %x \n", packet->dst);
          xil_printf("    >> type : %d \n", packet->type);
          xSemaphoreGive(mutPrint);
        }

        vPortFree((void *)packet);

        packet = NULL;
      };
    };

#endif

    nbPacketMauvaisCRCTotal += nbPacketMauvaisCRC;
    nbPacketMauvaisCRC = 0;

    nbPacketMauvaiseSourceTotal += nbPacketMauvaiseSource;
    nbPacketMauvaiseSource = 0;

    nbPacketFIFOpleineTotal += nbPacketFIFOpleine;
    nbPacketFIFOpleine = 0;

    nbPacketMauvaisePrioriteTotal += nbPacketMauvaisePriorite;
    nbPacketMauvaisePriorite = 0;

    nbPacketRejetesTotal =
        nbPacketMauvaisCRCTotal + nbPacketMauvaiseSourceTotal +
        nbPacketFIFOpleineTotal + nbPacketMauvaisePrioriteTotal;

    // // On stoppe tout le programme quand on a atteint la limite de paquets
    // if (nbPacketCrees > limite_de_paquets)
    //   xSemaphoreGive(Sem);
  }
}

void err_msg(char *entete, uint8_t err) {
  if (err != 0) {
    xil_printf(entete);
    xil_printf(": Une erreur est retourn�e : code %d \n", err);
  }
}

/*
*********************************************************************************************************
*                                               STARTUP TASK
*********************************************************************************************************
*/

void StartupTask(void *p_arg) {
  printf("Initialisation interruptions - \n");
  eanable_interruption();
  printf("Initialisation des interruptions terminées - \n");

  printf("Programme initialise\r\n");

  printf("Frequence courante du tick d horloge - %d\r\n", configTICK_RATE_HZ);

  // freq_hz = CPU_TS_TmrFreqGet(&err); /* Get CPU timestamp timer frequency. */
  freq_hz = configTICK_RATE_HZ;
  xil_printf("\nfreq du timestamp: %d\n", freq_hz);

  // On cr�e les t�ches

  for (int i = 0; i < NB_FIFO; i++) {
    switch (i) {
    case 0:
      FIFO[i].id = PACKET_VIDEO;
      FIFO[i].name = "HighQ";
      break;
    case 1:
      FIFO[i].id = PACKET_AUDIO;
      FIFO[i].name = "MediumQ";
      break;
    case 2:
      FIFO[i].id = PACKET_AUTRE;
      FIFO[i].name = "LowQ";
      break;
    default:
      break;
    };
  }

  for (int i = 0; i < NB_OUTPUT_PORTS; i++) {
    Port[i].id = i;
    switch (i) {
    case 0:
      Port[i].name = "Port 0";
      break;
    case 1:
      Port[i].name = "Port 1";
      break;
    case 2:
      Port[i].name = "Port 2";
      break;
    default:
      break;
    };
  }

  XTmrCtr_Start(&timer_dev, XPAR_AXI_TIMER_DEVICE_ID);

  int err = 0;
  TaskgenerateHandler = xTaskCreateStatic(
      TaskGenerate,   // Fonction tâche
      "TaskGenerate", // Nom de la tâche (debug)
      TASK_STK_SIZE,  // Taille pile en mots (attention: pas TASK_STK_SIZE / 2)
      NULL,           // Paramètre passé à la tâche
      TaskGeneratePRIO, // Priorité (entre 0 et configMAX_PRIORITIES-1)
      TaskGenerateSTK,  // Pointeur pile (tableau)
      &TaskGenerateTCB  // Pointeur TCB statique
  );
  err |= taskCreationErrorCheck(TaskgenerateHandler);

  TaskQueueingHandler =
      xTaskCreateStatic(TaskQueueing, "TaskQueueing", TASK_STK_SIZE, NULL,
                        TaskQueueingPRIO, TaskQueueingSTK, &TaskQueueingTCB);
  err |= taskCreationErrorCheck(TaskQueueingHandler);

  for (int i = 0; i < NB_FIFO; i++) {
    TaskComputingHandler[i] = xTaskCreateStatic(
        TaskComputing, "TaskComputing", TASK_STK_SIZE, &FIFO[i],
        TaskComputingPRIO - i, TaskComputingSTK[i], &TaskComputingTCB[i]);
    err |= taskCreationErrorCheck(TaskComputingHandler[i]);
  }

  for (int i = 0; i < NB_OUTPUT_PORTS; i++) {
    TaskOutputPortHandler[i] = xTaskCreateStatic(
        TaskOutputPort, "OutputPort", TASK_STK_SIZE, &Port[i],
        TaskOutputPortPRIO, TaskOutputPortSTK[i], &TaskOutputPortTCB[i]);
    err |= taskCreationErrorCheck(TaskOutputPortHandler[i]);
  }

  TaskStatsHandler =
      xTaskCreateStatic(TaskStats, "TaskStats", TASK_STK_SIZE, NULL,
                        TaskStatsPRIO, TaskStatsSTK, &TaskStatsTCB);
  err |= taskCreationErrorCheck(TaskStatsHandler);

  TaskResetHandler =
      xTaskCreateStatic(TaskReset, "TaskReset", TASK_STK_SIZE, NULL,
                        TaskResetPRIO, TaskResetSTK, &TaskResetTCB);
  err |= taskCreationErrorCheck(TaskResetHandler);

  TaskStopHandler = xTaskCreateStatic(TaskStop, "TaskStop", TASK_STK_SIZE, NULL,
                                      TaskStopPRIO, TaskStopSTK, &TaskStopTCB);
  err |= taskCreationErrorCheck(TaskStopHandler);

  xEventGroupWaitBits(RouterStatus, TASK_SHUTDOWN, pdTRUE, pdTRUE,
                      portMAX_DELAY);
  xil_printf("Prepare to shutdown System - \r\n");
  xEventGroupClearBits(RouterStatus, TASKS_ROUTER);
  TickType_t xDelay = pdMS_TO_TICKS(1000);
  while (1) {
    TurnLEDButton(0b1111);
    vTaskDelay(xDelay);
    TurnLEDButton(0b0000);
    vTaskDelay(xDelay);
  }
}
