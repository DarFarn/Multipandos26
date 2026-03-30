#include "../headers/const.h"
#include "../headers/types.h"
#include <uriscv/liburiscv.h>
#include "../phase1/headers/pcb.h"
#include "../phase1/headers/asl.h"
#include "../headers/listx.h"
#include <uriscv/types.h>
#include <uriscv/const.h>
#include "../headers/klog.h"

//External Nucleus global variables
extern int processCount;
extern int softblockcount;
extern struct list_head readyQueue;
extern pcb_t *current_process;
extern int *device_semaphores[];

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

//Calcolo del device number dall'interrupt bit map e l'interrupt line. Ritorna -1 se non trova il device
int getDeviceNumber(int intLineNo, unsigned int bitMap) {
    int devNo;
    for (devNo = 0; devNo < DEVICES_PER_LINE; devNo++) {
        if (bitMap & (1 << devNo)) {
            return devNo; //pending interrupt trovato
        }
    }
    return -1;
}

//Calcolo dell'inidirizzo del device’s device register
unsigned int getDeviceRegAddr(int intLineNo, int devNo) {
    return DEVICE_REG_BASE + ((intLineNo - 3) * DEV_REG_SIZE) + (devNo * 0x10);
}

