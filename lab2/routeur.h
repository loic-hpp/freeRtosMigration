/*
 * router.h
 *
 *  Created on: 26 July 2020
 *      Author: Guy BOIS
 */

#ifndef SRC_ROUTEUR_H_EXT_
#define SRC_ROUTEUR_H_EXT_


#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "masque_lies_aux_ISRs.h"
#include "interruptions.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define TASK_STK_SIZE 8192

/* ************************************************
 *                TASK PRIOS
 **************************************************/

#define			 TaskStatsPRIO 					22
#define          TaskStopPRIO     				21
#define          TaskResetPRIO     				20
#define          TaskOutputPortPRIO     		15
#define          TaskGeneratePRIO   			14
#define          TaskQueueingPRIO  				13
#define          TaskComputingPRIO 				12
#define          MaxTaskPrio                    (configMAX_PRIORITIES - 1)



#define			 WAITFORComputing				1
#define          DEBUG_ISR                      1

// Commandes a la compilation
#define 		 FULL_TRACE 					0
#define 		 PERFORMANCE_TRACE				1
// Les 4 prochains define impliquent PERFORMANE_TRACE == 1
// Dans ce qui suit MUTEX et SEMAPHORE a 0 indique pas de section critique
#define 		 MUTEX							1
#define 		 SEMAPHORE						0
// Choisir 1 attente active parmi les 2 suivants:
#define 		 DELAI_0_1						1
#define 		 DELAI_1_2						0

// Routing info.

#define NB_OUTPUT_PORTS 3
#define NB_FIFO 3

#define INT1_LOW      0x00000000
#define INT1_HIGH     0x3FFFFFFF
#define INT2_LOW      0x40000000
#define INT2_HIGH     0x7FFFFFFF
#define INT3_LOW      0x80000000
#define INT3_HIGH     0xBFFFFFFF
#define INT_BC_LOW    0xC0000000
#define INT_BC_HIGH   0xFFFFFFFF

// Reject source info.
#define REJECT_LOW1   0x10000000
#define REJECT_HIGH1  0x17FFFFFF
#define REJECT_LOW2   0x50000000
#define REJECT_HIGH2  0x57FFFFFF
#define REJECT_LOW3   0xD0000000
#define REJECT_HIGH3  0xD7FFFFFF

// Evenements lies aux taches
#define TASK_GENERATE_RDY  		0x01
#define TASK_QUEUING_RDY  		0x02
#define TASK_COMPUTING_RDY  	0x04
#define TASK_OUTPUTPORT_RDY  	0x08
#define TASK_STATS_RDY  		0x10

#define PACKET_VIDEO  			0
#define PACKET_AUDIO  			1
#define PACKET_AUTRE  			2
#define NB_PACKET_TYPE 			3
const int TASK_PARAM[] = {0,1,2};
// Mask
#define TASKS_ROUTER			0x01F     // Permet de demarrer ou stopper toutes les taches au meme moment

EventGroupHandle_t RouterStatus = NULL;

SemaphoreHandle_t 	Sem;
SemaphoreHandle_t 	Sem_MemBlock;
SemaphoreHandle_t 	SemTaskComputing;

typedef struct{
	int id;
	char* name;
} Info_Port;

typedef struct{
	int id;
	char* name;
} Info_FIFO;

Info_Port  	Port[NB_OUTPUT_PORTS];
Info_FIFO 	FIFO[NB_FIFO];


typedef struct {
    unsigned int src;
    unsigned int dst;
    unsigned int type;
    uint64_t timestamp;
    unsigned int crc;
    unsigned int data[10];
} Packet;						//16 mots de 32 bits par paquet


// Stacks
static StackType_t  TaskGenerateSTK[TASK_STK_SIZE];
static StackType_t  TaskQueueingSTK[TASK_STK_SIZE];
static StackType_t  TaskComputingSTK[NB_FIFO][TASK_STK_SIZE];
static StackType_t  TaskOutputPortSTK[NB_OUTPUT_PORTS][TASK_STK_SIZE];
static StackType_t  TaskStatsSTK[TASK_STK_SIZE];
static StackType_t  TaskResetSTK[TASK_STK_SIZE];
static StackType_t  TaskStopSTK[TASK_STK_SIZE];
static StackType_t  StartupTaskStk[TASK_STK_SIZE];



static StaticTask_t  TaskGenerateTCB;
static StaticTask_t  TaskStatsTCB;
static StaticTask_t  TaskQueueingTCB;
static StaticTask_t  TaskComputingTCB[NB_FIFO];
static StaticTask_t  TaskOutputPortTCB[NB_OUTPUT_PORTS];
static StaticTask_t  TaskResetTCB;
static StaticTask_t  TaskStopTCB;
static StaticTask_t  StartupTaskTCB;


/* ************************************************
 *                  Handler des tâches
 **************************************************/
TaskHandle_t TaskgenerateHandler;
TaskHandle_t TaskStatsHandler;
TaskHandle_t TaskQueueingHandler;
TaskHandle_t TaskComputingHandler[NB_FIFO];
TaskHandle_t TaskOutputPortHandler[NB_OUTPUT_PORTS];
TaskHandle_t TaskResetHandler;
TaskHandle_t TaskStopHandler;
TaskHandle_t StartupTaskHandler;

/* ************************************************
 *                  Queues
 **************************************************/

QueueHandle_t crc_errQ;
QueueHandle_t source_errQ;
QueueHandle_t TaskQueueingQ;
QueueHandle_t TaskComputingQ[NB_FIFO];
QueueHandle_t TaskOutputPortQ[NB_FIFO];
QueueHandle_t TaskStatsQ;


/* ************************************************
 *                  Semaphores
 **************************************************/

// Pas de sémaphore pour la partie 1 

/* ************************************************
 *                  Mutexes
 **************************************************/
SemaphoreHandle_t  		mutRejete;
SemaphoreHandle_t  		mutAlloc;
SemaphoreHandle_t  		mutPrint;
SemaphoreHandle_t  		mutTaskComputing;
QueueHandle_t   		BLockMem;
uint32_t  		*Tab_Block[10000][sizeof(Packet)];     /* 10000 packets of 16 words of 32 bits */


/*DECLARATION DES COMPTEURS POUR STATISTIQUES*/
int nbPacketCrees = 0;
int nbPacketTraites = 0;
int nbPacketRejetes = 0;
int nbPacketRejetesTotal = 0;
int nbPacketFIFOpleine = 0;
int nbPacketFIFOpleineTotal = 0;
int nbPacketMauvaiseSource = 0;
int nbPacketMauvaiseSourceTotal = 0;
int nbPacketMauvaisCRC =0;
int nbPacketMauvaisCRCTotal =0;
int nbPacketMauvaisePriorite =0;
int nbPacketMauvaisePrioriteTotal = 0;
int nbPacketTraites_Video = 0;
int routerIsOn = 0;
int routerIsOnPause = 0;



 const TickType_t delai_pour_vider_les_fifos_msec = pdMS_TO_TICKS(50); // la valeur entière ici est dictement le nombre de ms
int print_paquets_rejetes = 0;
int limite_de_paquets= 100000;

/* ************************************************
 *              TASK PROTOTYPES
 **************************************************/

void TaskGenerate(void *data); 
void TaskComputing(void *data);
void TaskForwarding(void *data);
void TaskOutputPort(void *data);
void TaskStats(void* data);
void StartupTask(void *data);

void dispatch_packet (Packet* packet);

void create_application();
int create_tasks();
int create_events();
void err_msg(char* ,uint8_t);
void Suspend_Delay_Resume_All(int nb_sec);
unsigned int computeCRC(uint16_t* w, int nleft);
int taskCreationErrorCheck(TaskHandle_t handle);

#endif /* SRC_ROUTEUR_H_EXT_ */
