
//In its simplest form whenever the Scheduler is called it should dispatch the “next” process in the Ready Queue.
//1. Remove the PCB from the head of the Ready Queue and store the pointer to the PCB in the
//Current Process field.
pcb t *outProcQ(struct list head *readyQueue, pcb t *p)
//2. Load 5 milliseconds on the PLT [Section 7.2].
LDIT(5)
//3. Perform a Load Processor State (LDST) [Section 13.2] on the processor state stored in PCB of
//the Current Process (p_s) of the current CPU.
LDST(p_s);
//Dispatching a process transitions it from a “ready” process to a “running” process
//??
if (emptyProcQ(*readyQueue)){

