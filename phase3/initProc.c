#include "../headers/types.h"
#include <uriscv/liburiscv.h>
#include "../headers/const.h"
#include <uriscv/types.h>
#include "../headers/const.h"
#include <uriscv/const.h>
#include "../phase2/headers/exceptions.h"
#include "../headers/support.h"

support_t support_structs[UPROCMAX]; //crea un array di 8 support_t, già definita in types.h

int masterSem = 0;
int shellSem= 0;

/* ===================== TEST ===================== */
void test() {

    /* init swap structures */
    initSwapStructs();

    /* init semaphores */
    masterSem = 0;
    shellSem = 0;

    /* create shell (ASID = 1 for simplicity) */
    initUProc(1);

    /* wait for shell termination */
    SYSCALL(PASSEREN, (int)&masterSem, 0, 0); //dove è definita PASSEREN così in maiuscolo?

    /* terminate */
    SYSCALL(TERMINATE, 0, 0, 0);
}

/* ===================== INIT UPROC ===================== */
void initUProc(int asid) {

    support_t *sup = &support_structs[asid - 1];

    sup->sup_asid = asid;

    /* -------- PAGE TABLE INIT -------- */
    for (int i = 0; i < 31; i++) {
        sup->sup_privatePgTbl[i].pte_entryHI = (0x80000 + i) | (asid << ASIDSHIFT);
        sup->sup_privatePgTbl[i].pte_entryLO = DIRTYON;
    }

    /* stack page */
    sup->sup_privatePgTbl[31].pte_entryHI = (0xBFFFF) | (asid << ASIDSHIFT);
    sup->sup_privatePgTbl[31].pte_entryLO = DIRTYON;

    /* -------- EXCEPTION CONTEXT -------- */
    sup->sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr)pager;
    sup->sup_exceptContext[PGFAULTEXCEPT].status = MSTATUS_MPP_M | MSTATUS_MPIE_MASK;
    sup->sup_exceptContext[PGFAULTEXCEPT].stackPtr =
        (memaddr)&sup->sup_stackTLB[499];

    sup->sup_exceptContext[GENERALEXCEPT].pc = (memaddr)supgeneralExceptionHandler;
    sup->sup_exceptContext[GENERALEXCEPT].status = MSTATUS_MPP_M | MSTATUS_MPIE_MASK;
    sup->sup_exceptContext[GENERALEXCEPT].stackPtr =
        (memaddr)&sup->sup_stackGen[499];

    /* -------- INITIAL STATE -------- */
    state_t state;

    state.pc_epc = 0x800000B0;
    state.gpr[2] = 0xC0000000;
    state.status = MSTATUS_MPP_U | MSTATUS_MPIE_MASK;
    state.entry_hi = asid << ASIDSHIFT;

    SYSCALL(CREATEPROCESS, (int)&state, (int)sup, 0);
}











