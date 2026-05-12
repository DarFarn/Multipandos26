#include "../headers/types.h"
#include <uriscv/liburiscv.h>
#include "../headers/const.h"
#include <uriscv/types.h>
#include <uriscv/const.h>
#include "../phase2/headers/exceptions.h"
#include "../phase3/headers/support.h"

//swap pool table is local to this module quindi da initproc la mettiamo qua?

//swap pool table:
//tabella con una entry per ogni frame della swap pool (=set di frame lasciati da parte per supportare 
//la memoria virtuale, composta da tre colonne con 1.ASID dell'UPROC che occupa il frame 2. VPN della pagina occupante 3. pointer alla
//entry corrispondente a quell'ASID ma nella page table (= strutt. dati che mappa indirizzi fisici usati da un processo a indirizzi in RAM)

//concretamente la implementerò quindi come un array di struct tipo:

swap_t swap_pool[POOLSIZE]; //ricordarsi di aumentare la size effettivamente ad ogni frame 

int swapSem; //swapmutexsemaphore

int flashRW(int asid, int page, int frame, const RW) {

support_t *sup = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0,0);  //get support structure corrispondente al device
unsigned int physAddr = (unsigned int) frame;//computa indirizzo fisico del frame (assumendo che sia allineato a 4k)

SYSCALL(DOIO, asid, DATA0, physAddr); //scrivi sul campo DATA0 l'indirizzo fisico appropriato del blocco di 4k da scrivere o leggere

unsigned int command = (blocknumber << 8 | RW); // costruisci command mettendo nei 3 bytes alti il block number e in quelli bassi il comando READ?
int status = SYSCALL(DOIO, asid, COMMAND, command);
return status;
}

int nextFrame = 0;

int getPageNumber(){
unsigned int vpn = getENTRYHI();
unsigned int p;

    if (vpn >= 0x80000 && vpn <= 0x8001E)
       return p = vpn - 0x80000;
    else
       return p = 31;
}

#define POOLSIZE (2 * UPROCMAX)

swap_t swap_pool[POOLSIZE];
int swapSem = 1;

void initSwapStructs() {
    for (int i = 0; i < POOLSIZE; i++) {
        swap_pool[i].sw_asid = -1;
        swap_pool[i].sw_pageNo= -1;
        swap_t *s = &swap_pool[i];
        s->sw_pte = NULL;
    }
}


//scrivere pager secondo algoritmo di 16 passi, questo scheletro semplificato da rivedere

void pager() {

state_t *saved = GET_EXCEPTION_STATE_PTR(0);

    unsigned int p = getPageNumber();
    
    support_t *sup = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    SYSCALL(PASSEREN, (int)&swapSem, 0, 0);

    int frame = nextFrame;
    nextFrame = (nextFrame + 1) % POOLSIZE;

    /* if occupied → invalidate */
    if (swap_pool[frame].sw_asid != -1) {
        swap_pool[frame].sw_pte->pte_entryLO &= ~VALIDON;
    }

    /* load page from flash (stub) */
    flashRW(sup->sup_asid, p, frame, FLASHREAD);

    /* update swap table */
    swap_pool[frame].sw_asid = sup->sup_asid;
    swap_pool[frame].sw_pageNo = p;
    swap_pool[frame].sw_pte = &sup->sup_privatePgTbl[p];

    /* update page table */
    sup->sup_privatePgTbl[p].pte_entryLO |= VALIDON;

    /* update TLB (easy version) */
    TLBCLR();

    SYSCALL(VERHOGEN, (int)&swapSem, 0, 0);

    LDST(saved);
}



 int sw_asid;        /* ASID number			*/
    int sw_pageNo;      /* page's virt page no.	*/
    pteEntry_t *sw_pte; /* page's PTE entry.	*/








