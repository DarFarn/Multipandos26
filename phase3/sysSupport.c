#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include <uriscv/const.h>
#include "../headers/types.h"
#include "../headers/const.h"
#include "../phase2/headers/interrupts.h"
#include "headers/support.h"
#include "../headers/klog.h"

/* Terminal status codes carry the completion code in the low byte and
   (for received characters) the character itself in the upper bits. */
#define TERMSTATMASK 0xFF

/* getDeviceRegAddr() (phase2/interrupts.c) expects the small interrupt
   *line* number (Table 1 of the spec: ..., terminal=7), not the
   IL_TERMINAL interrupt *exception code* (21) used for interrupt
   routing; the two are related by "line = exception code - 14". */
#define TERMINAL_INTLINE (IL_TERMINAL - 14)

/* Terminates the currently executing U-proc, signalling whoever is
   waiting for it: the InstantiatorProcess (masterSem) if this is the
   shell itself, the shell (shellSem) otherwise. Shared by SYS2 and by
   the Program Trap handler. */
void terminateUProc() {
    support_t *sup = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    if (sup->sup_asid == SHELLASID) {
        klog_print("TERM shell-to-master\n");
        SYSCALL(VERHOGEN, (int) &masterSem, 0, 0);
    } else {
        klog_print("TERM child-to-shell asid=");
        klog_print_dec((unsigned int) sup->sup_asid);
        klog_print("\n");
        SYSCALL(VERHOGEN, (int) &shellSem, 0, 0);
    }

    SYSCALL(TERMPROCESS, 0, 0, 0);
}

/* Returns TRUE if the [addr, addr + len) range lies entirely within the
   calling U-proc's own logical address space (kuseg). Used by SYS4
   (WriteTerminal), which is given an explicit, real length. */
static int isValidUserRange(unsigned int addr, int len) {
    if (len < 0 || len > MAXSTRLENG)
        return FALSE;
    if (addr < KUSEG || (addr + len) > USERSTACKTOP)
        return FALSE;
    return TRUE;
}

/* Returns TRUE if addr is a valid starting address within the calling
   U-proc's own logical address space (kuseg). Used by SYS5
   (ReadTerminal), which (unlike SYS4) is given no length at all: how
   many bytes actually get written depends on where the terminating
   newline shows up, not on a caller-supplied bound, so checking a fixed
   worst-case range here would reject perfectly valid buffers that
   happen to sit near the top of the one-page stack. */
static int isValidUserAddr(unsigned int addr) {
    return addr >= KUSEG && addr < USERSTACKTOP;
}

/* SYS4: WriteTerminal */
static void writeTerminal(state_t *state) {
    char *virtAddr = (char *) state->reg_a1;
    int len = (int) state->reg_a2;
    unsigned int *devReg;
    unsigned int status;
    int i;

    if (!isValidUserRange((unsigned int) virtAddr, len)) {
        terminateUProc();
        return;
    }

    devReg = getDeviceRegAddr(TERMINAL_INTLINE, 0);

    SYSCALL(PASSEREN, (int) &termWriteSem, 0, 0);

    for (i = 0; i < len; i++) {
        unsigned int command = (((unsigned int) virtAddr[i]) << 8) | TRANSMITCHAR;
        status = SYSCALL(DOIO, (int) &devReg[3], (int) command, 0); /* TRANSM_COMMAND */

        if ((status & TERMSTATMASK) != OKCHARTRANS) {
            SYSCALL(VERHOGEN, (int) &termWriteSem, 0, 0);
            state->reg_a0 = -((int) (status & TERMSTATMASK));
            return;
        }
    }

    SYSCALL(VERHOGEN, (int) &termWriteSem, 0, 0);
    state->reg_a0 = len;
}

/* SYS5: ReadTerminal */
static void readTerminal(state_t *state) {
    char *virtAddr = (char *) state->reg_a1;
    unsigned int *devReg;
    unsigned int status;
    int count = 0;
    char c;

    if (!isValidUserAddr((unsigned int) virtAddr)) {
        terminateUProc();
        return;
    }

    devReg = getDeviceRegAddr(TERMINAL_INTLINE, 0);

    SYSCALL(PASSEREN, (int) &termReadSem, 0, 0);

    while (count < MAXSTRLENG - 1) {
        status = SYSCALL(DOIO, (int) &devReg[1], RECEIVECHAR, 0); /* RECV_COMMAND */

        if ((status & TERMSTATMASK) != CHARRECV) {
            SYSCALL(VERHOGEN, (int) &termReadSem, 0, 0);
            state->reg_a0 = -((int) (status & TERMSTATMASK));
            return;
        }

        c = (char) (status >> 8);
        virtAddr[count] = c;
        count++;

        if (c == '\n')
            break;
    }

    SYSCALL(VERHOGEN, (int) &termReadSem, 0, 0);
    state->reg_a0 = count;
}

/* SYS6: Execute. Only the shell may spawn a new U-proc; it blocks until
   the spawned U-proc terminates. */
static void execute(state_t *state) {
    support_t *sup = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    int asid = (int) state->reg_a1;

    if (sup->sup_asid != SHELLASID) {
        terminateUProc();
        return;
    }

    if (asid < 2 || asid > UPROCMAX)
        return; /* silently ignore an out-of-range request */

    initUProc(asid);

    SYSCALL(PASSEREN, (int) &shellSem, 0, 0);
}

/* Support Level SYSCALL exception handler: dispatches SYS1, SYS2, SYS4,
   SYS5 and SYS6. */
static void supSyscallHandler(state_t *state) {
    int sysno = (int) state->reg_a0;
    cpu_t tod;

    switch (sysno) {

        case GET_TOD:
            STCK(tod);
            state->reg_a0 = (unsigned int) tod;
            break;

        case TERMINATE:
            terminateUProc();
            return; /* never reached */

        case WRITETERMINAL:
            writeTerminal(state);
            break;

        case READTERMINAL:
            readTerminal(state);
            break;

        case EXECUTE:
            execute(state);
            break;

        default:
            terminateUProc();
            return; /* never reached */
    }

    /* pc_epc was already advanced by the Nucleus's exceptionHandler(),
       once, for every SYSCALL exception, before passing it up here;
       incrementing it again would skip an extra instruction on resume. */
    LDST(state);
}

/* Support Level Program Trap exception handler: terminate the offending
   U-proc in an orderly fashion. */
static void supProgramTrapHandler() {
    klog_print("PROGRAM TRAP\n");
    terminateUProc();
}

/* Support Level general exception handler: reached via
   sup_exceptContext[GENERALEXCEPT]. */
void supGeneralExceptionHandler() {
    support_t *sup = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_t *state = &sup->sup_exceptState[GENERALEXCEPT];
    unsigned int excCode = state->cause & 0xFFu;

    if (excCode == 8 || excCode == 11)
        supSyscallHandler(state);
    else
        supProgramTrapHandler();
}
