
#include "../phase3/headers/support.h"

support_t support_structs[UPROCMAX];

int masterSem = 0;
int shellSem = 0;

/* declared in vmSupport.c */
extern void initSwapStructs();

/* forward */
void initUProc(int asid);

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
    SYSCALL(PASSEREN, (int)&masterSem, 0, 0);

    /* terminate */
    SYSCALL(TERMINATE, 0, 0, 0);
}

/* ===================== INIT UPROC ===================== */
void initUProc(int asid) {

    support_t *sup = &support_structs[asid - 1];

    sup->sup_asid = asid;

    /* -------- PAGE TABLE INIT -------- */
    for (int i = 0; i < 31; i++) {
        sup->sup_privatePgTbl[i].entry_hi = (0x80000 + i) | (asid << ASIDSHIFT);
        sup->sup_privatePgTbl[i].entry_lo = DIRTYON;
    }

    /* stack page */
    sup->sup_privatePgTbl[31].entry_hi = (0xBFFFF) | (asid << ASIDSHIFT);
    sup->sup_privatePgTbl[31].entry_lo = DIRTYON;

    /* -------- EXCEPTION CONTEXT -------- */
    sup->sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr)pager;
    sup->sup_exceptContext[PGFAULTEXCEPT].c_status = MSTATUS_MPP_M | MSTATUS_MPIE_MASK;
    sup->sup_exceptContext[PGFAULTEXCEPT].c_stackPtr =
        (memaddr)&sup->sup_stackTLB[499];

    sup->sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr)generalExceptionHandler;
    sup->sup_exceptContext[GENERALEXCEPT].c_status = MSTATUS_MPP_M | MSTATUS_MPIE_MASK;
    sup->sup_exceptContext[GENERALEXCEPT].c_stackPtr =
        (memaddr)&sup->sup_stackGen[499];

    /* -------- INITIAL STATE -------- */
    state_t state;

    state.s_pc = 0x800000B0;
    state.s_sp = 0xC0000000;
    state.s_status = MSTATUS_MPP_U | MSTATUS_MPIE_MASK;
    state.s_entryHI = asid << ASIDSHIFT;

    SYSCALL(CREATEPROCESS, (int)&state, (int)sup, 0);
}












