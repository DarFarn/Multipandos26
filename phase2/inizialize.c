#include "../headers/listx.h"
#include "../headers/const.h"
#include "../headers/types.h"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include <uriscv/const.h>

extern void test();
extern void uTLB_RefillHandler();
extern unsigned int exceptionHandler();

int processCount; 			//numero di processi iniziati ma non terminati 
int softblockcount;			//numero di processi bloccati
LIST_HEAD(readyQueue);      
pcb_PTR current_process;    //puntatore al processo corrente
semd_t device_semaphores[NRSEMAPHORES];  //occhiooooo?????????

int main(){ 
    
        
        
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
    
    for (int i = 0; i <= NRSEMAPHORES; i++){
        device_semaphores[i].s_key = 0; //semafori inizializzati a 0 OCCHIO!!!!!! 
    }
    
    volatile unsigned int *interval_timer = (volatile unsigned int *)INTERVALTMR;
    *interval_timer = PSECOND; 
    pcb_t *p = allocPcb();                              //allochiamo il primo pcb
    p->p_s.pc_epc = (memaddr)test;                      // PC
    p->p_s.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M;  // kernel mode + interrupt
    p->p_s.mie = MIE_ALL;                               // abilita interrupt
    RAMTOP(p->p_s.gpr[STATE_GPR_LEN]);                  //in teoria stack pointer caricato in ramtop
    insertProcQ(&readyQueue, p);                  // mettoin ready queue
    processCount++;                                     // incrementa contatore processi
    
    
    return 0;
    
}


    

