#include <inc/message.h>

#define MUTEX(line, mlock) pthread_mutex_lock(mlock); line pthread_mutex_unlock(mlock);

Msg *read_head, *read_tail;
Msg *write_head, *write_tail;

pthread_mutex_t r_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t w_lock = PTHREAD_MUTEX_INITIALIZER;

/* Not thread safe, should be only used after threads have exited */
void empty_list(Msg **head){
    Msg *ptr = *head;
    
    while(*head != NULL){
        ptr = (*head)->next;
        free(*head);
        *head = ptr;
    }

    return;
}

Msg *pop_msg_from_queue(Msg **head, pthread_mutex_t *lock){
    
    if(*head == NULL)
        return NULL;

    Msg *oldest = *head;

    /* Change head */
    MUTEX(*head = (*head)->next;, lock);

    /* Caller needs to free  - returns NULL if no head */
    return oldest;
}

void add_message_to_queue(
    char *msg, Msg **head, Msg **tail, pthread_mutex_t *lock){

    Msg *new;

    if((new = (Msg *)malloc(sizeof(Msg))) == NULL){
        perror("Couldn't allocate memory for a message");
        exit(EXIT_FAILURE);
    }

    strncpy(new->msg, msg, MAX_MSG_LEN);

    new->next = NULL;

    /* Linking the new node and updating tail and head */
    MUTEX(
        if(*head == NULL){
            *head = new;
        }else
            (*tail)->next = new;

        *tail = new;

    , lock);

    return;
}

/* Not thread safe, should be only used before staring the threads */
void init_list(Msg **head, Msg **tail){
    *head = NULL, *tail = NULL;

    return;
}
