
#include "../headers/listx.h"
#include "../headers/const.h"
#include "../headers/types.h"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include <uriscv/const.h>
#include "../headers/klog.h"
#include "../phase2/headers/exceptions.h"
#include "../phase2/headers/scheduler.h"
#include "../phase3/headers/support.h"


void uTLB_RefillHandler(){ //uTLB_RefillHandler (cosa intende per dovrebbe ancora trovarsi in exceptions.c? prob devo scriverla e poi metterla lì)
//1.recupera the saved exception state located at the start of the BIOS Data Page.
state_t *saved = GET_EXCEPTION_STATE_PTR(0) //dovrebbe essere la macro giusta ma sta da capire come variare numero CPU
//2. estrai la pagina mancante dalla parte di entryhi di saved 
unsigned int vpn = saved->entry_hi;
unsigned int p;
//offset dato dai vpn in teoria?
if (vpn >= 0x80000 && vpn <= 0x8001E){
p = vpn - 0x80000;
else if (vpn == 0xBFFFF)
p = 31; }


//3. recupera la page table del processo corrente
support_t *sup = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0 ,0);
pteEntry_t *pt = sup->sup_privatePgTbl;
//4. Prendi la entry corrispondente nella page table (ma si chiama pte nel progetto?)
pteEntry_t pte = pt[vpn];
//5. Scrivi nella TLB
setENTRYHI(pte.entry_hi);
setENTRYLO(pte.entry_lo);
TLBWR(); //TLBWR funziona così?

//6. riprendi il processo da saved
LDST(saved);

//bozza di refillhandler finita



//scrivere pager secondo algoritmo di 16 passi, vedi su chatgpt












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




