//implements test 

//inizializzerà:
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
#include "../phase3/headers/initProc.h"

//da implementare

//1. test(), entry point deve inizializzare (non vuol dire definire qua!):
//swap pool structure, per l'appunto definite in vmSupport 
//swap mutex, capire dove sta
//terminal semaphores, due dato che dovrebbe esserci un solo terminale alla fine 
//mastersemaphore per la gestione dello stesso istantiator process
//shellSemaphore 
//creare processo shell (tramite NSYS apposito?)
//poi alla fine eseguo P sul mastersemaphore e termino con NSYS2


support_t support_structs[UPROCMAX];
void initUProc(int asid){
//che inizializzerà, per lo specifico processo:
//sup_asid
//sup_privatePgTbl[32]
//sup_exceptContext[2]
SYSCALL(CREATEPROCESS, state, support_ptr, 0);

//inizializzare pagetable, lasciare in h solo struttura e qua inizializzazione vera e propria













