#ifndef MESSAGE_H
    #define MESSAGE_H

    #include <inc/setting.h>
    #include <inc/general.h>

    typedef struct _cchar{
        int color;
        int bytes;
    }CChar;

    typedef struct msg_{
        char msg[MAX_MSG_LEN];
        char **rows;
        int row_count;
        char username[MAX_USERNAME_LEN];
        CChar *username_colors;
        int color_count;
        char id[ID_SIZE];

        struct msg_ *next;
    }Msg;

    extern Msg *read_head;
    extern Msg *read_tail;
    extern Msg *write_head;
    extern Msg *write_tail;

    extern pthread_mutex_t r_lock;
    extern pthread_mutex_t w_lock;

    extern pthread_cond_t message_ready;

    void empty_list(Msg **head);
    Msg *pop_msg_from_queue(Msg **head, pthread_mutex_t *);
    void add_message_to_queue(Msg new, Msg **head, Msg **tail, pthread_mutex_t *);
    void init_list(Msg **head, Msg **tail);
    Msg compose_message(char *msg, char *id, char *username);
    void parse_username_for_msg(Msg *dest, char *src);

#endif