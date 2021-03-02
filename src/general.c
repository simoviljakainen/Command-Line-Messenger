#include <inc/general.h>

void handle_error(char *msg, int show_err, char *file, int line){
	fprintf(
		stderr,
		"ERROR(%d) %s:%d -- %s:%s\n",
		errno,
		file, line,
        (msg != NULL) ? msg : "-",
		(show_err) ? strerror(errno): "-"
	);
	exit(EXIT_FAILURE);
}
