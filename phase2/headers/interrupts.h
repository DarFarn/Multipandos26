//Calcolo dell'inidirizzo del device’s device register
unsigned int getDeviceRegAddr(int intLineNo, int devNo);

//Funzione che trova il device con proprità più alta. Ritorna -1 se non c'è
int getHighestPriorityDevice(unsigned int bitMap);

//Semaphore index per un device
//Linee 3-6: 4 linee * 8 devices --> 32 semafoti (0-31)
//Ogni Terminal Device ha 2 semafori --> a 8 terminal corrispondono 16 semafori (32-47)
int getSemaphoreIndex(int intLineNo, int devNo, int isTransmitter);

//Handles device interrupts (lines 3-7): acknowledges interrupt, performs V on device 
//semaphore to unblock waiting PCB, returns device status in a0 and resumes Current Process 
//or calls scheduler 
void handleNonTimerInterrupt(int intLineNo, unsigned int bitMap);

//Processor Local Timer (PLT) Interrupts
void handlePLTInterrupt();

//The System-wide Interval Timer and the Pseudo-clock
void handleIntervalTimerInterrupt();

//Find highest priority pending interrupt 
int findHighestPriorityInterrupt(unsigned int *bitMap);

//Main Interrupt Exception Handler 
//Chimato da exceptionHandler quando CAUSE_IS_INT è true
void interruptHandler();
