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


extern void test();
extern void uTLB_RefillHandler();
extern void exceptionHandler(); // penso vada bene (alice)
extern void scheduler();

int processCount;   // numero di processi iniziati ma non terminati
int softblockcount; // numero di processi bloccati
struct list_head readyQueue;
pcb_t *current_process;                 // puntatore al processo corrente
int device_semaphores[NRSEMAPHORES]; 
cpu_t startTOD;
pcb_t *activeProcs[MAXPROC]; // array che contiene i puntatori a tutti i processi attivi, da 0 a MAXPROC-1, se un indice è null allora il processo è terminato


int main(void)
{

    memaddr position;
    RAMTOP(position);
    //klog_print("ramtop:");
    //klog_print_hex(position);


    
    passupvector_t *passupvector = (passupvector_t *)PASSUPVECTOR;

    passupvector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
    passupvector->tlb_refill_stackPtr = KERNELSTACK + 0x4;
    passupvector->exception_handler = (memaddr)exceptionHandler;

    passupvector->exception_stackPtr = KERNELSTACK; // da ricontrollare se va bene, non vorrei sovrascrivere roba importante

    initPcbs();
    initASL();
    processCount = 0;
    softblockcount = 0;
    mkEmptyProcQ(&readyQueue);
    current_process = NULL;

    for (int i = 0; i <= PSEUDOCLOCK_INDEX; i++)
    {
        device_semaphores[i] = 0; // semafori inizializzati a 0 OCCHIO!!!!!!
    }
    STCK(startTOD);
    for (int i = 0; i < MAXPROC; i++) {
        activeProcs[i] = NULL; // inizializzo l'array dei processi attivi a null
    }
    LDIT(PSECOND);
    pcb_t *p = allocPcb(); // allochiamo il primo pcb

    /// setto i parametri di p a null
    INIT_LIST_HEAD(&p->p_child); // da ricontrollare che sia il modo giusto di settarli
    INIT_LIST_HEAD(&p->p_sib);

    p->p_parent = NULL;
    p->p_time = 0;
    p->p_semAdd = NULL;
    p->p_supportStruct = NULL;

    p->p_s.pc_epc = (memaddr)test;                     // PC
    p->p_s.status = MSTATUS_MIE_MASK | MSTATUS_MPIE_MASK | MSTATUS_MPP_M; // kernel mode + interrupt
    p->p_s.mie = MIE_ALL;                              // abilita interrupt
    p->p_s.reg_sp = position - (10 * PAGESIZE);              // in teoria stack pointer caricato in ramtop
    insertProcQ(&readyQueue, p);                       // mettoin ready queue
    processCount = 1;  
    
    //testing
    p->p_s.status = MSTATUS_MIE_MASK | MSTATUS_MPIE_MASK | MSTATUS_MPP_M;
    activeProcs[0] = p; // inserisco il processo appena creato nell'array dei processi attivi


    //klog_print("MPP_M:");
    //klog_print_hex(MSTATUS_MPP_M);
    //klog_print("MPIE:");
    //klog_print_hex(MSTATUS_MPIE_MASK);
    //klog_print("pcb status:");
    //klog_print_hex(p->p_s.status);                                  // incrementa contatore processi

    scheduler();

    return 0;
}
