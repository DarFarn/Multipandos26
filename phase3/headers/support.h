#ifndef SUPPORT_H_INCLUDED
#define SUPPORT_H_INCLUDED

#include "../../headers/types.h"

/* NOTE: support_t, swap_t, context_t and pteEntry_t are already declared
 * in headers/types.h and must not be redefined here.
 */

/* ===================== vmSupport.c ===================== */

/* Initializes the Swap Pool table and the swapSem mutex.
   Called once by test() before any U-proc is launched. */
void initSwapStructs();

/* The Pager: Support Level TLB exception handler (page fault handler). */
void pager();

/* ===================== sysSupport.c ===================== */

/* Support Level general exception handler: entry point reached via
   sup_exceptContext[GENERALEXCEPT]; dispatches to the SYSCALL handler
   or to the Program Trap handler. */
void supGeneralExceptionHandler();

/* Terminates the currently executing U-proc, waking up whoever is
   waiting for it (the shell, or the InstantiatorProcess for the shell
   itself). Shared by SYS2 and by the Program Trap handler. */
void terminateUProc();

/* ===================== initProc.c ===================== */

/* InstantiatorProcess: replaces the Phase 2 test() placeholder. */
void test();

/* Allocates and launches (NSYS1) the U-proc with the given ASID. */
void initUProc(int asid);

/* ===================== Support Level global variables ===================== */

extern support_t support_structs[UPROCMAX];

extern int masterSem; /* V'ed by the shell when it terminates            */
extern int shellSem;  /* V'ed by any U-proc launched by the shell         */
extern int swapSem;   /* mutual exclusion over the Swap Pool table        */
extern int termReadSem;  /* mutual exclusion over the terminal's receiver */
extern int termWriteSem; /* mutual exclusion over the terminal's transmitter */

extern swap_t swap_pool[POOLSIZE];

#endif
