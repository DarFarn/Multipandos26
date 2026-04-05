#include "./headers/pcb.h"

static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 1;
static const pcb_t null_pcb={0};

void initPcbs() {
    INIT_LIST_HEAD(&pcbFree_h); //inizializza la lista pcbFree_h come vuota
    for (int i = 0; i < MAXPROC; i++) {
        INIT_LIST_HEAD(&pcbFree_table[i].p_list);   //
        INIT_LIST_HEAD(&pcbFree_table[i].p_child); //inizializza le liste di tutti i campi come vuote per ogni i-esimo processo
        INIT_LIST_HEAD(&pcbFree_table[i].p_sib);  //
        list_add_tail(&pcbFree_table[i].p_list, &pcbFree_h); //aggiunge il processo iniziallizzato alla lista pcbFree_h
    }
}

void freePcb(pcb_t* p) {
    if (p == NULL) return;
    INIT_LIST_HEAD(&p->p_list);
    INIT_LIST_HEAD(&p->p_child); // resettiamo tutto
    INIT_LIST_HEAD(&p->p_sib);
    list_add_tail(&p->p_list, &pcbFree_h); //aggiunge il processo alla lista pcbFree_h
}

void *memset(void *s, int c, unsigned int n) {
    unsigned char *p=s;
    while (n--) {
        *p++=(unsigned char)c;
    }
    return s;
}

pcb_t* allocPcb() {
    if (list_empty(&pcbFree_h)) {
        return NULL; //se la lista dei processi liberi e' vuota
    }
    struct list_head* first = pcbFree_h.next; //prende il primo processo libero dalla lista pcbFree_h, che è preceduto dalla sentinella
    list_del(first);
    pcb_t* p = container_of(first, pcb_t, p_list); // preleva il pcp_t associato all'elemento list_head

    //inizializzazione/reset
    memset(p, 0, sizeof(pcb_t));
    INIT_LIST_HEAD(&p->p_list);
    INIT_LIST_HEAD(&p->p_child);
    INIT_LIST_HEAD(&p->p_sib);
    p->p_pid = next_pid++;  
    
    return p;
}

void mkEmptyProcQ(struct list_head* head) {
    INIT_LIST_HEAD(head);
}

int emptyProcQ(struct list_head* head) {
    return list_empty(head);
}

void insertProcQ(struct list_head* head, pcb_t* p) {
    if (head == NULL || p == NULL) return;

    //controllo che il processo che voglio inserire non sia già presente in un altra process queue
    if ((p->p_list.next != &p->p_list || p->p_list.prev != &p->p_list) &&
        p->p_list.next != NULL && p->p_list.prev != NULL) {
        list_del(&p->p_list);
    }

    INIT_LIST_HEAD(&p->p_list);
    
    // caso process queue vuota
    if (list_empty(head)) {
        list_add_tail(&p->p_list, head);
        return;
    }

    struct list_head* pos;
    pcb_t* candidate;

    list_for_each(pos, head) { //for che itera su ogni elemento della lista head
        candidate = container_of(pos, pcb_t, p_list); //arriviamo alla struttura associata
        if (candidate->p_prio < p->p_prio) {
            __list_add(&p->p_list, pos->prev, pos);
            return;
        }
    }

    //caso ==
    list_for_each_prev(pos, head) {
        candidate = container_of(pos, pcb_t, p_list);
        if (candidate->p_prio == p->p_prio) {
            __list_add(&p->p_list, pos, pos->next);
            return;
        }
    }
    
    // p va in fondo
    list_add_tail(&p->p_list, head);
}

pcb_t* headProcQ(struct list_head* head) {
    if (head == NULL || list_empty(head)) {
        return NULL;
    }
    return container_of(head->next, pcb_t, p_list);
}

pcb_t* removeProcQ(struct list_head* head) {
    if (head == NULL || list_empty(head)) {
        return NULL;
    }
    
    struct list_head* first = head->next;
    list_del(first);
    return container_of(first, pcb_t, p_list);
}

pcb_t* outProcQ(struct list_head* head, pcb_t* p) {
    if (head == NULL || p == NULL) {
        return NULL;
    }

    struct list_head* candidate;
    list_for_each(candidate, head) {
        if (candidate == &p->p_list) {
            list_del(candidate);
            return p;
        }
    }
    
    return NULL;
}

int emptyChild(pcb_t* p) {
    if (p == NULL) return 0;
    return list_empty(&p->p_child);
}

void insertChild(pcb_t* prnt, pcb_t* p) {
    if (prnt == NULL || p == NULL) return;

    if (p->p_parent != NULL) { // se ha già un'altro genitore
        outChild(p);
    }
    
    //se aveva un altro genitore allora aveva anche altri fratelli, devo rimuovere anche quelli
    if ((p->p_sib.next != &p->p_sib || p->p_sib.prev != &p->p_sib) &&
        p->p_sib.next != NULL && p->p_sib.prev != NULL) {
        list_del(&p->p_sib);
    }
    INIT_LIST_HEAD(&p->p_sib);
    p->p_parent = prnt;
    list_add_tail(&p->p_sib, &prnt->p_child); //aggiunge p alla lista dei figli di prnt
}

pcb_t* removeChild(pcb_t* p) {
    if (p == NULL || list_empty(&p->p_child)) {
        return NULL;
    }
    struct list_head* first_child = p->p_child.next; //primo figlio di p, preceduto dalla sentinella
    pcb_t* child = container_of(first_child, pcb_t, p_sib);
    list_del(first_child);
    child->p_parent = NULL;
    
    return child;
}

pcb_t* outChild(pcb_t* p) {
    if (p == NULL || p->p_parent == NULL) {
        return NULL;
    }
    struct list_head* pos;
    list_for_each(pos, &p->p_parent->p_child) {
        if (pos == &p->p_sib) { // scorre la lista dei figli del suo genitore
            list_del(pos);
            p->p_parent = NULL;
            return p;
        }
    }
    
    return NULL;
}
