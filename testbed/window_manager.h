#include <ncurses.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <locale.h>
#include <stdarg.h>
#include <time.h>

#include "message.h"

#define MAX_MESSAGE_LIST 100
#define MAX_MESSAGE_LEN 256
#define MSGBOX_LINES 1
#define MSGBOX_PAD_Y 1
#define MSGBOX_PAD_X 5
#define WIN_BORDER_SIZE_Y 1
#define WIN_BORDER_SIZE_X 1

void *run_ncurses_window(void *init);