#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define MAX_MSG_LEN 256

typedef struct msg_{
    char msg[MAX_MSG_LEN];
    /* TODO timestamp */
    /* TODO user */

    struct msg_ *next;
}Msg;

/* From socket to stdout (ncurses) */
extern Msg *read_head;
extern Msg *read_tail;
extern Msg *write_head;
extern Msg *write_tail;


void empty_list(Msg **head);
Msg *pop_msg_from_queue(Msg **head);
void add_message_to_queue(char *msg, Msg **head, Msg **tail);
void *init_message_handler(void *_);
void init_g(Msg **head, Msg **tail);

#endif