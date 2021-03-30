#include <inc/general.h>
#include <inc/message.h>

Msg *read_head, *read_tail;
Msg *write_head, *write_tail;

pthread_mutex_t r_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t w_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t message_ready = PTHREAD_COND_INITIALIZER;

Msg compose_message(char *msg, char *id, char *username){
    Msg new_message = {.id="00", .next = NULL};

    if(msg != NULL)
        snprintf(new_message.msg, MAX_MSG_LEN, "%s", msg);
    if(username != NULL)
        snprintf(new_message.username, MAX_USERNAME_LEN, "%s", username);
    if(id != NULL)
        snprintf(new_message.id, ID_SIZE, "%s", id);

    return new_message;
}

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
    Msg msg, Msg **head, Msg **tail, pthread_mutex_t *lock){

    Msg *new;

    if((new = (Msg *)calloc(1, sizeof(Msg))) == NULL){
        HANDLE_ERROR("Couldn't allocate memory for a message", 1);
    }

    snprintf(new->msg, MAX_MSG_LEN, "%s", msg.msg);
    snprintf(new->username, MAX_USERNAME_LEN, "%s", msg.username);
    snprintf(new->id, ID_SIZE, "%s", msg.id);

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
