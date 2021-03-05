#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include <ncurses.h>

#include <locale.h>
#include <stdarg.h>

#define MAX_MESSAGE_LIST 100
#define MSGBOX_LINES 1
#define MSGBOX_PAD_Y 1
#define MSGBOX_PAD_X 5
#define WIN_BORDER_SIZE_Y 1
#define WIN_BORDER_SIZE_X 1

void *run_ncurses_window(void *_);

extern char username[MAX_USERNAME_LEN];

#endif