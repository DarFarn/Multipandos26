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
#include "../phase2/headers/exceptions.h"

extern void scheduler();
extern void updateCPUtime();

//External Nucleus global variables
extern int processCount;
extern int softblockcount;
extern struct list_head readyQueue;
extern pcb_t *current_process;
extern int device_semaphores[];
extern cpu_t processStartTime;
extern cpu_t startTOD;

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
unsigned int* getDeviceRegAddr(int intLineNo, int devNo) {
    return ((unsigned int *) (DEVICE_REG_BASE + ((intLineNo) - 3) * DEV_REG_SIZE + (devNo) * 0x10));
}

//Funzione che trova il device con proprità più alta. Ritorna -1 se non c'è
int getHighestPriorityDevice(unsigned int bitMap) {
   for(int i = 0; i < 5; i++) {
       if(bitMap & (1u << i)) {
           return i;
       }
   }
   return -1; // No interrupt pending
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

/*
//Parte 7.1 Non-Timer Interrupts
void handleNonTimerInterrupt(int intLineNo, unsigned int bitMap) {
    klog_print("Handling non-timer interrupt on line:");
    klog_print_dec(intLineNo);
    klog_print(" with bitMap:");
    klog_print_hex(bitMap);
    int devNo = getHighestPriorityDevice(bitMap);
    klog_print("Device number with highest priority interrupt:");
    klog_print_dec(devNo);
    unsigned int devRegAddr;
    unsigned int *statusReg;
    unsigned int *commandReg;
    unsigned int statusCode;
    pcb_t *unblockedProc;
    int semIndex;
    int isTerminal = (intLineNo == 7);
    int isTransmitter = 0;
    
    //Trovare il device con priorità maggiore
    
    if (devNo < 0) { // succede questo man non dovrebbe
        // vuol dire che devno è calcolato male
        klog_print("nessun dispositivo trovato con bitMap:");
        if(current_process != NULL) {
            state_t *exceptionState = (state_t *)BIOSDATAPAGE;
            LDST(exceptionState);
        } else {
            scheduler();
        }
        return;  //Non è stato trovato nessun dispositivo
    }
    if (isTerminal) {
        unsigned int *termBase = getDeviceRegAddr(intLineNo, devNo);
        unsigned int *transmStatus = (unsigned int *)(termBase + TRANSTATUS); // DAC CONTROLLARE 
        unsigned int *recvStatus   = (unsigned int *)(termBase + RECVSTATUS); //DA CONTROLLARE
        // transmission ha priorità su reception
        if ((*transmStatus & STATUSMASK) != READY && (*transmStatus & STATUSMASK) != BUSY) {
            klog_print("Terminal transmitter interrupt detected\n");
            statusReg  = (unsigned int *)(termBase + TRANSM_STATUS_OFFSET);
            unsigned int savedStatus = *statusReg;  // spurious terminal interrupt, nothing pending
            isTransmitter = 1;
            commandReg = (unsigned int *)(termBase + TRANSM_COMMAND_OFFSET);
            *commandReg = ACK;
            klog_print("Terminal transmitter acknowledged, now checking if any process is waiting on this device\n");
            semIndex = 32 + devNo; // semaforo del transmitter
            klog_print("semaforo da cui sbloccare processo:");
            klog_print_dec(semIndex);
            klog_print("semaphore value before increment:");
            klog_print_dec(device_semaphores[semIndex]);
            if(device_semaphores[semIndex] <= 0){
                klog_print("Processo/i in attesa sul semaforo del transmitter, sbloccando...\n");
                device_semaphores[semIndex]++;
                pcb_t *unblockedProc = removeBlocked(&device_semaphores[semIndex]);
                klog_print("valore del semaforo dopo incremento:");
                klog_print_dec(device_semaphores[semIndex]);
                
                if (unblockedProc != NULL) {
                    klog_print("Processo trovato, sbloccando e aggiungendo alla ready queue\n");
                    unblockedProc->p_s.reg_a0 = savedStatus;  // status code
                    unblockedProc->p_semAdd = NULL; //Processo non più bloccato 
                    softblockcount--;
                    list_add_tail(&unblockedProc->p_list, &readyQueue);
                    klog_print("Processo unblocked and added to ready queue\n");
                }
            }
        }
        if((*recvStatus & STATUSMASK) != READY && (*recvStatus & STATUSMASK) != BUSY){
            klog_print("Terminal receiver interrupt detected\n");
             statusReg  = (unsigned int *)(termBase + RECV_STATUS_OFFSET);
             unsigned int savedStatus = *statusReg;  // spurious terminal interrupt, nothing pending
             isTransmitter = 0;
             commandReg = (unsigned int *)(termBase + RECV_COMMAND_OFFSET);
             *commandReg = ACK;
             semIndex = 40 + devNo; // semaforo del receiver
            if(device_semaphores[semIndex] <= 0){
                device_semaphores[semIndex]++;
                pcb_t *unblockedProc = removeBlocked(&device_semaphores[semIndex]);
                if (unblockedProc != NULL) {
                    unblockedProc->p_s.reg_a0 = savedStatus;  // status code
                    unblockedProc->p_semAdd = NULL; //Processo non più bloccato 
                    softblockcount--;
                    insertProcQ(&readyQueue, unblockedProc);
                }
            }
        }
    }    
    else{
        klog_print("Non-terminal device interrupt detected\n");
        unsigned int *devBase = getDeviceRegAddr(intLineNo, devNo);
        unsigned int savedStatus = *devBase + STATUS_OFFSET;
        commandReg = (unsigned int *)(devBase + COMMAND_OFFSET);
        *commandReg = ACK;
        semIndex = (((intLineNo) -3)* DEVICES_PER_LINE + (devNo));
        if(device_semaphores[semIndex] <= 0){
            device_semaphores[semIndex]++;
            pcb_t *unblockedProc = removeBlocked(&device_semaphores[semIndex]);
            if (unblockedProc != NULL) {
                unblockedProc->p_s.reg_a0 = savedStatus;  // status code
                unblockedProc->p_semAdd = NULL; //Processo non più bloccato 
                softblockcount--;
                insertProcQ(&readyQueue, unblockedProc);
            }
        }    
    }
    return;
} 
*/

void handleNonTimerInterrupt(int intLineNo, unsigned int bitMap) {
    klog_print("Handling non-timer interrupt on line:");
    //klog_print_dec(intLineNo);
    //klog_print(" with bitMap:");
   // klog_print_hex(bitMap);

    int devNo = getHighestPriorityDevice(bitMap);
   // klog_print("Device number with highest priority interrupt:");
    //klog_print_dec(devNo);

    if (devNo < 0) {
        klog_print("nessun dispositivo trovato\n");
        if (current_process != NULL) {
            LDST((state_t *)BIOSDATAPAGE);
        } else {
            scheduler();
        }
        return;
    }

    unsigned int *devBase = getDeviceRegAddr(intLineNo, devNo);
    int semIndex;
    unsigned int savedStatus;

    if (intLineNo == 7) {
        // termBase è uint32_t*, quindi +0,+1,+2,+3 sono word indices
        // +0 = RECV_STATUS, +1 = RECV_CMD, +2 = TRANS_STATUS, +3 = TRANS_CMD
        unsigned int txStatus = *(devBase + 2);
        unsigned int rxStatus = *(devBase + 0);

        // TX ha priorità su RX
        if ((txStatus & STATUSMASK) != READY && (txStatus & STATUSMASK) != BUSY) {
           // klog_print("Terminal transmitter interrupt detected\n");
            savedStatus = txStatus;
            *(devBase + 3) = ACK;  // scrivi ACK al TRANSM_COMMAND
          //  klog_print("Terminal transmitter ACK written\n");

            semIndex = 32 + devNo;
          //  klog_print("semaforo da cui sbloccare processo:");
            //klog_print_dec(semIndex);
         //   klog_print("semaphore value before increment:");
           // klog_print_dec(device_semaphores[semIndex]);

            device_semaphores[semIndex]++;

          //  klog_print("semaphore value after increment:");
            //klog_print_dec(device_semaphores[semIndex]);

            if (device_semaphores[semIndex] <= 0) {
                pcb_t *unblocked = removeBlocked(&device_semaphores[semIndex]);
                if (unblocked != NULL) {
                  //  klog_print("TX process unblocked\n");
                    unblocked->p_s.reg_a0 = savedStatus;
                    unblocked->p_semAdd = NULL;
                    softblockcount--;
                    insertProcQ(&readyQueue, unblocked);
                }
            }
        }

        if ((rxStatus & STATUSMASK) != READY && (rxStatus & STATUSMASK) != BUSY) {
        //    klog_print("Terminal receiver interrupt detected\n");
            savedStatus = rxStatus;
            *(devBase + 1) = ACK;  // scrivi ACK al RECV_COMMAND

            semIndex = 40 + devNo;
            device_semaphores[semIndex]++;

            if (device_semaphores[semIndex] <= 0) {
                pcb_t *unblocked = removeBlocked(&device_semaphores[semIndex]);
                if (unblocked != NULL) {
                //    klog_print("RX process unblocked\n");
                    unblocked->p_s.reg_a0 = savedStatus;
                    unblocked->p_semAdd = NULL;
                    softblockcount--;
                    insertProcQ(&readyQueue, unblocked);
                }
            }
        }

    } else {
        // Non-terminal: +0 = STATUS, +1 = COMMAND
      //  klog_print("Non-terminal device interrupt detected\n");
        savedStatus = *(devBase + 0);
        *(devBase + 1) = ACK;

        semIndex = (intLineNo - 3) * DEVICES_PER_LINE + devNo;
         //   klog_print("semaforo da cui sbloccare processo:");
            //klog_print_dec(semIndex);

        device_semaphores[semIndex]++;

        if (device_semaphores[semIndex] <= 0) {
            pcb_t *unblocked = removeBlocked(&device_semaphores[semIndex]);
            if (unblocked != NULL) {
                unblocked->p_s.reg_a0 = savedStatus;
                unblocked->p_semAdd = NULL;
                softblockcount--;
                insertProcQ(&readyQueue, unblocked);
            }
        }
    }

    // Ritorna al processo corrente o chiama lo scheduler
    if (current_process != NULL) {
        LDST((state_t *)BIOSDATAPAGE);
    } else {
        scheduler();
    }
}

//Parte 7.2 Processor Local Timer (PLT) Interrupts

void handlePLTInterrupt() {
    setTIMER((cpu_t) NEVER); // disabilita timer fino a nuovo dispatch
    if (current_process != NULL) {
        state_t *state = (state_t *)BIOSDATAPAGE;
        current_process->p_s = *state; // Salva lo stato del processo corrente
        current_process->p_s.status |= MSTATUS_MIE_MASK;
        insertProcQ(&readyQueue, current_process);
        current_process = NULL;
    }
    scheduler();
    return;
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
    device_semaphores[PSEUDOCLOCK_INDEX] = 0; // reset del semaforo del pseudo-clock
    
    /* Return to Current Process or call Scheduler */
    if (current_process != NULL) {
        state_t *exceptionState = (state_t *)BIOSDATAPAGE;
        LDST(exceptionState);
    } else {
        scheduler();
    }
    return;
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
// ritorna la linea dell'interupt con priorità maggiore
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
        unsigned int *bitMapAddr = (unsigned int *)(INTERRUPT_BIT_MAP + ((intLineNo - 3) * 0x4));
        *bitMap = *bitMapAddr;
        if (*bitMap == 0) return 0;
    }

    return intLineNo;
}



//Main Interrupt Exception Handler
//Chimato da exceptionHandler quando CAUSE_IS_INT è true
void interruptHandler() {
    state_t *state = (state_t *)BIOSDATAPAGE; //dove il processore salva lo stato in caso di eccezione
    unsigned int cause = state->cause;
    unsigned int excCode = cause & 0xFFu; // CAUSE_EXCCODE_MASK
    klog_print("INTERRUPT HANDLER ENTRY\n");
   
    cpu_t currentTime;
    STCK(currentTime);
    if(current_process != NULL) {
        current_process->p_time += (currentTime - processStartTime);
    }
    startTOD = currentTime; //aggiorno startTOD per il processo che verrà dispatchato dopo l'interrupt, così quando tornerà a fare updateCPUtime avrà il tempo giusto da aggiungere a p_time
    
    switch (excCode) {
        case IL_CPUTIMER:
          //  klog_print("Handling PLT Interrupt\n");
            handlePLTInterrupt();
        
            
        case IL_TIMER:
         //   klog_print("Handling Interval Timer Interrupt\n");
            handleIntervalTimerInterrupt();
            
            
        default:
            //Device interrupts (lines 3-7)
            if(excCode >= IL_DISK && excCode <= IL_TERMINAL) {
           //     klog_print("Handling Device Interrupt");
              // klog_print("\n");
                int intLineNo = (int)(excCode - 14u);
                unsigned int bitMap = (*(unsigned int *)(INTERRUPT_BIT_MAP + ((intLineNo - 3) * 0x4)));
            //    klog_print("STO PER ENTRARE IN handleNonTimerInterrupt\n");
                handleNonTimerInterrupt(intLineNo, bitMap);
            } else {
                klog_print("UNKNOWN INTERRUPT TYPE\n");
            }
            
    }
    if(current_process != NULL) {
        state_t *exceptionState = (state_t *)BIOSDATAPAGE;
        LDST(exceptionState);
    } else {
        scheduler();    
    }    
}