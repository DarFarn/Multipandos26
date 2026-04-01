/// @brief aggiorna il tempo di CPU usato dal processo corrente
void updateCPUTime(); // !!! startTime va aggiornato dallo schedule quando disptcha un processo

/// @brief NSYS1
/// @param state stato del processo nel momento dell'eccezione
void createProcess(state_t *state);

/// @brief uccide il processo p e tutta la sua progene
/// @param p processo da uccidere
void processKiller(pcb_t *p);

/// @brief trova il pcb del processo con pid speficato
/// @param pid 
/// @return pcb del processo p_id = pid
pcb_t *findProcess(int pid);

/// @brief helper di findProcess, dato un nodo di partenza, cerca il processo con pid specificato in tutto il sottoalbero
/// @param root processo radice da cui iniziare la ricerca
/// @param pid pid del processo da trovare
/// @return 
pcb_t *processFinder(pcb_t *root, int pid);

/// @brief NSYS2
/// @param state stato del processore nel momento dell'eccezione 
void terminateProcess(state_t *state);

/// @brief NSYS3
/// @param state stato del processore nel momento dell'eccezione 
void passeren(state_t *state);

/// @brief NSYS4
/// @param state stato del processore nel momento dell'eccezione 
void verhogen(state_t *state);

/// @brief fa una P sul semaforo specificato, per operazioni di I/O
/// @param semAdd indirizzo del semaforo su cui fare la P
void semaphoreP(int *semAdd);

/// @brief fa una V sul semaforo specificato, per operazioni di I/O
/// @param semAdd indirizzo del semaforo su cui fare la V
void semaphoreV(int *semAdd);

/// @brief NSYS5
/// @param state stato del processore nel momento dell'eccezione
void doIO(state_t *state);

/// @brief NSYS6
/// @param state stato del processore nel momento dell'eccezione 
void getCPUtime(state_t *state);

/// @brief NSYS7
/// @param state stato del processore nel momento dell'eccezione 
void waitForClock(state_t *state);

/// @brief NSYS8
/// @param state stato del processore nel momento dell'eccezione 
void getSupportData(state_t *state);

/// @brief NSYS9
/// @param state stato del processore nel momento dell'eccezione 
void getProcessID(state_t *state);

/// @brief NSYS10
/// @param state stato del processore nel momento dell'eccezione
void yield(state_t *state);


/// @brief big gestore principale delle eccezioni, le smista
void exceptionHandler();

/// @brief implementazione della logica di pass up or die in caso di eccezione
/// @param index indice del tipo di eccezione (program trap, TLB miss, interrupt)
void passUpOrDie(int index);

void tlbHandler();

void programTrapHandler();

void syscallHandler(state_t *state);
