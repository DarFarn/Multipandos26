//implements test 

//inizializzerà:
#include "../headers/listx.h"
#include "../headers/const.h"
#include "../headers/types.h"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include <uriscv/const.h>
#include "../headers/klog.h"
#include "../phase2/headers/exceptions.h"
#include "../phase2/headers/scheduler.h"

//definisco page table

typedef struct page_table[]{
for (i=0, i++, i <= 31){
pagetable[i].vpn = 0x80000 + i;
pagetable[i].asid = sup_asid; // da capire come scriverlo correttamente 
pagetable[i].D = 1;
pagetable[i].G = 1;
pagetable[i].V = 0;
}
pagetable[31].vpn = 0xBFFFF;
//9.1.2.

typedef struct support_t{
int sup_asid; //sarà l'ASID del processo stesso
context_t sup_exceptContext[2];
sup_exceptContext[0]->c_pc = //address del TLB_handler di questo livello;
sup_exceptContext[1]->c_pc = //address dell'exception handler generale
sup_exceptContext[0]->c_status = MSTATUS_MPP_M;
sup_exceptContext[1]->c_status = MSTATUS_MPP_M;
sup_exceptContext[0]->c_stackPtr = &(sup_stackGen[500]);
sup_exceptContext[1]->c_status = &(sup_stackGen[500]);
page_table sup_privatePgTbl[32];
//queste tre richiedono tutte inizializzazione precedente al lancio, capire per le prime due;
state_t sup_exceptState[2]; 
sup_stackTLB[500];
sup_stackGen[500];
}

//quali altre strutture e/o operazoni vanno in questo header? probabilmente tutte quelle su semafori?












