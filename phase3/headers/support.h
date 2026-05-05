#ifndef SUPPORT_H
#define SUPPORT_H

#include <uriscv/liburiscv.h>

#define UPROCMAX 8
#define PGFAULTEXCEPT 0
#define GENERALEXCEPT 1

typedef struct {
    int sup_asid;

    state_t sup_exceptState[2];
    context_t sup_exceptContext[2];

    pteEntry_t sup_privatePgTbl[32];

    int sup_stackTLB[500];
    int sup_stackGen[500];
} support_t;

/* Swap pool entry */
typedef struct {
    int asid;
    int vpn;
    pteEntry_t *pte;
} swap_t;

/* extern globals */
extern swap_t swap_pool[];
extern int swapSem;
extern int masterSem;
extern int shellSem;

#endif











