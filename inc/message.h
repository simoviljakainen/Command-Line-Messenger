#ifndef MESSAGE_H
    #define MESSAGE_H

    #include <inc/setting.h>
    #include <inc/general.h>

    typedef struct msg_{
        char msg[MAX_MSG_LEN];
        char username[MAX_USERNAME_LEN];
        char id[ID_SIZE];

        struct msg_ *next;
    }Msg;

    extern Msg *read_head;
    extern Msg *read_tail;
    extern Msg *write_head;
    extern Msg *write_tail;

    extern pthread_mutex_t r_lock;
    extern pthread_mutex_t w_lock;

    void empty_list(Msg **head);
    Msg *pop_msg_from_queue(Msg **head, pthread_mutex_t *);
    void add_message_to_queue(Msg new, Msg **head, Msg **tail, pthread_mutex_t *);
    void init_list(Msg **head, Msg **tail);

#endif