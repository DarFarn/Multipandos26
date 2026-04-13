#include <uriscv/liburiscv.h>
#include "../headers/const.h"
#include "../headers/types.h"
#include "../headers/listx.h"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include <uriscv/types.h>
#include <uriscv/const.h>
#include "../phase2/headers/scheduler.h"
#include "../phase2/headers/initialize.h"
#include "../phase2/headers/interrupts.h"
#include "../headers/klog.h"

extern int processCount;
extern int softblockcount;
extern struct list_head readyQueue;
extern pcb_t *current_process;
extern cpu_t processStartTime; // i need that, deve essere fatto dallo scheduler
extern int device_semaphores[];
extern pcb_t *activeProcs[MAXPROC]; // array che contiene i puntatori a tutti i processi attivi, da 0 a MAXPROC-1, se un indice è null allora il processo è terminato
extern cpu_t startTOD;


extern void scheduler();
extern void interruptHandler();

// void uTLB_RefillHandler() {
//    setENTRYHI(0x80000000);
//    setENTRYLO(0x00000000);
//    TLBWR();
//    LDST((state_t*) BIOSDATAPAGE);
//}

static void copyState(state_t *dst, state_t *src) {
    unsigned int *d = (unsigned int *) dst;
    unsigned int *s = (unsigned int *) src;
    for (int i = 0; i < STATE_T_SIZE_IN_BYTES / WORDLEN; i++)
        d[i] = s[i];
}


void *memcpy(void *dest, const void *src, unsigned int n) { //BOOOOOHH
    char *d = dest;
    const char *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

void updateCPUtime(){
    cpu_t now;
    STCK(now); 
    current_process->p_time = current_process->p_time + (now - startTOD);
    startTOD = now; //PROBLEMA 1
}

void createProcess(state_t *state){
    klog_print("STO PER CRAFTARE UN PROCESSO\n");
    pcb_t *newProcess = allocPcb();
    state_t *newState = (state_t *) state->reg_a1; // reg_a1 contiene il puntatore allo stato del nuovo processo
    if (newProcess == NULL){
        state->reg_a0 = -1;
        copyState(&current_process->p_s, state);
        LDST(&current_process->p_s);
        return;
    }
    copyState(&newProcess->p_s, newState);
    if(state->reg_a2){
        newProcess->p_prio = state->reg_a2;  
    }
    else{
        newProcess->p_prio = 0;
    }
    if (state->reg_a3){
        newProcess->p_supportStruct = (support_t *) state->reg_a3;
    }
    else{
        newProcess->p_supportStruct = NULL;
    }
    newProcess->p_time = 0;
    newProcess->p_semAdd = NULL;
    
    insertChild(current_process, newProcess);
    insertProcQ(&readyQueue, newProcess);
    processCount++;
    state->reg_a0 = newProcess->p_pid;
}

void processKiller(pcb_t* p){
    if(p==NULL) return;
    while(!emptyChild(p)){
        pcb_t* child = removeChild(p);
        processKiller(child);
    }
    outChild(p);
    if(p->p_semAdd != NULL){
        outBlocked(p);
        if(p->p_semAdd >= &device_semaphores[0] && p->p_semAdd <= &device_semaphores[PSEUDOCLOCK_INDEX]){
            softblockcount--;
        }
        p->p_semAdd = NULL;
    }
    else if (p != current_process){
        outProcQ(&readyQueue, p);
    }
    freePcb(p);
    processCount--;
}

pcb_t* processFinder(pcb_t* root, int pid){ 

    if(root == NULL) return NULL; 
    if(root->p_pid == pid) return root;

    struct list_head* pos;
    list_for_each(pos, &root->p_child){
        pcb_t* child = container_of(pos, pcb_t, p_sib);
        pcb_t* res = processFinder(child, pid);
        if(res != NULL) return res;
    }
    return NULL;
}


pcb_t* findProcess(int pid){
    pcb_t* root = current_process;
    while(root->p_parent != NULL){
        root = root->p_parent;
    }
    return processFinder(root, pid);
}

void terminateProcess(state_t *state){
    if(state->reg_a1 == 0){
        processKiller(current_process);
        current_process = NULL;
    }
    else{
        pcb_t* toKill = findProcess(state->reg_a1);
        if(toKill == NULL){
            scheduler();
            return;
        }
        if(toKill == current_process){
            current_process = NULL;
            
        }
        processKiller(toKill);
    }
    scheduler();
}

void passeren(state_t* state){
    klog_print("STARTANDO LA P OPERATION\n");
    int *semAdd = (int *)state->reg_a1;
    if (semAdd == NULL) {
        klog_print("P addr NULL\n");
        return;
    }
    klog_print("P addr:");
    klog_print_hex((unsigned int)semAdd);
    (*semAdd)--;
    klog_print("sem val:");
    klog_print_dec(*semAdd);
    if (*semAdd < 0) {
        klog_print("P operation blocking process\n");
        klog_print("Passeren saving status:");
        klog_print_hex(state->status);
        klog_print("Passeren pC:");
        klog_print_hex(state->pc_epc);
        current_process->p_s = *state;
        current_process->p_semAdd = semAdd;
        if (insertBlocked(semAdd, current_process) != 0) {
            PANIC();
        }
        current_process = NULL;
        scheduler();
    }
    klog_print("P returning");
    return;
}

void verhogen(state_t* state){
    klog_print("starting V operation\n");
    int *semAdd = (int *)state->reg_a1; 
    klog_print("V addr:");
    klog_print_hex((unsigned int)semAdd);
    (*semAdd)++;
    if(*semAdd <= 0){
        pcb_t* toFree = removeBlocked(semAdd);
        if(toFree != NULL){
            klog_print("VERHOGEN unblocked pcb p_s.status:");
            klog_print_hex(toFree->p_s.status);   // ← add this
            toFree->p_semAdd = NULL;
            insertProcQ(&readyQueue, toFree);
        }
    }
    klog_print("V almost done");
    return;    
}

void semaphoreP(int *semAdd){
    (*semAdd)--;
    if(*semAdd < 0){
        current_process->p_semAdd = semAdd;
        insertBlocked(semAdd, current_process);
        softblockcount++;
        scheduler();
    }
    return;    
}

void semaphoreV(int *semAdd){
    (*semAdd)++;
    if(*semAdd <= 0){
        pcb_t* toFree = removeBlocked(semAdd);
        if(toFree != NULL){
            toFree->p_semAdd = NULL;
            insertProcQ(&readyQueue, toFree);
            softblockcount--;
        }
    }
    else return;    
}
/*
void doIO(state_t *state){ //???????????????????fatta con chat?????????????????????????????????
    memaddr commandAddr = (memaddr) state->reg_a1;  // Address of the command field
    int commandValue = (int) state->reg_a2;
    
    devregarea_t *devArea = (devregarea_t *) RAMBASEADDR;
    memaddr devRegBase = commandAddr & ~0xF;
    int offset = commandAddr - START_DEVREG;
    int lineIndex = offset / 0x80;
    int deviceNum = (offset % 0x80) / 0x10;
    int line = lineIndex + 3; 
    
    int semIndex;
    if (line >= 3 && line <= 6) {
        // Non-terminal devices: disks, tapes, networks, printers
        semIndex = lineIndex * DEVPERINT + deviceNum;;
    } else if (line == 7) {
        // Terminals: 2 sub-devices per terminal (recv/trans)
        int fieldOffset = commandAddr - devRegBase;
        if(fieldOffset == (RECVCOMMAND * WORDLEN)){
            semIndex = (DEVINTNUM -1) * DEVPERINT + deviceNum * 2;  // recv semaphore
        }
        else{
            semIndex = (DEVINTNUM -1) * DEVPERINT + deviceNum * 2 + 1;
        }
    } else {
        state->reg_a0 = -1;  // Invalid line
        return;
    }
    *(int *)commandAddr = commandValue;  
    
    semaphoreP(&device_semaphores[semIndex]);
    
    // Note: The status will be set in a0 when the process resumes after interrupt
    // se ne dovrebbe occupare l'interrupt handler
}
    */

void doIO(state_t *state) {

    klog_print("STARTANDO LA DOIO OPERATION\n");

    memaddr commandAddr = (memaddr) state->reg_a1;
    int commandValue    = (int) state->reg_a2;

    klog_print("commandAddr:");
    klog_print_hex(commandAddr);
    klog_print("START_DEVREG:");
    klog_print_hex(START_DEVREG);
    klog_print("offset:");
    klog_print_hex(commandAddr - START_DEVREG);
    klog_print("limit:");
    klog_print_hex(5 * 0x80);

    if (commandAddr < START_DEVREG) {
        klog_print("INVALID DEVICE ADDRESS because it's below the start address\n");
        state->reg_a0 = -1;
        return;
    }

    unsigned int offset = commandAddr - START_DEVREG;
    if (offset >= 5 * 0x80) {
        klog_print("INVALID DEVICE ADDRESS because it's beyond the valid range\n");
        state->reg_a0 = -1;
        return;
    }

    int lineIndex = offset / 0x80;                // which interrupt line region (0-4)
    int devNo     = (offset % 0x80) / 0x10;       // which device (0-7)
    int line      = lineIndex + 3;                // actual line number (3-7)

    int semIndex;
    if (line >= 3 && line <= 6) {
        int fieldOffset = offset % 0x10;
        if (fieldOffset != 4) {
            klog_print("INVALID NON-TERMINAL COMMAND ADDRESS\n");
            state->reg_a0 = -1;
            return;
        }
        semIndex = getSemaphoreIndex(line, devNo, 0);
    } else {
        memaddr termBase  = START_DEVREG + lineIndex * 0x80 + devNo * 0x10;
        int fieldOffset   = commandAddr - termBase;
        if (fieldOffset == 4) {
            semIndex = getSemaphoreIndex(line, devNo, 0);     // recv
        } else if (fieldOffset == 12) {
            semIndex = getSemaphoreIndex(line, devNo, 1); // transmit
        } else {
            klog_print("INVALID TERMINAL COMMAND ADDRESS\n");
            state->reg_a0 = -1;
            return;
        }
    }
    current_process->p_s = *state;

    klog_print("doIO addr:");
    klog_print_hex(commandAddr);
    klog_print("semIdx:");
    klog_print_dec(semIndex);

    // Write command to device register
    *(int *)commandAddr = commandValue;

    int *sem = &device_semaphores[semIndex];
    (*sem)--;
    current_process->p_semAdd = sem;
    if (insertBlocked(sem, current_process) != 0) {
        PANIC();
    }
    softblockcount++;
    current_process = NULL;
    scheduler();
}

void getCPUtime(state_t *state){
    cpu_t currentTime;
    STCK(currentTime);
    cpu_t elapsedTime = currentTime - processStartTime;  // supponendo che startTime sia settato bene dallo scheduler
    // qual'è la differenza tra elapsedTime e current_process->p_time? elapsedTime è il tempo trascorso dall'ultimo dispatch, mentre p_time è il tempo totale accumulato dal processo fino ad ora. Quindi per ottenere il tempo totale fino ad ora, devo sommare elapsedTime a p_time.
    // ovvero elapsedTime rappresenta il tempo trascorso da quando ha ripreso l'esecuzione l'ultima volta, mentre p_time rappresenta il tempo totale accumulato dal processo fino a quel momento. Quindi, per ottenere il tempo totale fino ad ora, devo sommare elapsedTime a p_time.
    // quindi startTime invece è il timestamp del momento in cui il processo è stato rilanciato, e viene aggiornato ogni volta che il processo riprende l'esecuzione
    // dopo essere stato bloccato. è quidni un contatore che viene resettato
    // ogni volta che lancio un processo aggiorno startTime con il timestamp attuale
    // startTime va resettato ogni volta che viene dispachato un processo
    state->reg_a0 = current_process->p_time + elapsedTime;
}

void waitForClock(state_t *state){
     int *sem = &device_semaphores[PSEUDOCLOCK_INDEX];
    (*sem)--;
    current_process->p_semAdd = sem;
    if (insertBlocked(sem, current_process) != 0) {
        PANIC();
    }
    softblockcount++;
    current_process = NULL;
    scheduler();
}

void getSupportData(state_t *state){
    if(current_process->p_supportStruct != NULL){
        state->reg_a0 = (unsigned int) current_process->p_supportStruct; // lo devo ritornare come struct o unsigned int? boh
    }
    else{
        state->reg_a0 = (unsigned int) NULL; // PROBLEMNA 2
    }
}

void getProcessID(state_t *state){
    if(state->reg_a1 == 0){
        state->reg_a0 = current_process->p_pid;
    }
    else{
        // devo trovare il processo padre de current_process
        pcb_t *parent = current_process->p_parent;
        if(parent != NULL){
            state->reg_a0 = parent->p_pid;
        }
        else state->reg_a0 = 0; // se non ha padre, ritorna 0 (processo root)    
    }
}

void yield(state_t *state){ 
    current_process->p_s = *state;
    updateCPUtime();
    list_add_tail(&(current_process->p_list), &readyQueue); // not sure se vada bene mettere current_process->p_lst così
    current_process = NULL;
    scheduler();
}

void passUpOrDie(int index) {
    klog_print("PassUPorDie opeartion started");
    if (current_process->p_supportStruct == NULL) {
        processKiller(current_process);
        current_process = NULL;
        klog_print("KILLATO PROCESSO CORRENTE PER MANCANZA SUPPORT STRUCT");
        scheduler();
    } else {
        klog_print("PassUPorDie operation continuing");
        support_t *sup = current_process->p_supportStruct;
        state_t *savedState = GET_EXCEPTION_STATE_PTR(0);
        sup->sup_exceptState[index] = *savedState;
        LDCXT(sup->sup_exceptContext[index].stackPtr, 
            sup->sup_exceptContext[index].status, 
            sup->sup_exceptContext[index].pc);
    }
    return;
}

void tlbHandler(){
    passUpOrDie(PGFAULTEXCEPT);
}

void programTrapHandler(){
    klog_print("program trap handling started\n");
    klog_print_dec(current_process != NULL ? 1 : 0);
    passUpOrDie(GENERALEXCEPT);
    return;
}

void syscallHandler(state_t *state){

    klog_print("SYS a0:");
    klog_print_dec(-(state->reg_a0)); 
    klog_print("MPP check:");
    klog_print_dec((state->status & 0x1800) == 0 ? 1 : 0);
    int a0 = state->reg_a0;
    if(a0 > 0){
        passUpOrDie(GENERALEXCEPT);
        return;
    }
    else if(a0 < 0){
        // Check if in user mode
        if((current_process->p_s.status & MSTATUS_MPP_MASK) == 0){  // invece che con state->status??? // user mode
            state->cause = state->cause & CLEAREXECCODE;
            programTrapHandler();
            return;
        }
        else{  // Kernel mode
            switch(a0){
                case CREATEPROCESS:
                    createProcess(state);
                    state->pc_epc = state->pc_epc + 4;
                    LDST(state);
                    return;
                case TERMPROCESS:
                    terminateProcess(state);  
                    return;
                case PASSEREN:
                    state->pc_epc = state->pc_epc + 4;
                    updateCPUtime();
                    passeren(state);
                    klog_print("P operation completed\n");
                    LDST(state);
                    return;
                case VERHOGEN:
                    verhogen(state);
                    klog_print("V operation completed\n");
                    state->pc_epc = state->pc_epc + 4;
                    klog_print("Verhogen about to return state:");
                    klog_print_hex(state->status);
                    LDST(state);
                    return;
                case DOIO:
                    state->pc_epc = state->pc_epc + 4;
                    klog_print("DOIO saving status:");
                    klog_print_hex(current_process->p_s.status);
                    updateCPUtime();
                    doIO(state);
                
                    klog_print("DOIO operation completed\n");
                    return;
                case GETTIME:
                    getCPUtime(state);
                    state->pc_epc = state->pc_epc + 4;
                    LDST(state);
                    return;
                case CLOCKWAIT:
                    state->pc_epc = state->pc_epc + 4;
                    updateCPUtime();
                    current_process->p_s = *state;
                    waitForClock(state);
                    return;
                case GETSUPPORTPTR:
                    getSupportData(state);
                    state->pc_epc = state->pc_epc + 4;
                    LDST(state);
                    return;
                case GETPROCESSID:
                    getProcessID(state);
                    state->pc_epc = state->pc_epc + 4;
                    LDST(state);
                    return;
                case YIELD:
                    state->pc_epc = state->pc_epc + 4;
                    yield(state);
                    return;
                default:
                    // servizio inesistente
                    state->cause = (state->cause & CLEAREXECCODE) | (PRIVINSTR << CAUSESHIFT);
                    programTrapHandler();
                    return;  
            }
        }
    }
    else{
        // a0 == 0, servizio non valido
        state->cause = (state->cause & CLEAREXECCODE) | (PRIVINSTR << CAUSESHIFT);
        programTrapHandler();
    }
    return;
}

void exceptionHandler(){
    state_t *savedState = (state_t *) BIOSDATAPAGE; // dove il processore salva lo stato in caso di eccezione
    savedState->mie = MIE_ALL; // FOOOOORSE
    klog_print("EXC entry status:");
    klog_print_hex(savedState->status);    
    klog_print("EXC entry cause:");
    klog_print_hex(savedState->cause);
    unsigned int cause = savedState->cause;
    
    
    //testing
    
    klog_print("status:");
    klog_print_hex(savedState->status);
    klog_print("raw cause:");
    klog_print_hex(getCAUSE());
    

    if(cause & 0x80000000){  
        interruptHandler();
        return;
    }
    unsigned int excCode = cause & 0xFFu;
    klog_print("exc:");
    klog_print_dec(excCode);
    if(excCode >= 24 && excCode <= 28){ 
        tlbHandler();
        return;
    }
    else if(excCode == 8 || excCode == 11){ 
        syscallHandler(savedState);
        return;
    }
    else if((excCode >= 0 && excCode <=7)||excCode == 9 || excCode == 10 || (excCode >= 12 && excCode <=23)){  
        klog_print("STARTING PROGRAM TRAP HANDLER\n");
        programTrapHandler();
        return;
    }
    else programTrapHandler(); // per eccezioni non previste, le tratto come program trap
    return;
}   