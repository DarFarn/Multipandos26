#include "../headers/const.h"
#include "../headers/types.h"
#include <uriscv/liburiscv.h>
#include "../phase1/headers/pcb.h"
#include "../phase1/headers/asl.h"
#include "../headers/listx.h"
#include <uriscv/types.h>
#include <uriscv/const.h>
#include "../headers/klog.h"
#include "../phase2/headers/scheduler.h"
#include "../phase2/headers/initialize.h"

extern void scheduler();

//External Nucleus global variables
extern int processCount;
extern int softblockcount;
extern struct list_head readyQueue;
extern pcb_t *current_process;
extern int device_semaphores[];
extern cpu_t processStartTime;

//Device register base addresses
#define INTERRUPT_BIT_MAP 0x10000040 
#define DEVICE_REG_BASE 0x10000054 
#define TERM_REG_BASE 0x10000254 //indirizzi memoria Terminal Devices (Line 7)

//Offset dei device registers
#define STATUS_OFFSET 0x0 
#define COMMAND_OFFSET 0x4 

//Offset dei Terminal Devices
#define RECV_STATUS_OFFSET  0x0
#define RECV_COMMAND_OFFSET 0x4
#define TRANSM_STATUS_OFFSET 0x8
#define TRANSM_COMMAND_OFFSET 0xC

//Costanti
#define DEVICES_PER_LINE 8 //40 device / 5 device interrupt lines
#define DEV_REG_SIZE 0x80 //size di una interrupt line
#define STATUSMASK 0xFF


#define ACK 1
#define PRINTCHR 2
#define CHAROFFSET 8

#define PLT_INT_LINE 1 //Serve per time slices
#define INTERVAL_TIMER_LINE 2 //serve per pseudo-clock da 100ms ticks


//Calcolo dell'inidirizzo del device’s device register
unsigned int getDeviceRegAddr(int intLineNo, int devNo) {
    return DEVICE_REG_BASE + ((intLineNo - 3) * DEV_REG_SIZE) + (devNo * 0x10);
}

//Funzione che trova il device con proprità più alta. Ritorna -1 se non c'è
int getHighestPriorityDevice(unsigned int bitMap) {
    int devNo;
    for (devNo = 0; devNo < DEVICES_PER_LINE; devNo++) {
        if (bitMap & (1 << devNo)) {
            return devNo;
        }
    }
    return -1;
}

//Semaphore index per un device
//Linee 3-6: 4 linee * 8 devices --> 32 semafori (0-31)
//Ogni Terminal Device ha 2 semafori --> a 8 terminal corrispondono 16 semafori (32-47)
int getSemaphoreIndex(int intLineNo, int devNo, int isTransmitter) {
    if (intLineNo == 7) {
        return isTransmitter ? (32 + devNo) : (40 + devNo);
    } else {
        int lineOffset = intLineNo - 3; //Linee 3-6
        return (lineOffset * DEVICES_PER_LINE) + devNo;
    }
}


//Parte 7.1 Non-Timer Interrupts
void handleNonTimerInterrupt(int intLineNo, unsigned int bitMap) {
    int devNo;
    unsigned int devRegAddr;
    unsigned int *statusReg;
    unsigned int *commandReg;
    unsigned int statusCode;
    pcb_t *unblockedProc;
    int semIndex;
    int isTerminal = (intLineNo == 7);
    int isTransmitter = 0;
    
    //Trovare il device con priorità maggiore
    devNo = getHighestPriorityDevice(bitMap);
    if (devNo < 0) {
        return;  //Non è stato trovato nessun dispositivo
    }
    if (isTerminal) {
    unsigned int termBase = getDeviceRegAddr(intLineNo, devNo);
    unsigned int *transmStatus = (unsigned int *)(termBase + TRANSM_STATUS_OFFSET);
    unsigned int *recvStatus   = (unsigned int *)(termBase + RECV_STATUS_OFFSET);

    if ((*transmStatus & STATUSMASK) != READY) {
        isTransmitter = 1;
        statusReg  = (unsigned int *)(termBase + TRANSM_STATUS_OFFSET);
        commandReg = (unsigned int *)(termBase + TRANSM_COMMAND_OFFSET);
    } else if ((*recvStatus & STATUSMASK) != READY) {
        isTransmitter = 0;
        statusReg  = (unsigned int *)(termBase + RECV_STATUS_OFFSET);
        commandReg = (unsigned int *)(termBase + RECV_COMMAND_OFFSET);
    } else {
        return;  // spurious terminal interrupt, nothing pending
    }
} 
    else {
        //Caloclo device register adress
        devRegAddr = getDeviceRegAddr(intLineNo, devNo);
        statusReg = (unsigned int *)(devRegAddr + STATUS_OFFSET);
        commandReg = (unsigned int *)(devRegAddr + COMMAND_OFFSET);
    }
    
   //Salvataggio status code
    statusCode = *statusReg; // & STATUSMASK
    
    //Acknowledgement dell'interrupt
    *commandReg = ACK;
    
    // Operazione V sul device semaphore
    semIndex = getSemaphoreIndex(intLineNo, devNo, isTransmitter);
    device_semaphores[semIndex]++; // MAYYYYYBE????????
    unblockedProc = removeBlocked(&device_semaphores[semIndex]);
    
    //Sbloccare un processo se ce ne sono di bloccati
    if (unblockedProc != NULL) {
        // Inseriemnto status code nel registro a0 del PCB appena sbloccato
        unblockedProc->p_s.reg_a0 = statusCode;
        unblockedProc->p_semAdd = NULL; //Processo non più bloccato 
        softblockcount--;
        
        //Inserimento del PCB nella RadyQueue
        insertProcQ(&readyQueue, unblockedProc);
    }
    
    //Return Current Process o chiama lo scheduler
    if (current_process != NULL) {
        state_t *exceptionState = (state_t *)BIOSDATAPAGE;
        LDST(exceptionState);
    } else {
        scheduler();
    }
}


//Parte 7.2 Processor Local Timer (PLT) Interrupts

void handlePLTInterrupt() {
    setTIMER(TIMESLICE * (*((cpu_t *) TIMESCALEADDR)));
    state_t *exceptionState = (state_t *)BIOSDATAPAGE;
    klog_print("PLT saving status:");
    klog_print_hex(exceptionState->status);
    current_process->p_s = *exceptionState;
    current_process->p_s.status |= MSTATUS_MIE_MASK; // Assicura che gli interrupt siano abilitati quando il processo riprende


    cpu_t currentTime;
    STCK(currentTime);
    current_process->p_time += (currentTime - processStartTime);

    insertProcQ(&readyQueue, current_process);
    current_process = NULL;
    scheduler();
}



//Parte 7.3 The System-wide Interval Timer and the Pseudo-clock
void handleIntervalTimerInterrupt() {
    /* Acknowledge interrupt con reload di Interval Timer */
    LDIT(PSECOND);
    
    //Ublocking PCB waiting for Pseudo-clock tick 
    pcb_t *unblockedProc;
    
    while ((unblockedProc = removeBlocked(&device_semaphores[PSEUDOCLOCK_INDEX])) != NULL) {
        unblockedProc->p_s.reg_a0 = 0;  //status code
        unblockedProc->p_semAdd = NULL;
        softblockcount--;
        insertProcQ(&readyQueue, unblockedProc);
    }
    
    /* Return to Current Process or call Scheduler */
    if (current_process != NULL) {
        state_t *exceptionState = (state_t *)BIOSDATAPAGE;
        LDST(exceptionState);
    } else {
        scheduler();
    }
}


//Non ho ben capito, chat
//Find highest priority pending interrupt 

/*int findHighestPriorityInterrupt(unsigned int *bitMap) {
    unsigned int cause = getCAUSE();
    int intLineNo = 0;
    
    /* Extract exception code using GETEXECCODE (0x7C = bits 2-6) */
    /* CAUSE_EXCCODE_MASK is not defined in your const.h, so use GETEXECCODE */
    //unsigned int excCode = cause & GETEXECCODE;
    
    // Interrupt Line map Tabella 1
    /*switch (excCode) {
        case 7:  intLineNo = 1; break;  
        case 3:  intLineNo = 2; break;  
        case 17: intLineNo = 3; break;  
        case 18: intLineNo = 4; break;  
        case 19: intLineNo = 5; break;  
        case 20: intLineNo = 6; break;  
        case 21: intLineNo = 7; break;  
        default: return 0;  //Sconosciuto
    }
        */
    
    /* For device interrupts (lines 3-7), read the Interrupting Devices Bit Map */
    //if (intLineNo >= 3) {
        /* Bit Map starts at 0x10000040, each line is 4 bytes */
    //            unsigned int *bitMapAddr = (unsigned int *)(INTERRUPT_BIT_MAP + ((intLineNo - 3) * 4));
    //    *bitMap = *bitMapAddr;
        
        /* Spurious interrupt: line indicated but no device bit set */
    //    if (*bitMap == 0) {
    //        return 0;
    //    }
//    }
    
 //   return intLineNo;
//}

//altro crazy talk
int findHighestPriorityInterrupt(unsigned int *bitMap) {
    unsigned int cause = getCAUSE();
    int intLineNo = 0;

    // Check in priority order: lowest line number = highest priority
    if (CAUSE_IP_GET(cause, IL_CPUTIMER))       intLineNo = 1;
    else if (CAUSE_IP_GET(cause, IL_TIMER))     intLineNo = 2;
    else if (CAUSE_IP_GET(cause, IL_DISK))      intLineNo = 3;
    else if (CAUSE_IP_GET(cause, IL_FLASH))     intLineNo = 4;
    else if (CAUSE_IP_GET(cause, IL_ETHERNET))  intLineNo = 5;
    else if (CAUSE_IP_GET(cause, IL_PRINTER))   intLineNo = 6;
    else if (CAUSE_IP_GET(cause, IL_TERMINAL))  intLineNo = 7;
    else return 0;

    if (intLineNo >= 3) {
        unsigned int *bitMapAddr = (unsigned int *)(INTERRUPT_BIT_MAP + ((intLineNo - 3) * 4));
        *bitMap = *bitMapAddr;
        if (*bitMap == 0) return 0;
    }

    return intLineNo;
}



//Main Interrupt Exception Handler
//Chimato da exceptionHandler quando CAUSE_IS_INT è true
void interruptHandler() {
    unsigned int bitMap = 0;
    int intLineNo;
    
    //Trova interrupt con priorità più alta
    intLineNo = findHighestPriorityInterrupt(&bitMap);
    
    if (intLineNo == 0) {
        //se l'interrupt non esiste
        return;
    }
    
    switch (intLineNo) {
        case PLT_INT_LINE:
            handlePLTInterrupt();
            break;
            
        case INTERVAL_TIMER_LINE:
            handleIntervalTimerInterrupt();
            break;
            
        default:
            //Device interrupts (lines 3-7)
            handleNonTimerInterrupt(intLineNo, bitMap);
            break;
    }
}