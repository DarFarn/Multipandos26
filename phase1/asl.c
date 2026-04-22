#include "./headers/asl.h"
#include "./headers/pcb.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h; //lista dei semafori liberi
static struct list_head semd_h; //lista dei semafori attivi

void initASL() {
    INIT_LIST_HEAD(&semdFree_h);
    INIT_LIST_HEAD(&semd_h);
    for (int i = 0; i < MAXPROC; i++) {
        INIT_LIST_HEAD(&semd_table[i].s_procq);
        INIT_LIST_HEAD(&semd_table[i].s_link);
        list_add_tail(&semd_table[i].s_link, &semdFree_h);
    }
}

// Controlla se il semaforo con key semAdd è attivo
static semd_t* findSemd(int* semAdd) {
    struct list_head* pos;
    semd_t* active;
    
    list_for_each(pos, &semd_h) {
        active = container_of(pos, semd_t, s_link);
        if (active->s_key == semAdd) {
            return active;
        }
    }
    return NULL;
}

int insertBlocked(int* semAdd, pcb_t* p) {
    if (semAdd == NULL || p == NULL) {
        return 1; // Error condition
    }
    
    semd_t* semd = findSemd(semAdd);
    
    if (semd == NULL) { //semaforo inattivo

        if (list_empty(&semdFree_h)) {
            return 1; // non ci sono semafori liberi, nulla da fare
        }

        struct list_head* entry = semdFree_h.next; //prendo il primo semaforo libero
        list_del(entry);
        semd = container_of(entry, semd_t, s_link);
        

        semd->s_key = semAdd;
        mkEmptyProcQ(&semd->s_procq); //inizializza la lista dei processi bloccati del semaforo semd come vuota
        
        // inserisce il semaforo nella lista degli ASL (ordinato per key address)
        struct list_head* pos;
        semd_t* asl_entry;
        int done = 0;
        
        list_for_each(pos, &semd_h) {
            asl_entry = container_of(pos, semd_t, s_link);
            if (asl_entry->s_key > semAdd) {
                __list_add(&semd->s_link, pos->prev, pos);
                done = 1;
                break;
            }
        }
        
        if (!done) { //chiave minima
            list_add_tail(&semd->s_link, &semd_h);
        }
    }
    p->p_semAdd = semAdd; //associa al semaforo
    insertProcQ(&semd->s_procq, p);
    
    return 0;
}

pcb_t* removeBlocked(int* semAdd) {
    if (semAdd == NULL) {
        return NULL;
    }
    semd_t* semd = findSemd(semAdd);
    if (semd == NULL) {
        return NULL;
    }
    pcb_t* p = removeProcQ(&semd->s_procq);
    
    if (p != NULL) {
        p->p_semAdd = NULL;
    }

    if (emptyProcQ(&semd->s_procq)) { // se p era l'ultimo processo rimasto nella process queue
        list_del(&semd->s_link); //rimuove il semaforo dalla lista degli ASL
        INIT_LIST_HEAD(&semd->s_link);
        list_add_tail(&semd->s_link, &semdFree_h);
    }
    
    return p;
}

pcb_t* outBlocked(pcb_t* p) {
    if (p == NULL || p->p_semAdd == NULL) {
        return NULL;
    }

    semd_t* semd = findSemd(p->p_semAdd); // il suo semaforo
    
    if (semd == NULL) {
        return NULL;
    }

    pcb_t* removed = outProcQ(&semd->s_procq, p);
    
    if (removed == NULL) {
        return NULL; // pcb non trovato
    }
    removed->p_semAdd = NULL;
    if (emptyProcQ(&semd->s_procq)) {
        list_del(&semd->s_link);
        INIT_LIST_HEAD(&semd->s_link);
        list_add_tail(&semd->s_link, &semdFree_h);
    }
    
    return removed;
}

pcb_t* headBlocked(int* semAdd) {
    if (semAdd == NULL) {
        return NULL;
    }
    semd_t* semd = findSemd(semAdd);
    if (semd == NULL || emptyProcQ(&semd->s_procq)) {
        return NULL;
    }

    return headProcQ(&semd->s_procq);
}
