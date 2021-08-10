#include <inc/general.h>
#include <inc/message.h>
#include <inc/setting.h>
#include <inc/window_manager.h>

void refresh_windows(int count, ...);
WINDOW *create_window(int height, int width, int loc_x, int loc_y, int border);
void init_windows(WINDOW **main, WINDOW **in, WINDOW **border_main, WINDOW **border_in);
int init_colors(void);
int handle_command(char *command);

int get_char_size(char lead_byte);
int get_char_width(char *c, int size);
int get_char_from_string(char *string, char *c);
void fix_multibyte_chars(char *start, char *end);
int parse_message_to_rows(Msg *message);
void patch_msg_expressions(char *message);

int insert_into_message_history(Msg *messages, int *count, Msg msg);
void insert_into_msg_rows(char ***rows, int count, char *row, int len);
void display_message_history(Msg *messages, int msg_count, WINDOW *win, int offset);
int fix_row_lengths(Msg *messages, int msg_count);
int handle_offset(int old_offset, int increment, int times, int row_count);

void free_msg_rows(Msg *msg);
void free_msg_username_colors(Msg *msg);

int main_maxx, max_text_win, colors_supported;  //max-window size and maximum lines shown

void *run_ncurses_window(void *_) {
  WINDOW *main = NULL, *in, *border_in, *border_main;

  setlocale(LC_ALL, "");  //for utf-8

  if (initscr() == NULL) {
    HANDLE_ERROR("Failed to initialize ncurses window.", 0);
  }

  colors_supported = init_colors();

  /* curs_set(0) => hide cursor, returns ERR if request not supported */
  curs_set(0);

  init_windows(&main, &in, &border_main, &border_in);
  refresh_windows(4, main, in, border_main, border_in);

  Msg messages[MAX_MESSAGE_LIST], *msg;

  int msg_count = 0, c_byte, row_count = 0, rows_in_msg, offset = 0;
  char msg_buffer[MAX_MSG_LEN], *msg_ptr = msg_buffer;

  struct timeval start_time, end_time, test_time;
  struct timespec sleep_time;
  long elapsed_nsecs;

  do {
    /* Start timer */
    gettimeofday(&start_time, NULL);  //Only works on Unix

    /* Only if input is ready */
    if ((c_byte = wgetch(in)) != ERR) {
      switch (c_byte) {
        case '\n':
        case '\r':
        case KEY_ENTER:
          if (msg_ptr == msg_buffer)
            continue;

          fix_multibyte_chars(msg_buffer, msg_ptr - 1);

          *msg_ptr = '\0';

          if (*msg_buffer == '/') {
            if (handle_command(&msg_buffer[1]))
              goto close_UI;

            /* Put the message into the send queue */
          } else {
            patch_msg_expressions(msg_buffer);

            pthread_mutex_lock(&w_lock);

            add_message_to_queue(
                compose_message(msg_buffer, NULL, user.username),
                &write_head, &write_tail, NULL);
            pthread_cond_signal(&message_ready);

            pthread_mutex_unlock(&w_lock);
          }

          msg_ptr = msg_buffer;

          werase(in);
          break;

        case KEY_RESIZE:
          init_windows(&main, &in, &border_main, &border_in);
          row_count = fix_row_lengths(messages, msg_count);
          offset = 0;
          break;

        case KEY_BACKSPACE:
          wdelch(in);
          if (msg_ptr > msg_buffer)
            msg_ptr--;
          break;

        case KEY_UP:
          offset = handle_offset(offset, -1, 1, row_count);
          break;

        case KEY_DOWN:
          offset = handle_offset(offset, 1, 1, row_count);
          break;

        default:
          if (msg_ptr < &msg_buffer[MAX_MSG_LEN - 1]) {
            *msg_ptr = c_byte;
            msg_ptr++;
          }
          break;
      }
    }

    if ((msg = pop_msg_from_queue(&read_head, &r_lock)) != NULL) {
      rows_in_msg = insert_into_message_history(messages, &msg_count, *msg);
      row_count += rows_in_msg;

      /* Move the row window only if the window is full of text */
      if (row_count > max_text_win - 1)
        offset = handle_offset(offset, 1, rows_in_msg, row_count);

      free(msg);
    }

    /* Calculate sleep time = 1/fps_target - time_taken */
    gettimeofday(&end_time, NULL);

    sleep_time = remainder_timespec(
        nanosec_to_timespec((1.0 / connection.fps) * NANOSECS_IN_SEC),
        get_time_interval(start_time, end_time));

    nanosleep(&sleep_time, NULL);  //cannot use std=c99

    /* Calculate the FPS */
    gettimeofday(&test_time, NULL);
    elapsed_nsecs = timespec_to_nanosec(
        get_time_interval(start_time, test_time));

    COLOR(
        mvwprintw(
            border_main, 0, 0, "FPS: %.0lf",
            round((double)NANOSECS_IN_SEC / elapsed_nsecs));
        , border_main, 2)

    /* Refresh */
    display_message_history(messages, msg_count, main, offset);
    refresh_windows(4, main, in, border_main, border_in);

  } while (true);

close_UI:
  /* Clean row history */
  for (int i = 0; i < msg_count; i++) {
    free_msg_rows(&messages[i]);
    free_msg_username_colors(&messages[i]);
  }

  endwin();

  delwin(main);
  delwin(border_main);
  delwin(in);
  delwin(border_in);

  return NULL;
}

/* Offset == what row (1->row_count) is the first one to be displayed */
int handle_offset(int old_offset, int increment, int times, int row_count) {
  int new_offset = old_offset;

  while (times > 0) {
    new_offset += increment;
    times--;

    if (new_offset > row_count - max_text_win + 1)
      return new_offset - increment;

    if (new_offset < 0)
      return new_offset - increment;
  }

  return new_offset;
}

int parse_message_to_rows(Msg *message) {
  int row_idx = 0;
  int char_size = 0, char_bytes = 0, cur_row_len = 0;

  char row_buff[MAX_ROW_SIZE + 1];
  char *char_ptr, wide_char[MAX_BYTES_IN_CHAR], *sub_row_ptr;

  cur_row_len = ROW_FORMAT_LEN + strlen(message->username) + strlen(message->id);

  char_ptr = message->msg;
  sub_row_ptr = row_buff;

  while (true) {
    char_size = get_char_from_string(char_ptr, wide_char);

    if (*wide_char == '\0')  // if the message ends, end the loop
      break;

    memcpy(sub_row_ptr, wide_char, char_size);

    char_bytes += char_size;  // length in bytes
    sub_row_ptr += char_size;
    cur_row_len += get_char_width(wide_char, char_size);  // length in characters

    if (cur_row_len >= main_maxx) {  //row is full
      (cur_row_len > main_maxx) ? char_bytes -= char_size : 0;

      insert_into_msg_rows(&message->rows, row_idx++, row_buff, char_bytes);

      sub_row_ptr = row_buff;
      char_bytes = 0;
      cur_row_len = 0;
    }
    char_ptr += char_size;
  }

  if (char_bytes > 0) {  //save last row
    insert_into_msg_rows(&message->rows, row_idx++, row_buff, char_bytes);
  }

  message->row_count = row_idx;

  return row_idx;
}

void free_msg_rows(Msg *msg) {
  for (int row_idx = 0; row_idx < msg->row_count; row_idx++)
    free(msg->rows[row_idx]);
  free(msg->rows);

  msg->rows = NULL;

  return;
}

void free_msg_username_colors(Msg *msg) {
  free(msg->username_colors);
  msg->username_colors = NULL;

  return;
}

int fix_row_lengths(Msg *messages, int msg_count) {
  int row_count = 0;

  for (int msg_idx = 0; msg_idx < msg_count; msg_idx++) {
    free_msg_rows(&messages[msg_idx]);
    row_count += parse_message_to_rows(&messages[msg_idx]);
  }

  return row_count;
}

WINDOW *create_window(int height, int width, int loc_x, int loc_y, int border) {
  WINDOW *win;

  if ((win = newwin(height, width, loc_x, loc_y)) == NULL) {
    HANDLE_ERROR("Failed to create a new window", 0);
  }

  if (border) {
    wborder(win, SB, SB, TB, BB, CB, CB, CB, CB);
  }

  return win;
}

void patch_msg_expressions(char *message) {
  int new_len, exp_len, new_msg_len;
  char *substr, rest[MAX_MSG_LEN];

  for (int i = 0; i < expression_count; i++) {
    substr = message;
    while ((substr = strstr(substr, expressions[i].exp)) != NULL) {
      new_len = strlen(expressions[i].new);
      exp_len = strlen(expressions[i].exp);
      new_msg_len = strlen(message) + new_len - exp_len;

      if (new_msg_len > MAX_MSG_LEN - 1)
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

void fix_multibyte_chars(char *start, char *end) {
  int size;

  for (int i = 1; i <= MAX_BYTES_IN_CHAR && end >= start; i++, end--) {
    size = get_char_size(*end);

    if (i < size) {
      *end = '\0';
      break;
    }
  }

  return;
}

void refresh_windows(int count, ...) {
  va_list windows;

  va_start(windows, count);

  for (int i = 0; i < count; i++) {
    if (wnoutrefresh(va_arg(windows, WINDOW *)) == ERR) {
      HANDLE_ERROR("Failed to refresh the window.", 0);
    }
  }

  doupdate();

  va_end(windows);

  return;
}

int init_colors(void) {
  if (has_colors() && can_change_color()) {
    start_color();

    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(4, COLOR_GREEN, COLOR_BLACK);
    init_pair(5, COLOR_BLUE, COLOR_BLACK);
    init_pair(6, COLOR_CYAN, COLOR_BLACK);
    init_pair(7, COLOR_RED, COLOR_BLACK);

    return 0;
  }

  return 1;
}

void init_windows(
    WINDOW **main, WINDOW **in, WINDOW **border_main, WINDOW **border_in) {
  int max_y, max_x;
  getmaxyx(stdscr, max_y, max_x);

  if (max_y < WIN_BORDER_SIZE_Y * 4 + MSGBOX_LINES + MSGBOX_PAD_Y)
    return;

  if (max_x < WIN_BORDER_SIZE_X * 4 + MSGBOX_PAD_X * 2)
    return;

  if (*main != NULL) {
    delwin(*main);
    delwin(*border_main);
    delwin(*in);
    delwin(*border_in);
  }

  *border_main = create_window(
      max_y, max_x, 0, 0, 1);

  *main = create_window(
      max_y - WIN_BORDER_SIZE_Y * 2,
      max_x - WIN_BORDER_SIZE_X * 2,
      WIN_BORDER_SIZE_Y,
      WIN_BORDER_SIZE_X,
      0);

  *in = create_window(
      MSGBOX_LINES,
      max_x - MSGBOX_PAD_X * 2 - WIN_BORDER_SIZE_X * 4,
      max_y - MSGBOX_PAD_Y - WIN_BORDER_SIZE_Y * 2 - MSGBOX_LINES,
      (*main)->_begx + MSGBOX_PAD_X + WIN_BORDER_SIZE_X,
      0);

  int inmx, inmy;
  getmaxyx(*in, inmy, inmx);  //struct's _maxx etc should not be used

  *border_in = create_window(
      inmy + WIN_BORDER_SIZE_Y * 2,
      inmx + WIN_BORDER_SIZE_X * 2,
      (*in)->_begy - WIN_BORDER_SIZE_Y,
      (*in)->_begx - WIN_BORDER_SIZE_X,
      1);

  max_text_win = ((*border_in)->_begy) - (*main)->_begy;
  main_maxx = getmaxx(*main);

  nodelay(*in, TRUE);  //input does not block output
  keypad(*in, TRUE);   //ncurses interpret keys

  return;
}

void insert_into_msg_rows(char ***rows, int count, char *row, int len) {
  if ((*rows = (char **)realloc(*rows, (count + 1) * sizeof(char *))) == NULL) {
    HANDLE_ERROR("Couldn't allocate memory for msg rows", 1);
  }
  (*rows)[count] = strndup(row, len);

  return;
}

/* Copy the message into the point x*/
int insert_into_message_history(Msg *messages, int *count, Msg msg) {
  int rows_in_msg;

  /* Messages are shifted - oldest message is discarded */
  if (*count == MAX_MESSAGE_LIST) {
    int msg_idx, old_row_count;

    /* free oldest message */
    old_row_count = messages[0].row_count;
    free_msg_rows(&messages[0]);
    free_msg_username_colors(&messages[0]);

    for (msg_idx = 0; msg_idx < (*count) - 1; msg_idx++) {
      messages[msg_idx] = messages[msg_idx + 1];
    }

    messages[msg_idx] = msg;
    rows_in_msg = parse_message_to_rows(&messages[msg_idx]);
    parse_username_for_msg(&messages[msg_idx], msg.username);

    return rows_in_msg - old_row_count;
  }

  messages[*count] = msg;
  rows_in_msg = parse_message_to_rows(&messages[*count]);
  parse_username_for_msg(&messages[*count], msg.username);
  (*count)++;

  return rows_in_msg;
}

/* Print the character into a window to determine its width */
int get_char_width(char *wide_char, int size) {
  static WINDOW *test_win = NULL;

  char *char_ptr = strndup(wide_char, size);

  if (test_win == NULL)
    test_win = newwin(1, main_maxx, 0, 0);
  else
    wmove(test_win, 0, 0);

  wprintw(test_win, "%s", char_ptr);
  free(char_ptr);

  return getcurx(test_win);
}

int get_char_size(char lead_byte) {
  int size = 1;

  /* Check for char's leading byte */
  if ((lead_byte & 0xE0) == 0xC0)
    size = 2;  //2 byte - utf-8
  else if ((lead_byte & 0xF0) == 0xE0)
    size = 3;  //3 byte - utf-8
  else if ((lead_byte & 0xF8) == 0xF0)
    size = 4;  //4 byte - utf-8

  return size;
}

int get_char_from_string(char *string, char *c) {
  char ch = *string;

  int size = get_char_size(ch);
  memcpy(c, string, size);

  return size;
}

void print_colored_str_to_window(WINDOW *win, CChar *char_colors, char *str) {
  int i = 0;
  char *substr;

  if (!colors_supported) {
    while (*str != '\0') {
      substr = strndup(str, char_colors[i].bytes);

      COLOR(
          wprintw(win, "%s", substr);
          , win, char_colors[i].color)

      str += char_colors[i].bytes;
      i++;
      free(substr);
    }
  } else {
    wprintw(win, "%s", str);
  }

  return;
}

void display_message_history(Msg *messages, int msg_count, WINDOW *win, int offset) {
  int cur_idx = 0;

  for (int msg_idx = 0; msg_idx < msg_count; msg_idx++) {
    for (int row_idx = 0; row_idx < messages[msg_idx].row_count; row_idx++) {
      if (offset > 0) {
        offset--;
        continue;
      }

      wmove(win, cur_idx, 0);
      wclrtoeol(win);

      if (!row_idx) {  // first row, print meta
        print_colored_str_to_window(
            win, messages[msg_idx].username_colors,
            messages[msg_idx].username);
        wprintw(
            win, ROW_FORMAT,
            messages[msg_idx].id, messages[msg_idx].rows[row_idx]);

      } else {
        wprintw(win, "%s", messages[msg_idx].rows[row_idx]);
      }

      cur_idx++;

      if (cur_idx == max_text_win - 1)
        return;
    }
  }

  return;
}

int handle_command(char *raw_command) {
  char command[MAX_MSG_LEN], args[MAX_MSG_LEN], *response;

  sscanf(raw_command, "%s %s", command, args);

  if (!strcmp(command, C_CHANGE_USERNAME)) {
    snprintf(
        user.username, MAX_USERNAME_LEN,
        "%s", args);
    response = "Username changed.";

  } else if (!strcmp(command, C_CHANGE_FPS)) {
    connection.fps = (atoi(args)) ? str_to_uint16_t(args) : 60;
    response = "Target FPS changed.";

  } else if (!strcmp(command, C_QUIT)) {
    return 1;

  } else {
    response = "Invalid command.";
  }

  add_message_to_queue(
      compose_message(response, "0", "/7:System"),
      &read_head, &read_tail, &r_lock);

  return 0;
}
