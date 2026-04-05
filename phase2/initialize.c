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
void print(char *msg);
extern void uTLB_RefillHandler();
extern void exceptionHandler(); // penso vada bene (alice)
extern void scheduler();

int processCount;   // numero di processi iniziati ma non terminati
int softblockcount; // numero di processi bloccati
LIST_HEAD(readyQueue);
pcb_t *current_process;                 // puntatore al processo corrente
int device_semaphores[NRSEMAPHORES]; // occhiooooo?????????

int main()
{
    
    passupvector_t *passupvector = (passupvector_t *)PASSUPVECTOR;

    passupvector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
    passupvector->tlb_refill_stackPtr = (memaddr)KERNELSTACK;
    passupvector->exception_handler = (memaddr)exceptionHandler;

    passupvector->exception_stackPtr = (memaddr)KERNELSTACK;

    initPcbs();
    initASL();
    processCount = 0;
    softblockcount = 0;
    mkEmptyProcQ(&readyQueue);
    current_process = NULL;

    for (int i = 0; i < NRSEMAPHORES; i++)
    {
        device_semaphores[i] = 0; // semafori inizializzati a 0 OCCHIO!!!!!!
    }

    volatile unsigned int *interval_timer = (volatile unsigned int *)INTERVALTMR;
    *interval_timer = PSECOND;
    pcb_t *p = allocPcb(); // allochiamo il primo pcb

    /// setto i parametri di p a null
    INIT_LIST_HEAD(&p->p_child); // da ricontrollare che sia il modo giusto di settarli
    INIT_LIST_HEAD(&p->p_sib);

    p->p_parent = NULL;
    p->p_time = 0;
    p->p_semAdd = NULL;
    p->p_supportStruct = NULL;

    p->p_s.pc_epc = (memaddr)test;                     // PC
    p->p_s.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M; // kernel mode + interrupt
    p->p_s.mie = MIE_ALL;                              // abilita interrupt
    RAMTOP(p->p_s.reg_sp);                 // in teoria stack pointer caricato in ramtop
    insertProcQ(&readyQueue, p);                       // mettoin ready queue
    processCount++;  
    
    //testing
    p->p_s.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M;

    klog_print("MPP_M:");
    klog_print_hex(MSTATUS_MPP_M);
    klog_print("MPIE:");
    klog_print_hex(MSTATUS_MPIE_MASK);
    klog_print("pcb status:");
    klog_print_hex(p->p_s.status);                                  // incrementa contatore processi

    scheduler();

    return 0;
}
