
#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"

struct thread *find_running();
Tid yield (struct thread *from, struct thread *to);
void thread_stub (void (*thread_main)(void *), void *arg);
void destroy_e_q (struct thread *from);

/* This is the thread control block */
struct thread {
    ucontext_t mycontext;
    Tid tid;
    int tstate;
    void *dsp; //default stack pointer
    struct thread *next;
    int ran;
    int e_q;
    int dead;
};

struct thread *running_e;
struct thread *t_head;
struct thread *q_head;
struct thread m_thread;
Tid avail_id;

enum states{
    ready = 1,
    running = 2,
    exited = 3,
    killed = 4,
    sleep = 5
};


void
thread_init(void)
{
    int err;
    t_head = &m_thread;
    t_head->tid = 0;
    t_head->tstate = running;
    t_head->next = NULL;
    t_head->e_q = 0;
    t_head->dead = 0;
    t_head->ran = 1;
    q_head = NULL;
    running_e = NULL;
    avail_id = -1;
    err = getcontext(&t_head->mycontext);
    assert (!err);
}


Tid
thread_id()
{
    struct thread *running;
    running = find_running();
    if (running == NULL)
        return THREAD_INVALID;
    return running->tid;
}

void thread_stub (void (*thread_main)(void *), void *arg)
{
    interrupts_set(1);
    Tid ret;
    thread_main (arg);
    ret = thread_exit();
    assert (ret == THREAD_NONE);
    exit(0);
}

Tid
thread_create(void (*fn) (void *), void *parg)
{
    int enabled = interrupts_set(0);
    int err, i, new_id = 1; 
    struct thread *current = t_head->next;
    struct thread *previous;
    if (current == NULL)
    {
        struct thread *t = (struct thread*) malloc(sizeof (struct thread));
        void *t_stack = (void*)malloc (THREAD_MIN_STACK);
        if (t_stack == NULL)
        {
            interrupts_set(enabled);
            return THREAD_NOMEMORY;
        }
        t->dsp = t_stack;
        err = getcontext(&t->mycontext);
        assert(!err);
        t->mycontext.uc_mcontext.gregs[REG_RIP] = (unsigned long) thread_stub;
        t->mycontext.uc_mcontext.gregs[REG_RSP] = (unsigned long) (THREAD_MIN_STACK + t_stack - sizeof (unsigned long)); //////NEED TO UNDERSTAND
        t->mycontext.uc_mcontext.gregs[REG_RDI] = (unsigned long) fn;
        t->mycontext.uc_mcontext.gregs[REG_RSI] = (unsigned long) parg;
        t->ran = 0;
        t->tstate = ready;
        t_head->next = t;
        t->next = NULL;
        t->tid = new_id;
        interrupts_set(enabled);
        return t->tid;
    }
    for (i = 0 ; i < THREAD_MAX_THREADS ; i++)
    {
        if (new_id == 1024)
        {
            interrupts_set(enabled);
            return THREAD_NOMORE;
        }
        if (current == NULL)
        {
            struct thread *t = (struct thread*) malloc(sizeof (struct thread));
            void *t_stack = (void*)malloc (THREAD_MIN_STACK);
            if (t_stack == NULL)
            {
                interrupts_set(enabled);
                return THREAD_NOMEMORY;
            }
            t->dsp = t_stack;
            err = getcontext(&t->mycontext);
            assert(!err);
            t->mycontext.uc_mcontext.gregs[REG_RIP] = (unsigned long) thread_stub;
            t->mycontext.uc_mcontext.gregs[REG_RSP] = (unsigned long) (THREAD_MIN_STACK + t_stack - sizeof (unsigned long)); //////NEED TO UNDERSTAND
            t->mycontext.uc_mcontext.gregs[REG_RDI] = (unsigned long) fn;
            t->mycontext.uc_mcontext.gregs[REG_RSI] = (unsigned long) parg;
            t->ran = 0;
            t->tstate = ready;
            previous->next = t;
            t->next = NULL;
            t->tid = new_id;
            interrupts_set(enabled);
            return t->tid;
        }
        previous = current;
        current = current->next;
        new_id ++;
    }    
    interrupts_set(enabled);
    return THREAD_NOMORE;
}

struct 
thread *find_running()
{
    int i;
    if (running_e != NULL)
        return running_e;
    struct thread *current = t_head;
    for (i = 0; i < THREAD_MAX_THREADS - 1 ;i++)
    {
        if (current->tstate == running)
            return current;
        else if (current == NULL)
            return NULL;
        else 
            current = current->next;
    }
    return NULL;
}


Tid 
yield (struct thread *from, struct thread *to)
{
    volatile int setcontext_called = 0;
    int err, ret;
    from->tstate = ready;
    to->tstate = running;
    running_e = NULL;
    ret = to->tid;
    to->ran = 1;
    err = getcontext(&from->mycontext);
    assert (!err);
    if (setcontext_called == 0)
    {
        setcontext_called = 1;
        err = setcontext(&to->mycontext);
        assert (!err);
    }
    return ret;
    
}

/*Tid 
yield_killed (struct thread *from, struct thread *to)
{
    int ret;
    from->tstate = ready;
    to->tstate = running;
    ret = to->tid;
    thread_exit();
    return ret;
}*/

void
destroy_e_q (struct thread *from)
{
    if (from->e_q != exited && q_head != NULL)
    {
        while (q_head != NULL)
        {
            struct thread *current = q_head;
            struct thread *nxt = q_head->next;
            //printf ("%d \n", current->tid);
            free (current->dsp);
            free (current);
            q_head = nxt;
        }
    }
    /*else if (from->e_q == exited)
    {
        struct thread *current = q_head;
        while (current != from)
        {
            struct thread *nxt = q_head->next;
            free (current->dsp);
            free (current);
            q_head->next = nxt;
        }
    }*/
    //volatile int setcontext_called = 0;
    //int ret;
    /*to->tstate = running;
    ret = to->tid;*/
    /*err = getcontext(&from->mycontext);
    assert (!err);
    if (setcontext_called == 0)
    {
        setcontext_called = 1;
        err = setcontext(&to->mycontext);
        assert (!err);
    }*/
}


Tid
thread_yield(Tid want_tid)
{
    int enabled = interrupts_set(0);
    struct thread *current = t_head;
    struct thread *previous_t;
    // THREAD_ANY
    if (want_tid == THREAD_ANY) {
        previous_t = find_running();
        if (current->next == NULL && running_e == NULL) {
            destroy_e_q(previous_t);
            interrupts_set(enabled);
            return THREAD_NONE;
        }
        if (previous_t == NULL) {
            interrupts_set(enabled);
            return THREAD_NONE;
        }
        /*while (current->ran == 1 && current->next != NULL) {
            current = current->next;
        }*/
        if (previous_t->next == NULL)//&& current->ran == 1)
            current = t_head;
        else 
            current = previous_t->next;

        if (running_e != NULL) {
            int ret = yield(running_e, current);
            interrupts_set(enabled);
            return ret;
        } else if (current->tstate == ready && previous_t->dead != killed) {
            destroy_e_q(previous_t);
            int ret = yield(previous_t, current);
            interrupts_set(enabled);
            return ret;
        } else if (previous_t->dead == killed) {
            destroy_e_q(previous_t);
            thread_exit();
            //return ret;
        }
        destroy_e_q(previous_t);
        interrupts_set(enabled);
        return THREAD_NONE;
    }
    
    if (current->tid == want_tid || want_tid == THREAD_SELF)
    {
        interrupts_set(enabled);
        return current->tid;
    }
    if (want_tid > THREAD_MAX_THREADS - 1)
    {
        interrupts_set(enabled);
        return THREAD_INVALID;
    }
    previous_t = find_running();
    while (current->tid != want_tid)
    {
        if (current->next == NULL && running_e == NULL)
        {
            destroy_e_q (previous_t);
            interrupts_set(enabled);
            return THREAD_INVALID;
        }
        else
            current = current->next;
    }
    /*if (running_e != NULL) {
        int ret = destroy_e_q(running_e, current);
        return ret;
    }*/
    if (current->tstate == ready && previous_t->dead != killed)
    {
        destroy_e_q (previous_t);
        int ret  = yield (previous_t, current);
        interrupts_set(enabled);
        return ret;
    }
    else if (previous_t->dead == killed) {
        destroy_e_q (previous_t);
        thread_exit();
        interrupts_set(enabled);
        //return ret;
    }
    destroy_e_q (previous_t);
    interrupts_set(enabled);
    return THREAD_INVALID;
}


Tid
thread_exit()
{
    int enabled = interrupts_set(0);
    if (t_head->next == NULL)
    {
        return THREAD_NONE;
    }
    struct thread *current = find_running();
    
    //Find where current is in the ready queue then move to exit queue
    struct thread *trav = t_head;
    struct thread *prev;
    struct thread *temp1;
    if (current == t_head)
    {
        t_head = t_head->next;
        current->next = NULL;
    }
    else
    {
        while (trav != current) {
            prev = trav;
            trav = trav->next;
        }
        /*if (trav == current)
            printf("This is current\n");*/
        /*if (current->tstate != running)
        {
            current->e_q = exited;
            prev->next = current->next;
        }*/
        temp1 = trav->next;
        prev->next = temp1;
        trav->next = NULL;
    }
    current->e_q = exited;
    struct thread *temp = q_head;
    struct thread *prev2;
    if (q_head == NULL)
    {
        //printf ("qhead NULL \n");
        q_head = current;
        
    }
    else
    {
        while (temp != NULL) {
            prev2 = temp;
            temp = temp->next;
        }
        prev2->next = trav;
        trav->next = NULL;
    }
    
    avail_id = current->tid;
    running_e = current;
    interrupts_set(enabled);
    return thread_yield(THREAD_ANY);
}

Tid
thread_kill(Tid tid)
{
    struct thread *run = find_running();
    destroy_e_q (run);
    if (tid > THREAD_MAX_THREADS - 1 || run->tid == tid)
    {
        return THREAD_INVALID;
    }
    struct thread *current = t_head;
    while (current->tid != tid)
    {
        if (current->next == NULL)
            return THREAD_INVALID;
        current = current->next;
    }
    current->dead = killed;
    return current->tid;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

struct wait_queue {
    struct thread* thread_w_q[THREAD_MAX_THREADS];
};

struct wait_queue *
wait_queue_create() {
    struct wait_queue *wq;

    wq = malloc(sizeof (struct wait_queue));
    assert(wq);
    int i;
    for (i = 0; i < THREAD_MAX_THREADS; i++) {
        wq->thread_w_q[i] = NULL;
    }
    return wq;
}

void
wait_queue_destroy(struct wait_queue *wq) {
    free(wq);
}

Tid
thread_sleep(struct wait_queue *queue)
{
    struct thread* current;
    current = find_running(); 
    int enabled = interrupts_set(0);

    if (queue == NULL) {
        interrupts_set(enabled);
        return THREAD_INVALID;
    }
    if (t_head->next == NULL) {
        interrupts_set(enabled);
        return THREAD_NONE;
    }
    struct thread* temp;
    if (current->next != NULL)
        temp = current->next;
    else if (current == t_head)
        temp = t_head->next;
    else if (current != t_head)
        temp = t_head;

    struct thread *trav = t_head;
    struct thread *prev;
    struct thread *temp1;
    if (current == t_head) {
        t_head = t_head->next;
        current->next = NULL;
    } else {
        while (trav != current) {
            prev = trav;
            trav = trav->next;
        }
        temp1 = trav->next;
        prev->next = temp1;
        trav->next = NULL;
    }
    int j = 0;
    while (queue->thread_w_q[j] != NULL) {
        j++;
    }
    queue->thread_w_q[j] = current;
    int retId;
    retId = yield(current, temp);
    interrupts_set(enabled);
    return retId;
}



int
thread_wakeup(struct wait_queue *queue, int all)
{
    int enabled = interrupts_set(0);
    struct thread* current = t_head;
    struct thread* previous;
    if (queue == NULL){
        interrupts_set(enabled);
        return 0;
    }
    int w_count = 0;
    if (all == 1){
        while(current!=NULL){
            previous = current;
            current = current->next;
        }
        int i = 0;
        while(queue->thread_w_q[i]!=NULL){
            previous->next = queue->thread_w_q[i];
            queue->thread_w_q[i]->next = NULL;
            previous = previous->next;
            current = queue->thread_w_q[i]->next;
            i++;
            w_count++;   
        }
        for (i=0; i < THREAD_MAX_THREADS; i++){
            queue->thread_w_q[i] = NULL;
        }
        interrupts_set(enabled);
        return w_count;
    }
    else if (all == 0){
        int i = 0;
        while(current!=NULL){
            previous = current;
            current = current->next;
        }
        if (queue->thread_w_q[i] != NULL) {
            previous->next = queue->thread_w_q[i];
            queue->thread_w_q[i]->next = NULL;
            current = queue->thread_w_q[i]->next;
            while (queue->thread_w_q[i] != NULL) {
                queue->thread_w_q[i] = queue->thread_w_q[i + 1];
                i++;
            }
            interrupts_set(enabled);
            return 1;
        } 
        interrupts_set(enabled);
        return 0;
    }
    interrupts_set(enabled);
    return 0;
}

struct lock {
    int locked;
    struct wait_queue *wq;
};

struct lock *
lock_create()
{
    int enabled = interrupts_set(0);
    struct lock *lock;

    lock = malloc(sizeof(struct lock));

    lock->locked =0;
    lock->wq = wait_queue_create();
    
    interrupts_set(enabled);
    return lock;
}

void
lock_destroy(struct lock *lock) {
    int enabled = interrupts_set(0);
    assert(lock != NULL);

    if (!lock->locked)
    {    
        wait_queue_destroy(lock->wq);
        free(lock);
    }
    interrupts_set(enabled);
    return;
}

void
lock_acquire(struct lock *lock) {
    int enabled = interrupts_set(0);
    
    while (lock->locked)
    {
        thread_sleep (lock->wq);
    }
    lock->locked = 1;
    interrupts_set(enabled);
}
void
lock_release(struct lock *lock) {
    int enabled = interrupts_set(0);
    if (lock->locked)
    {
        lock->locked = 0;
        thread_wakeup(lock->wq,1);
    }
    interrupts_set(enabled);
}

struct cv {
    struct wait_queue *wq;
};

struct cv *
cv_create() {
    int enabled = interrupts_set(0);
    struct cv *cv;

    cv = malloc(sizeof (struct cv));
    assert(cv);

    cv->wq = wait_queue_create();
    interrupts_set(enabled);
    return cv;
}

void
cv_destroy(struct cv *cv) {
    if (cv!= NULL)
    {
        wait_queue_destroy(cv->wq);
        free(cv);
    }
}


void
cv_wait(struct cv *cv, struct lock *lock) {
    int enabled = interrupts_set(0);
    if (lock->locked)
    {
        lock_release(lock);
        thread_sleep(cv->wq);
    }
    lock_acquire(lock);
    interrupts_set(enabled);
}


void
cv_signal(struct cv *cv, struct lock *lock) {
    int enabled = interrupts_set(0);
    if (lock->locked)
        thread_wakeup(cv->wq, 0);
    interrupts_set(enabled);
}

void
cv_broadcast(struct cv *cv, struct lock *lock) {
    int enabled = interrupts_set(0);
    if (lock->locked)
        thread_wakeup(cv->wq, 1);
    interrupts_set(enabled);
    return;
}