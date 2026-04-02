#include "../headers/const.h"
#include "../headers/types.h"
#include <uriscv/liburiscv.h>
#include "../phase1/headers/pcb.h"
#include "../phase1/headers/asl.h"
#include "../headers/listx.h"
#include <uriscv/types.h>
#include <uriscv/const.h>
#include "../headers/klog.h"
#include "scheduler.h"

extern void scheduler();

//External Nucleus global variables
extern int processCount;
extern int softblockcount;
extern LIST_HEAD(readyQueue);
extern pcb_t *current_process;
extern int device_semaphores[NRSEMAPHORES];

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
//Linee 3-6: 4 linee * 8 devices --> 32 semafoti (0-31)
//Ogni Terminal Device ha 2 semafori --> a 8 terminal corrispondono 16 semafori (32-47)
int getSemaphoreIndex(int intLineNo, int devNo, int isTransmitter) {
    if (intLineNo == 7) {
        return 32 + (devNo * 2) + (isTransmitter ? 1 : 0);
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
    
    /* For terminal devices, check transmitter priority */
    if (isTerminal) {
        unsigned int termBase = TERM_REG_BASE + (devNo * DEV_REG_SIZE);
        unsigned int *transmStatus = (unsigned int *)(termBase + TRANSM_STATUS_OFFSET);
        unsigned int *recvStatus = (unsigned int *)(termBase + RECV_STATUS_OFFSET);
        
        /* Transmitter has higher priority than receiver */
        if ((*transmStatus & STATUSMASK) != 0) {
            isTransmitter = 1;
            statusReg = (unsigned int *)(termBase + TRANSM_STATUS_OFFSET);
            commandReg = (unsigned int *)(termBase + TRANSM_COMMAND_OFFSET);
        } else {
            isTransmitter = 0;
            statusReg = (unsigned int *)(termBase + RECV_STATUS_OFFSET);
            commandReg = (unsigned int *)(termBase + RECV_COMMAND_OFFSET);
        }
    } else {
        //Caloclo device register adress
        devRegAddr = getDeviceRegAddr(intLineNo, devNo);
        statusReg = (unsigned int *)(devRegAddr + STATUS_OFFSET);
        commandReg = (unsigned int *)(devRegAddr + COMMAND_OFFSET);
    }
    
   //Salvataggio status code
    statusCode = *statusReg & STATUSMASK;
    
    //Acknowledgement dell'interrupt
    *commandReg = ACK;
    
    // Operazione V sul device semaphore
    semIndex = getSemaphoreIndex(intLineNo, devNo, isTransmitter);
    unblockedProc = removeBlocked(device_semaphores[semIndex]);
    
    //Sbloccare un processo se ce ne sono di bloccati
    if (unblockedProc != NULL) {
        // Inseriemnto status code nel registro a0 del PCB appena sbloccato
        unblockedProc->p_s.regs[REG_A0] = statusCode;
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
void handlePLTInterrupt(void) {
    //Acknowledge PLT interrupt mettendo nel timer un nuovo valore
    setTIMER(TIMESLICE);
    
    //Copriare stato CPU nel PCB del Current Process
    state_t *exceptionState = (state_t *)BIOSDATAPAGE;
    current_process->p_s = *exceptionState;
    
    //Aggiornemento dell'accumulated CPU time
    cpu_t currentTime;
    STCK(currentTime);
    currentProcess->p_time += (currentTime - processStartTime); 
    
    //Inserimento Current Process nella ReadyQueue
    insertProcQ(&readyQueue, current_process);
    current_process = NULL;
    
    scheduler();
}
