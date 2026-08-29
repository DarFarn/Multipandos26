#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include <uriscv/const.h>
#include "../headers/types.h"
#include "../headers/const.h"
#include "../phase2/headers/interrupts.h"
#include "headers/support.h"
#include "../headers/klog.h"
#include "../headers/klog.h"

/* getDeviceRegAddr() (phase2/interrupts.c) expects the small interrupt
   *line* number (Table 1 of the spec: disk=3, flash=4, ...), not the
   IL_FLASH interrupt *exception code* (18) used for interrupt routing;
   the two are related by "line = exception code - 14" for device lines. */
#define FLASH_INTLINE (IL_FLASH - 14)

swap_t swap_pool[POOLSIZE];
int swapSem;

/* FIFO page replacement algorithm: whenever a frame is needed, simply
   pick the next one, round robin. */
static int nextFrame = 0;

/* Physical address of the first Swap Pool frame. Computed at boot time
   from the actual installed RAM size (RAMTOP), rather than guessed, so
   the Swap Pool can never overlap the kernel's own .data/.bss: it is
   carved out of the top of RAM, right below the frames the Instantiator
   process reserves for its own stack. */
static memaddr swapPoolStart;

void initSwapStructs() {
    int i;
    memaddr ramtop;

    for (i = 0; i < POOLSIZE; i++) {
        swap_pool[i].sw_asid = -1;
        swap_pool[i].sw_pageNo = -1;
        swap_pool[i].sw_pte = NULL;
    }

    RAMTOP(ramtop);
    swapPoolStart = ramtop - (TESTSTACKFRAMES * PAGESIZE) - (POOLSIZE * PAGESIZE);

    klog_print("ramtop=");
    klog_print_hex(ramtop);
    klog_print(" swapPoolStart=");
    klog_print_hex(swapPoolStart);
    klog_print("\n");

    swapSem = 1;
    nextFrame = 0;
}

/* Reads/writes one 4Kb block from/to the flash device dedicated to the
   given ASID (flash device number == asid - 1). "command" is FLASHREAD
   or FLASHWRITE, "block" is the flash block (== logical page) number and
   "frameAddr" is the physical address of the RAM frame involved. Returns
   the device's completion status. */
static int flashRW(int asid, int block, memaddr frameAddr, int command) {
    unsigned int *devReg = getDeviceRegAddr(FLASH_INTLINE, asid - 1);
    unsigned int commandValue;
    int status;

    SYSCALL(PASSEREN, (int) &flashSem[asid - 1], 0, 0);

    devReg[2] = (unsigned int) frameAddr; /* DATA0 */
    commandValue = (block << 8) | command;

    status = SYSCALL(DOIO, (int) &devReg[1], (int) commandValue, 0); /* COMMAND */

    SYSCALL(VERHOGEN, (int) &flashSem[asid - 1], 0, 0);

    return status;
}

/* Returns the logical page number (0..31) of the address currently
   loaded in EntryHi, i.e. the page whose translation just missed the
   TLB. Pages [0..30] are the .text/.data pages, anything else (in
   particular the stack, at 0xBFFFF000) is treated as page 31. */
static int getPageNumber() {
    unsigned int vpn = getENTRYHI() >> VPNSHIFT;
    unsigned int firstPage = KUSEG >> VPNSHIFT;
    unsigned int lastPage = firstPage + (USERPGTBLSIZE - 2);

    if (vpn >= firstPage && vpn <= lastPage)
        return (int) (vpn - firstPage);

    return USERPGTBLSIZE - 1;
}

/* The Pager: Support Level TLB exception handler. Implements the 14-step
   algorithm described in the Phase 3 spec, Section 4.2. */
void pager() {

    support_t *sup = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_t *savedState = &sup->sup_exceptState[PGFAULTEXCEPT];
    int p, frame, status;
    memaddr frameAddr;
    unsigned int statusReg;

    /* Page Table entries are always marked dirty (writable), so a
       TLB-Modification exception should never legitimately occur; every
       TLB exception reaching the Pager is therefore handled as an
       ordinary page fault. */

    klog_print("PAGER enter asid=");
    klog_print_dec((unsigned int) sup->sup_asid);
    klog_print(" pc=");
    klog_print_hex(savedState->pc_epc);
    klog_print("\n");

    SYSCALL(PASSEREN, (int) &swapSem, 0, 0);

    p = getPageNumber();

    /* pick a frame (FIFO) */
    frame = nextFrame;
    nextFrame = (nextFrame + 1) % POOLSIZE;
    frameAddr = swapPoolStart + (frame * PAGESIZE);

    klog_print("PAGER page=");
    klog_print_dec((unsigned int) p);
    klog_print(" frame=");
    klog_print_dec((unsigned int) frame);
    klog_print(" occupant=");
    klog_print_dec((unsigned int) (swap_pool[frame].sw_asid + 1));
    klog_print("\n");

    /* if the frame is occupied, evict its current owner */
    if (swap_pool[frame].sw_asid != -1) {
        /* mark the old owner's PTE invalid ... */
        swap_pool[frame].sw_pte->pte_entryLO &= ~VALIDON;

        /* ... and, atomically, drop the stale TLB entry (if cached) */
        statusReg = getSTATUS();
        setSTATUS(statusReg & ~MSTATUS_MIE_MASK);
        TLBCLR();
        setSTATUS(statusReg);

        /* write the (possibly dirty) frame back to its owner's backing
           store before reusing it */
        status = flashRW(swap_pool[frame].sw_asid, swap_pool[frame].sw_pageNo, frameAddr, FLASHWRITE);
        if (status != READY) {
            klog_print("PAGER evict-write FAIL status=");
            klog_print_hex((unsigned int) status);
            klog_print("\n");
            SYSCALL(VERHOGEN, (int) &swapSem, 0, 0);
            terminateUProc();
            return;
        }
    }

    /* bring the missing page in from the Current Process's own backing
       store */
    status = flashRW(sup->sup_asid, p, frameAddr, FLASHREAD);
    if (status != READY) {
        klog_print("PAGER read FAIL asid=");
        klog_print_dec((unsigned int) sup->sup_asid);
        klog_print(" page=");
        klog_print_dec((unsigned int) p);
        klog_print(" status=");
        klog_print_hex((unsigned int) status);
        klog_print("\n");
        SYSCALL(VERHOGEN, (int) &swapSem, 0, 0);
        terminateUProc();
        return;
    }

    klog_print("PAGER done asid=");
    klog_print_dec((unsigned int) sup->sup_asid);
    klog_print(" page=");
    klog_print_dec((unsigned int) p);
    klog_print("\n");

    /* update the Swap Pool table */
    swap_pool[frame].sw_asid = sup->sup_asid;
    swap_pool[frame].sw_pageNo = p;
    swap_pool[frame].sw_pte = &sup->sup_privatePgTbl[p];

    /* update the Current Process's Page Table entry and, atomically,
       the TLB */
    sup->sup_privatePgTbl[p].pte_entryLO = frameAddr | DIRTYON | VALIDON;

    statusReg = getSTATUS();
    setSTATUS(statusReg & ~MSTATUS_MIE_MASK);
    TLBCLR();
    setSTATUS(statusReg);

    SYSCALL(VERHOGEN, (int) &swapSem, 0, 0);

    LDST(savedState);
}
