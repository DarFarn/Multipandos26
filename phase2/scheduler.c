//In its simplest form whenever the Scheduler is called it should dispatch the “next” process in the Ready Queue.
//1. Remove the PCB from the head of the Ready Queue and store the pointer to the PCB in the
//Current Process field.
//2. Load 5 milliseconds on the PLT [Section 7.2].
//3. Perform a Load Processor State (LDST) [Section 13.2] on the processor state stored in PCB of
//the Current Process (p_s) of the current CPU.
//Dispatching a process transitions it from a “ready” process to a “running” process
