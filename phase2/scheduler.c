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


void scheduler (){
    //In its simplest form whenever the Scheduler is called it should dispatch the “next” process in the Ready Queue.
    //1. Remove the PCB from the head of the Ready Queue and store the pointer to the PCB in the
    //Current Process field.
    
    klog_print("SCHEDULER STARTED\n");
    current_process = removeProcQ(&readyQueue);
    
    // inizia il mio crazy talks
    if(current_process == NULL){
        klog_print("READY QUEUE EMPTY\n");
        if(processCount == 0){
            klog_print("NO PROCESSES TO RUN, HALTING\n");
            HALT();
        }
        if(processCount > 0 && softblockcount == 0){
            PANIC();
        }
        if(processCount > 0 && softblockcount > 0){
            setMIE(MIE_ALL & ~MIE_MTIE_MASK);
            unsigned int status = getSTATUS();
            status |= MSTATUS_MIE_MASK;
            setSTATUS(status);
            WAIT();
            scheduler(); // dopo il wait, quando viene svegliato, richiama scheduler per riprendere l'esecuzione
            return;
        }    
    }
    // fine crazy talk


    
    cpu_t now;
    STCK(now);
    processStartTime = now; 
    //2. Load 5 milliseconds on the PLT [Section 7.2].
    //LDIT(5);
    setTIMER(TIMESLICE); // to check, fatto da alice


    //3. Perform a Load Processor State (LDST) [Section 13.2] on the processor state stored in PCB of
    //the Current Process (p_s) of the current CPU.

    //come verificare che sia della current cpu

    LDST(&current_process->p_s);   
    
/*

    //Dispatching a process transitions it from a “ready” process to a “running” process
    //??
    
    //The Scheduler should behave in the following manner if the Ready Queue is empty:
    
    if (emptyProcQ(&readyQueue)){            
    
    
    //1. If the Process Count is 0, invoke the HALT BIOS service/instruction [Section 13.2]. Consider this a job well done
        if (processCount == 0){
                HALT();
        }        
        //Deadlock for PandOSsh is defined as when the Process Count > 0 and the Soft-block Count is
        //zero. Take an appropriate deadlock detected action; invoke the PANIC BIOS service/instruction
        //[Section 13.2].
    
        if( processCount > 0 && softblockcount == 0){
            PANIC();
        }
        
        
        //If the Process Count > 0 and Soft-block Count > 0 enter a Wait State. 
        
        if (processCount > 0 && softblockcount > 0){
            WAIT();
        } 
    }      
    


    //Important: Before executing the WAIT instruction, the Scheduler must first set the mie register to enable interrupts and either disable the PLT (also through the mie register) using:
    setMIE(MIE_ALL & ~MIE_MTIE_MASK);
    unsigned int status = getSTATUS();
    status |= MSTATUS_MIE_MASK;
    setSTATUS(status);
    WAIT();
    
    LDST((state_t*) BIOSDATAPAGE);
*/    
}
