#include "../phase3/headers/support.h"
//implements:

void generalExceptionHandler() {

    support_t *sup = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    state_t *state = &sup->sup_exceptState[GENERALEXCEPT];

    if ((state->cause & EXCCODE_MASK) == SYSCALL)
        syscallHandler();
    else
        programTrapHandler();
}

void syscallHandler() {

    support_t *sup = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_t *state = &sup->sup_exceptState[GENERALEXCEPT];

    int sysno = state->a0;

    switch (sysno) {

        case TERMINATE:
            if (sup->sup_asid == 1)
                SYSCALL(VERHOGEN, (int)&masterSem, 0, 0);
            else
                SYSCALL(VERHOGEN, (int)&shellSem, 0, 0);

            SYSCALL(TERMINATE, 0, 0, 0);
            break;

        case EXECUTE:
            initUProc(state->a1);
            SYSCALL(PASSEREN, (int)&shellSem, 0, 0);
            break;

        default:
            SYSCALL(TERMINATE, 0, 0, 0);
    }

    state->pc += 4;
    LDST(state);
}


void programTrapHandler() {
    SYSCALL(TERMINATE, 0, 0, 0);
}

void programtrapHandler(){
//terminate program (NSYS2?);}
