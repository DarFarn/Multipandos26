#include "../headers/const.h"
#include "../headers/types.h"
#include <uriscv/liburiscv.h>
#include "../phase1/headers/pcb.h"
#include "../phase1/headers/asl.h"
#include "../headers/listx.h"
#include <uriscv/types.h>
#include <uriscv/const.h>
#include "../headers/klog.h"

/* Variabili globali Nucleus (initialize.c) */
extern int processCount;
extern int softblockcount;
extern struct list_head readyQueue;
extern pcb_t *current_process;
extern int *device_semaphores[];
