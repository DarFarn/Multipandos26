#include <uriscv/liburiscv.h>
#include "const.h"
#include "types.h"
#include "listx.h"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include <uriscv/types.h>
#include <uriscv/const.h>
#include "exceptions.h"
#include "scheduler.h"

extern int processCount;
extern int softblockcount;
extern struct list_head readyQueue;
extern pcb_t *current_process;
extern cpu_t *startTime; // i need that, deve essere fatto dallo scheduler
extern int deviceSemaphores[SEMDEVLEN]; // intero o semd_t??????????????
extern int pseudoClockSemaphore;

extern void scheduler();
extern void interruptHandler();

void uTLB_RefillHandler() {
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST((state_t*) BIOSDATAPAGE);
}

void updateCPUtime(){
    cpu_t now;
    STCK(now); 
    current_process->p_time = current_process->p_time + (now - *startTime);
    startTime = now;
}

void createProcess(state_t *state){
    pcb_t *newProcess = allocPcb();
    if (newProcess == NULL){
        state->reg_a0 = -1;
        return;
    }
    if(state->reg_a1){
        newProcess->p_s = *((state_t *) state->reg_a1);
    } 
    if(state->reg_a2){
        newProcess->p_supportStruct = (support_t *) state->reg_a2;  
    }
    else{
        newProcess->p_supportStruct = NULL;
    }
    newProcess->p_time = 0;
    newProcess->p_semAdd = NULL;
    if(state->reg_a3){
        newProcess->p_prio = state->reg_a3;
    }
    else{
        newProcess->p_prio = 0;
    }

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
        if((p->p_semAdd >= &deviceSemaphores[0] && p->p_semAdd < &deviceSemaphores[SEMDEVLEN]) || p->p_semAdd == &pseudoClockSemaphore){
            softblockcount--;
        }
        p->p_semAdd = NULL;
    }
    freePcb(p);
    processCount--;
}

pcb_t* findProcess(int pid){
    pcb_t* root = current_process;
    while(root->p_parent != NULL){
        root = root->p_parent;
    }
    return processFinder(root, pid);
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

void terminateProcess(state_t *state){
    if(state->reg_a1 == 0){
        processKiller(current_process);
    }
    else{
        pcb_t* toKill = findProcess(state->reg_a1);
        if(toKill == NULL){
            return;
        }
        processKiller(toKill);
    }
    scheduler();
}

void passeren(state_t* state){
    int *semAdd = (int *)state->reg_a1; // intero o puntatore ad un'intero??????
    (*semAdd)--;
    if(*semAdd < 0){
        current_process->p_semAdd = semAdd;
        insertBlocked(semAdd, current_process);
        scheduler();
    }
    else return;    
}

void verhogen(state_t* state){
    int *semAdd = (int *)state->reg_a1; 
    (*semAdd)++;
    if(*semAdd <= 0){
        pcb_t* toFree = removeBlocked(semAdd);
        if(toFree != NULL){
            toFree->p_semAdd = NULL;
            insertProcQ(&readyQueue, toFree);
        }
    }
    else return;    
}

void semaphoreP(int *semAdd){
    (*semAdd)--;
    if(*semAdd < 0){
        current_process->p_semAdd = semAdd;
        insertBlocked(semAdd, current_process);
        softblockcount++;
        scheduler();
    }
    else return;    
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
    
    semaphoreP(&deviceSemaphores[semIndex]);
    
    // Note: The status will be set in a0 when the process resumes after interrupt
    // se ne dovrebbe occupare l'interrupt handler
}

void getCPUtime(state_t *state){
    cpu_t currentTime;
    STCK(currentTime);
    cpu_t elapsedTime = currentTime - *startTime;  // supponendo che startTime sia settato bene dallo scheduler
    // startTime va resettato ogni volta che viene dispachato un processo
    state->reg_a0 = current_process
    ->p_time + elapsedTime;
}

void waitForClock(state_t *state){
    semaphoreP(&pseudoClockSemaphore);
}

void getSupportData(state_t *state){
    if(current_process->p_supportStruct != NULL){
        state->reg_a0 = (support_t *) current_process->p_supportStruct; // lo devo ritornare come struct o unsigned int? boh
    }
    else{
        state->reg_a0 = NULL;
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
    scheduler();
}

void exceptionHandler(){
    state_t *savedState = GET_EXCEPTION_STATE_PTR(0);
    unsigned int cause = getCAUSE();
    unsigned int excCode = (cause & GETEXECCODE) >> CAUSESHIFT;
    if(CAUSE_IP_GET(cause, IL_CPUTIMER) || CAUSE_IP_GET(cause, IL_TIMER) || CAUSE_IP_GET(cause, IL_DISK) || 
       CAUSE_IP_GET(cause, IL_FLASH) || CAUSE_IP_GET(cause, IL_ETHERNET) || CAUSE_IP_GET(cause, IL_PRINTER) || 
       CAUSE_IP_GET(cause, IL_TERMINAL)){  
        interruptHandler();
    }
    else if(excCode >= 24 && excCode <= 28){ 
        tlbHandler();
    }
    else if(excCode == 8 || excCode == 11){ 
        syscallHandler(savedState);
    }
    else if((excCode >= 0 && excCode <=7)||excCode == 9 || excCode == 10 || (excCode >= 12 && excCode <=23)){  
        programTrapHandler();
    }
    else programTrapHandler(); // per eccezioni non previste, le tratto come program trap
}   

void passUpOrDie(int index) {
    if (current_process->p_supportStruct == NULL) {
        processKiller(current_process);
        scheduler();
    } else {
        support_t *sup = current_process->p_supportStruct;
        state_t *savedState = GET_EXCEPTION_STATE_PTR(0);
        sup->sup_exceptState[index] = *savedState;
        LDCXT(sup->sup_exceptContext[index].stackPtr, 
            sup->sup_exceptContext[index].status, 
            sup->sup_exceptContext[index].pc);
    }
}

void tlbHandler(){
    passUpOrDie(PGFAULTEXCEPT);
}

void programTrapHandler(){
    passUpOrDie(GENERALEXCEPT);
}

void syscallHandler(state_t *state){
    int a0 = state->reg_a0;
    if(a0 > 0){
        passUpOrDie(GENERALEXCEPT);
        return;
    }
    else if(a0 < 0){
        // Check if in user mode
        if((state->status & MSTATUS_MPP_MASK) == 0){  // user mode
            state->cause = (state->cause & CLEAREXECCODE) | (PRIVINSTR << CAUSESHIFT);
            programTrapHandler();
            return;
        }
        else{  // Kernel mode
            switch(a0){
                case CREATEPROCESS:
                    createProcess(state);
                    state->pc_epc = state->pc_epc + 4;
                    LDST(state);
                    break;
                case TERMPROCESS:
                    terminateProcess(state);  
                    break;
                case PASSEREN:
                    state->pc_epc = state->pc_epc + 4;
                    current_process->p_s = *state; // Salva lo stato prima di fare la P
                    updateCPUtime();
                    passeren(state);
                    LDST(state); // se il processo non viene bloccato riprende da qui
                    break;
                case VERHOGEN:
                    verhogen(state);
                    state->pc_epc = state->pc_epc + 4;
                    LDST(state);
                    break;
                case DOIO:
                    state->pc_epc = state->pc_epc + 4;
                    current_process->p_s = *state; // Salva lo stato prima di fare l'I/O
                    updateCPUtime();
                    doIO(state);
                    break;
                case GETTIME:
                    getCPUtime(state);
                    state->pc_epc = state->pc_epc + 4;
                    LDST(state);
                    break;
                case CLOCKWAIT:
                    state->pc_epc = state->pc_epc + 4;
                    current_process->p_s = *state; // Salva lo stato prima di aspettare il clock
                    updateCPUtime();
                    waitForClock(state);
                    break;
                case GETSUPPORTPTR:
                    getSupportData(state);
                    state->pc_epc = state->pc_epc + 4;
                    LDST(state);
                    break;
                case GETPROCESSID:
                    getProcessID(state);
                    state->pc_epc = state->pc_epc + 4;
                    LDST(state);
                    break;
                case YIELD:
                    state->pc_epc = state->pc_epc + 4;
                    updateCPUtime();
                    yield(state);
                    break;
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
}