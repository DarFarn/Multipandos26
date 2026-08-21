#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include <uriscv/const.h>
#include "../headers/types.h"
#include "../headers/const.h"
#include "headers/support.h"

/* Pool of Support Structures, one per possible ASID [1..UPROCMAX].
   support_structs[asid - 1] is reused every time a U-proc with that
   ASID is (re)launched. */
support_t support_structs[UPROCMAX];

int masterSem = 0;
int shellSem = 0;
int termReadSem = 1;
int termWriteSem = 1;

/* ===================== TEST (InstantiatorProcess) ===================== */

void test() {

    initSwapStructs();

    masterSem = 0;
    shellSem = 0;
    termReadSem = 1;
    termWriteSem = 1;

    /* launch the shell; it is the only U-proc allowed to request SYS6 */
    initUProc(SHELLASID);

    /* wait for the shell to terminate */
    SYSCALL(PASSEREN, (int)&masterSem, 0, 0);

    /* no more U-procs left: driving processCount to 0 triggers HALT */
    SYSCALL(TERMPROCESS, 0, 0, 0);
}

/* ===================== INIT UPROC ===================== */

void initUProc(int asid) {

    support_t *sup = &support_structs[asid - 1];
    int i;

    sup->sup_asid = asid;

    /* -------- PAGE TABLE INIT (Section 2.1) -------- */
    /* pages 0..30: .text and .data, VPN starting at 0x80000000 */
    for (i = 0; i < USERPGTBLSIZE - 1; i++) {
        sup->sup_privatePgTbl[i].pte_entryHI = (KUSEG + (i << VPNSHIFT)) | (asid << ASIDSHIFT);
        sup->sup_privatePgTbl[i].pte_entryLO = DIRTYON; /* D=1, V=0: not yet present */
    }

    /* page 31: the stack page, at 0xBFFFF000 */
    sup->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryHI = (USERSTACKTOP - PAGESIZE) | (asid << ASIDSHIFT);
    sup->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryLO = DIRTYON;

    /* -------- EXCEPTION CONTEXTS (Section 9.1.2) -------- */
    sup->sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr) pager;
    sup->sup_exceptContext[PGFAULTEXCEPT].status = MSTATUS_MPP_M | MSTATUS_MIE_MASK | MSTATUS_MPIE_MASK;
    sup->sup_exceptContext[PGFAULTEXCEPT].stackPtr = (memaddr) &sup->sup_stackTLB[499];

    sup->sup_exceptContext[GENERALEXCEPT].pc = (memaddr) supGeneralExceptionHandler;
    sup->sup_exceptContext[GENERALEXCEPT].status = MSTATUS_MPP_M | MSTATUS_MIE_MASK | MSTATUS_MPIE_MASK;
    sup->sup_exceptContext[GENERALEXCEPT].stackPtr = (memaddr) &sup->sup_stackGen[499];

    /* -------- INITIAL PROCESSOR STATE (Section 9.1.1) -------- */
    state_t newState;

    newState.pc_epc = UPROCSTARTADDR;
    newState.reg_sp = USERSTACKTOP;
    newState.status = MSTATUS_MPP_U | MSTATUS_MIE_MASK | MSTATUS_MPIE_MASK;
    newState.mie = MIE_ALL;
    newState.entry_hi = asid << ASIDSHIFT;

    SYSCALL(CREATEPROCESS, (int) &newState, PROCESS_PRIO_LOW, (int) sup);
}










