typedef struct msg_{
    char msg[256];
    /* TODO timestamp */
}Msg;

Msg message_queue[100];

void add_message_to_queue(Msg msg){
    static int id = 0;
    message_queue[id++] = msg;
}