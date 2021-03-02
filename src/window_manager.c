#include <inc/window_manager.h>

int max_y, max_x, max_text_win; //max-window size and maximum lines shown

typedef struct timespec Tmsc; 

WINDOW *create_window(int height, int width, int loc_x, int loc_y, int border){
    WINDOW *win = newwin(height, width, loc_x, loc_y);

    if(border){
        wborder(win, '|', '|', '-', '-', '+', '+', '+', '+');
    }

    return win;
}

void refresh_windows(int count, ...){
    va_list windows;

    va_start(windows, count);

    for(int i = 0; i < count; i++){
        wnoutrefresh(va_arg(windows, WINDOW *));
    }

    doupdate();

    va_end(windows);

    return;
}

void init_windows(
    WINDOW **main, WINDOW **in, WINDOW **border_main, WINDOW **border_in){

    getmaxyx(stdscr, max_y, max_x);

    if(max_y < WIN_BORDER_SIZE_Y * 4 + MSGBOX_LINES + MSGBOX_PAD_Y)
        return;
    
    if(max_x < WIN_BORDER_SIZE_X * 4 + MSGBOX_PAD_X * 2)
        return;

    if(*main != NULL){
        delwin(*main);
        delwin(*border_main);
        delwin(*in);
        delwin(*border_in);
    }

    *border_main = create_window(
        max_y, max_x, 0, 0, 1
    );

    *main = create_window(
        max_y - WIN_BORDER_SIZE_Y * 2,
        max_x - WIN_BORDER_SIZE_X * 2,
        WIN_BORDER_SIZE_Y,
        WIN_BORDER_SIZE_X,
        0
    );

    *in = create_window(
        MSGBOX_LINES,
        max_x - MSGBOX_PAD_X * 2 - WIN_BORDER_SIZE_X * 4,
        max_y - MSGBOX_PAD_Y - WIN_BORDER_SIZE_Y * 2 - MSGBOX_LINES,
        (*main)->_begx + MSGBOX_PAD_X + WIN_BORDER_SIZE_X,
        0
    );

    int inmx, inmy;
    getmaxyx(*in , inmy, inmx); // struct's _maxx etc should not be used

    *border_in = create_window(
        inmy + WIN_BORDER_SIZE_Y * 2,
        inmx + WIN_BORDER_SIZE_X * 2,
        (*in)->_begy - WIN_BORDER_SIZE_Y,
        (*in)->_begx - WIN_BORDER_SIZE_X,
        1
    );

    max_text_win = ((*border_in)->_begy) - (*main)->_begy;

    nodelay(*in, TRUE); // so that the input does not block output
    keypad(*in, TRUE);

    return;
}

void insert_into_message_history(Msg *messages, int count, Msg msg){

    /* Messages are shifted - oldest message is discarded */
    if(count == MAX_MESSAGE_LIST){
        int i;

        for(i = 0; i < count-1; i++){
            messages[i] = messages[i+1];
        }

        messages[i] = msg;

        return;
    }

    messages[count] = msg;

    return;
}

void display_message_history(Msg *messages, Msg *ptr, int count, WINDOW *win){

    for(int i = 0; i < count && i < max_text_win; i++){
        if(&ptr[i] > &messages[count-1])
            continue;

        wclrtoeol(win);
        mvwprintw(win, i, 1, "%s", ptr[i].msg);
    }

    return;
}

void *run_ncurses_window(void *init){
    WINDOW *main = NULL, *in, *border_in, *border_main;
    Msg messages[MAX_MESSAGE_LIST], *msg_list_ptr = messages, *msg;

    setlocale(LC_ALL, ""); // for utf-8
    initscr(); curs_set(0); // curs_set => hide cursor
    
    init_windows(&main, &in, &border_in, &border_main);
    refresh_windows(4, main, in, border_in, border_main);

    int count = 0, c, msec = 1;
    char msg_buffer[MAX_MSG_LEN], *msg_ptr = msg_buffer;

    /* Limit fps */
    Tmsc sleep_time = {.tv_nsec = msec * 1000000};

    do{
        /* Only if input is ready */
        if((c = wgetch(in)) != ERR){
            switch(c){
                case '\n': case KEY_ENTER:
                    /* Put the message into the send queue */
                    *msg_ptr = '\0';

                    add_message_to_queue(
                        msg_buffer, &write_head, &write_tail, &w_lock
                    );

                    msg_ptr = msg_buffer;
                    werase(in);
                    break;

                case KEY_RESIZE:
                    init_windows(&main, &in, &border_in, &border_main);
                    break;

                case KEY_BACKSPACE:
                    wdelch(in);
                    msg_ptr--;
                    break;

                case KEY_UP:
                    if(msg_list_ptr > messages)
                        msg_list_ptr--;
                    break;

                case KEY_DOWN:
                    if(msg_list_ptr < &messages[count - max_text_win])
                        msg_list_ptr++;
                    break;

                default:
                    if(msg_ptr < &msg_buffer[MAX_MSG_LEN-2]){
                        *msg_ptr = c;
                        msg_ptr++;
                    }
                    break;
            }
        }

        if((msg = pop_msg_from_queue(&read_head, &r_lock)) != NULL){

            insert_into_message_history(messages, count, *msg);
                    
            (count < MAX_MESSAGE_LIST) ? count++ : 0;

            if(count > max_text_win && msg_list_ptr < &messages[count - max_text_win])
                msg_list_ptr++;

            free(msg);
        }

        display_message_history(messages, msg_list_ptr, count, main);
        refresh_windows(4, main, in, border_in, border_main);
        nanosleep(&sleep_time, NULL); // cannot use std=c99

    }while(c != 'q');

    endwin();

    return NULL;
}