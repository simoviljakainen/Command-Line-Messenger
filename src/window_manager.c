#include <inc/general.h>
#include <inc/setting.h>
#include <inc/message.h>
#include <inc/window_manager.h>

void refresh_windows(int count, ...);
WINDOW *create_window(int height, int width, int loc_x, int loc_y, int border);
void init_windows(WINDOW **main, WINDOW **in, WINDOW **border_main, WINDOW **border_in);
int handle_command(char *command);
void patch_msg_expressions(char *message);

int get_char_width(char *c, int size);
int get_char_from_string(char *string, char *c);
int parse_message_to_rows(Msg *message, char **rows, int row_count);

void insert_into_message_history(Msg *messages, int count, Msg msg);
void insert_into_row_history(char **rows, int count, char *row, int len);
void display_message_history(char **rows, char **ptr, int row_count, WINDOW *win);
int fix_row_lengths(Msg *messages, int m_count, char **rows);

int main_maxx, max_text_win; //max-window size and maximum lines shown


void *run_ncurses_window(void *_){
    WINDOW *main = NULL, *in, *border_in, *border_main;
    Msg messages[MAX_MESSAGE_LIST], *msg;
    char *row_history[MAX_MESSAGE_LIST], **row_ptr = row_history;

    setlocale(LC_ALL, ""); //for utf-8
    
    if(initscr() == NULL) {
        HANDLE_ERROR("Failed to initialize ncurses window.", 0);
    }

    /* curs_set(0) => hide cursor, returns ERR if request not supported */
    curs_set(0); 
    
    init_windows(&main, &in, &border_main, &border_in);
    refresh_windows(4, main, in, border_main, border_in);

    int msg_count = 0, c_byte, row_count = 0, rows_in_msg;
    char msg_buffer[MAX_MSG_LEN], *msg_ptr = msg_buffer;

    struct timeval start_time, end_time, test_time; 
    struct timespec sleep_time;
    long elapsed_nsecs;

    do{
        /* Start timer */
        gettimeofday(&start_time, NULL); //Only works on Unix

        /* Only if input is ready */
        if((c_byte = wgetch(in)) != ERR){
            switch(c_byte){
                case '\n': case '\r': case KEY_ENTER:

                    *msg_ptr = '\0';

                    if(*msg_buffer == '/'){
                        if(handle_command(&msg_buffer[1]))
                            goto close_UI;
                        
                    /* Put the message into the send queue */
                    }else{
                        patch_msg_expressions(msg_buffer);

                        pthread_mutex_lock(&w_lock);

                        add_message_to_queue(
                            compose_message(msg_buffer, NULL, user.username),
                            &write_head, &write_tail, NULL
                        );
                        pthread_cond_signal(&message_ready);

                        pthread_mutex_unlock(&w_lock);
                    }

                    msg_ptr = msg_buffer;

                    werase(in);
                    break;

                case KEY_RESIZE:
                    init_windows(&main, &in, &border_main, &border_in);
                    row_count = fix_row_lengths(messages, msg_count, row_history);
                    row_ptr = row_history;
                    break;

                case KEY_BACKSPACE:
                    wdelch(in);
                    if(msg_ptr > msg_buffer)
                        msg_ptr--;
                    break;

                case KEY_UP:
                    if(row_ptr > row_history)
                        row_ptr--;
                    break;

                case KEY_DOWN:
                    if(row_ptr < &row_history[row_count - max_text_win])
                        row_ptr++;
                    break;

                default:
                    if(c_byte & 0x80) //if not ascii
                        continue;

                    if(msg_ptr < &msg_buffer[MAX_MSG_LEN-1]){
                        *msg_ptr = c_byte;
                        msg_ptr++;
                    }
                    break;
            }
        }

        if((msg = pop_msg_from_queue(&read_head, &r_lock)) != NULL){

            insert_into_message_history(messages, msg_count, *msg);
            rows_in_msg = parse_message_to_rows(msg, row_history, row_count);
                    
            row_count += rows_in_msg;
            (row_count > MAX_MESSAGE_LIST) ? row_count = MAX_MESSAGE_LIST : 0;
            
            (msg_count < MAX_MESSAGE_LIST) ? msg_count++ : 0;

            /* Move the row window only if the window is full of text */
            if(row_count > max_text_win && row_ptr < &row_history[row_count - max_text_win])
                row_ptr += rows_in_msg;

            free(msg);
        }

        /* Calculate sleep time = 1/fps_target - time_taken */
        gettimeofday(&end_time, NULL);

        sleep_time = remainder_timespec(
            nanosec_to_timespec((1.0 / connection.fps) * NANOSECS_IN_SEC),
            get_time_interval(start_time, end_time)
        );

        nanosleep(&sleep_time, NULL); //cannot use std=c99

        /* Calculate the FPS */
        gettimeofday(&test_time, NULL);
        elapsed_nsecs = timespec_to_nanosec(
            get_time_interval(start_time, test_time)
        );
        
        mvwprintw(
            border_main, 0, 0, "FPS: %.0lf",
            round((double)NANOSECS_IN_SEC / elapsed_nsecs)
        );
        
        /* Refresh */
        display_message_history(row_history, row_ptr, row_count, main);
        refresh_windows(4, main, in, border_main, border_in);
        
    }while(true);

    close_UI:
        /* Clean row history */
        for(int i = 0; i < row_count; i++)
            free(row_history[i]);
        
        delwin(main);
        delwin(border_main);
        delwin(in);
        delwin(border_in);

        endwin();

    return NULL;
}

int parse_message_to_rows(Msg *message, char **rows, int row_count){
    int row_idx = row_count, max_boi = main_maxx;
    int char_size = 0, char_bytes = 0, cur_row_len = 0;

    int max_msg_size = MAX_USERNAME_LEN + ID_SIZE + MAX_MSG_LEN + 1;

    char row_buff[max_boi * MAX_BYTES_IN_CHAR], raw_message[max_msg_size];
    char *char_ptr, wide_char[MAX_BYTES_IN_CHAR], *sub_row_ptr;

    /* Concat the user details and message */
    snprintf(
        raw_message, max_msg_size,
        "%s(%s): %s",
        message->username, message->id, message->msg
    );

    char_ptr = raw_message;
    sub_row_ptr = row_buff;

    while(true){
        char_size = get_char_from_string(char_ptr, wide_char);
        
        if(*wide_char == '\0') // if the message ends, end the loop
            break;

        memcpy(sub_row_ptr, wide_char, char_size);

        char_bytes += char_size; // length in bytes
        sub_row_ptr += char_size;
        cur_row_len += get_char_width(wide_char, char_size); // length in characters

        if(cur_row_len == max_boi){ //row is full
            insert_into_row_history(rows, row_idx++, row_buff, char_bytes);

            sub_row_ptr = row_buff;
            char_bytes = 0;
            cur_row_len = 0;
        }        
        char_ptr += char_size;
    }

    if(char_bytes > 0){ //save last row    
        insert_into_row_history(rows, row_idx++, row_buff, char_bytes);
    }

    return row_idx - row_count;            
}

int fix_row_lengths(Msg *messages, int msg_count, char **rows){
    int row_count = 0;

    for(int i = 0; i < msg_count; i++){
        row_count += parse_message_to_rows(&messages[i], rows, row_count);
    }

    return (row_count > MAX_MESSAGE_LIST) ? MAX_MESSAGE_LIST : row_count;
}

WINDOW *create_window(int height, int width, int loc_x, int loc_y, int border){
    WINDOW *win;
    
    if((win = newwin(height, width, loc_x, loc_y)) == NULL){
        HANDLE_ERROR("Failed to create a new window", 0);
    }

    if(border){
        wborder(win, SB, SB, TB, BB, CB, CB, CB, CB);
    }

    return win;
}

void patch_msg_expressions(char *message){
    int new_len, exp_len, new_msg_len;
    char *substr, rest[MAX_MSG_LEN];

    for(int i = 0; i < expression_count; i++){
        substr = message;
        while((substr = strstr(substr, expressions[i].exp)) != NULL){
            
            new_len = strlen(expressions[i].new);
            exp_len = strlen(expressions[i].exp);
            new_msg_len = strlen(message) + new_len - exp_len;

            if(new_msg_len > MAX_MSG_LEN - 1)
                break;

            strcpy(rest, substr + exp_len);

            /* Copy the expression into the original string */
            memcpy(substr, expressions[i].new, new_len);

            /* Copy rest of the string */
            memcpy(substr + new_len, rest, strlen(rest) + 1);
        }
    }

    return;
}

void refresh_windows(int count, ...){
    va_list windows;

    va_start(windows, count);

    for(int i = 0; i < count; i++){
        if(wnoutrefresh(va_arg(windows, WINDOW *)) == ERR){
            HANDLE_ERROR("Failed to refresh the window.", 0);
        }
    }

    doupdate();

    va_end(windows);

    return;
}

void init_windows(
    WINDOW **main, WINDOW **in, WINDOW **border_main, WINDOW **border_in){
    
    int max_y, max_x;
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
    getmaxyx(*in , inmy, inmx); //struct's _maxx etc should not be used

    *border_in = create_window(
        inmy + WIN_BORDER_SIZE_Y * 2,
        inmx + WIN_BORDER_SIZE_X * 2,
        (*in)->_begy - WIN_BORDER_SIZE_Y,
        (*in)->_begx - WIN_BORDER_SIZE_X,
        1
    );

    max_text_win = ((*border_in)->_begy) - (*main)->_begy;
    main_maxx = getmaxx(*main);

    nodelay(*in, TRUE); //input does not block output
    keypad(*in, TRUE); //ncurses interpret keys

    return;
}

void insert_into_row_history(char **rows, int count, char *row, int len){

    (count > MAX_MESSAGE_LIST) ? count = MAX_MESSAGE_LIST : 0;

    /* Rows are shifted - oldest row is discarded */
    if(count == MAX_MESSAGE_LIST){
        int i;
        free(rows[0]);

        for(i = 0; i < count-1; i++){
            rows[i] = rows[i+1];
        }

        rows[i] = strndup(row, len);

        return;
    }

    rows[count] = strndup(row, len);

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

/* Print the character into a window to determine its width */
int get_char_width(char *wide_char, int size){
    static WINDOW *test_win = NULL;

    char *char_ptr = strndup(wide_char, size);

    if(test_win == NULL)
        test_win = newwin(1, main_maxx, 0, 0);
    else
        wmove(test_win, 0, 0);

    wprintw(test_win, "%s", char_ptr);
    free(char_ptr);
    
    return getcurx(test_win);
}

int get_char_from_string(char *string, char *c){
    char ch = *string;
    int size = 1;

    /* Check for char's leading byte */
    if((ch & 0xE0) == 0xC0)
        size = 2; //2 byte - utf-8
    else if((ch & 0xF0) == 0xE0)
        size = 3; //3 byte - utf-8
    else if((ch & 0xF8) == 0xF0)
        size = 4; //4 byte - utf-8
    
    memcpy(c, string, size);
    
    return size;
}

void display_message_history(char **rows, char **ptr, int row_count, WINDOW *win){

    for(int i = 0; i < row_count && i < max_text_win; i++){
        if(&ptr[i] > &rows[row_count-1])
            break;

        wmove(win, i, 0); //Moving the cursor for the clear
        wclrtoeol(win);
        mvwprintw(win, i, 0, "%s", ptr[i]);
    }

    return;
}

int handle_command(char *raw_command){
    char command[MAX_MSG_LEN], args[MAX_MSG_LEN], *response;

    sscanf(raw_command, "%s %s", command, args);

    if(!strcmp(command, C_CHANGE_USERNAME)){
        snprintf(
			user.username, MAX_USERNAME_LEN,
			"%s", args
		);
        response = "Username changed.";

    }else if(!strcmp(command, C_CHANGE_FPS)){
        connection.fps = (atoi(args)) ? str_to_uint16_t(args) : 60;
        response = "Target FPS changed.";

    }else if(!strcmp(command, C_QUIT)){
        return 1;

    }else{
        response = "Invalid command.";
    }
    
    add_message_to_queue(
        compose_message(response, "0", "System"),
        &read_head, &read_tail, &r_lock
    );

    return 0;
}
