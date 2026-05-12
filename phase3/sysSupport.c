#include "../headers/types.h"
#include <uriscv/types.h>
#include <uriscv/liburiscv.h>
#include "../headers/const.h"
#include <uriscv/const.h>
#include "../phase2/headers/exceptions.h"
#include "../headers/support.h"

//implements:


void supsyscallHandler() {

    support_t *sup = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_t *state = &sup->sup_exceptState[GENERALEXCEPT];

    int sysno = state->gpr[10];

    switch (sysno) {

        case TERMINATE:
            if (sup->sup_asid == 1)
                SYSCALL(VERHOGEN, (int)&masterSem, 0, 0);
            else
                SYSCALL(VERHOGEN, (int)&shellSem, 0, 0);

            SYSCALL(TERMINATE, 0, 0, 0);
            break;

        case CREATEPROCESS:
            initUProc(state->gpr[11]);
            SYSCALL(PASSEREN, (int)&shellSem, 0, 0);
            break;

        default:
            SYSCALL(TERMINATE, 0, 0, 0);
    }

    state->pc_epc += 4;
    LDST(state);
}


void supprogramTrapHandler() {
    SYSCALL(TERMINATE, 0, 0, 0);
}
void supgeneralExceptionHandler() {

    support_t *sup = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    state_t *state = &sup->sup_exceptState[GENERALEXCEPT];

    if (((getCAUSE() & GETEXECCODE) >> 2) == SYSCALL)
        supsyscallHandler();
    else
        supprogramTrapHandler();
}

