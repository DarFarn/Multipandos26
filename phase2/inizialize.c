#include "../headers/listx.h"
#include "../headers/const.h"
#include "../headers/types.h"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>


int processCount; 			//numero di processi iniziati ma non terminati 
int softblockcount;			//numero di processi bloccati
LIST_HEAD(readyQueue);      
pcb_t* current_process;    //puntatore al processo corrente
semd_t device_semaphores; //QUANTI DEVO FARNE




