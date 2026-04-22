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

extern int processCount;
extern int softblockcount; 
extern struct list_head readyQueue;
extern struct pcb_t* current_process;
cpu_t processStartTime; // forse va inizializzato a 0, da vedere!!!!!!!!!!!!!!!!!!!!!!!!
extern pcb_t *activeProcs[MAXPROC]; // array che contiene i puntatori a tutti i processi attivi, da 0 a MAXPROC-1, se un indice è null allora il processo è terminato

void scheduler(void) { 
    klog_print("ciao sei nello scheduler :)\n");
     /* 1. Se c’è un processo ready lo eseguiamo */

    /* 1. Se c’è un processo ready lo eseguiamo */
    if (!emptyProcQ(&readyQueue)) {
      //  klog_print("Processo pronto, dispatching\n");
        current_process = removeProcQ(&readyQueue);
        setTIMER(TIMESLICE * (*((cpu_t *) TIMESCALEADDR)));
        LDST(&current_process->p_s);
        int pid = current_process->p_pid;
        klog_print("PID del processo dispatchato:");
        klog_print_dec(pid);
    }

    /* 2. Se non ci sono più processi HALT del sistema */
    if (processCount == 0) {

        HALT();
    }

    /* 3. Se ci sono processi soft-blocked facciamo WAIT */
    if (softblockcount > 0) {
        klog_print("Nessun processo pronto, ma ci sono processi bloccati, facendo WAIT\n");
        current_process = NULL;

        setMIE(MIE_ALL & ~MIE_MTIE_MASK);
        unsigned int status = getSTATUS();
        status |= MSTATUS_MIE_MASK;
        setSTATUS(status);

        WAIT();
    }

    if (emptyProcQ(&readyQueue) && processCount > 0 && softblockcount == 0) {
        // cerca un processo attivo
        for (int i = 0; i < MAXPROC; i++) {
            if (activeProcs[i] != NULL) {
                current_process = activeProcs[i];
                LDST(&current_process->p_s);
            }
        }
        // se non troviamo nulla siamo in DEADLOCK
       
        HALT();
    }
    if (processCount == 0) {
        
        HALT();
    }
    PANIC(); /* Non dovrebbe mai arrivarci */
}


