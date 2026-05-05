#include "../phase3/headers/support.h"

void uTLB_RefillHandler() {

    state_t *saved = GET_EXCEPTION_STATE_PTR(0);

    unsigned int vpn = saved->entry_hi;
    unsigned int p;

    if (vpn >= 0x80000 && vpn <= 0x8001E)
        p = vpn - 0x80000;
    else
        p = 31;

    support_t *sup = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    pteEntry_t pte = sup->sup_privatePgTbl[p];

    setENTRYHI(pte.entry_hi);
    setENTRYLO(pte.entry_lo);
    TLBWR();

    LDST(saved);
}

#define POOLSIZE (2 * UPROCMAX)

swap_t swap_pool[POOLSIZE];
int swapSem = 1;
int nextFrame = 0;

void initSwapStructs() {
    for (int i = 0; i < POOLSIZE; i++) {
        swap_pool[i].asid = -1;
        swap_pool[i].vpn = -1;
        swap_pool[i].pte = NULL;
    }
}


//scrivere pager secondo algoritmo di 16 passi, questo scheletro semplificato da rivedere

void pager() {

    support_t *sup = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    state_t *state = &sup->sup_exceptState[PGFAULTEXCEPT];

    unsigned int vpn = state->entry_hi;
    unsigned int p;

    if (vpn >= 0x80000 && vpn <= 0x8001E)
        p = vpn - 0x80000;
    else
        p = 31;

    SYSCALL(PASSEREN, (int)&swapSem, 0, 0);

    int frame = nextFrame;
    nextFrame = (nextFrame + 1) % POOLSIZE;

    /* if occupied → invalidate */
    if (swap_pool[frame].asid != -1) {
        swap_pool[frame].pte->entry_lo &= ~VALIDON;
    }

    /* load page from flash (stub) */
    flashRead(sup->sup_asid, p, frame);

    /* update swap table */
    swap_pool[frame].asid = sup->sup_asid;
    swap_pool[frame].vpn = p;
    swap_pool[frame].pte = &sup->sup_privatePgTbl[p];

    /* update page table */
    sup->sup_privatePgTbl[p].entry_lo |= VALIDON;

    /* update TLB (easy version) */
    TLBCLR();

    SYSCALL(VERHOGEN, (int)&swapSem, 0, 0);

    LDST(state);
}











//swap pool table is local to this module quindi da initproc la mettiamo qua?

//swap pool table:
//tabella con una entry per ogni frame della swap pool (=set di frame lasciati da parte per supportare 
//la memoria virtuale, composta da tre colonne con 1.ASID dell'UPROC che occupa il frame 2. VPN della pagina occupante 3. pointer alla
//entry corrispondente a quell'ASID ma nella page table (= strutt. dati che mappa indirizzi fisici usati da un processo a indirizzi in RAM)

//concretamente la implementerò quindi come un array di struct tipo:

typedef struct{
int ASID; 
int vpn;
pteEntry_t *pte;
} swap_t;

swap_t swap_pool[SWAP POOL SIZE]; //ricordarsi di aumentare la size effettivamente ad ogni frame 

int swapSem; //swapmutexsemaphore

int flashRead(int asid, int page, int frame);
int flashWriteW(int asid, int page, int frame);

int nextFrame = 0;

initSwapStructs();//chiamato da test, capire cosa deve fare




