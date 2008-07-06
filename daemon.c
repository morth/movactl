
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/signal.h>
#include <unistd.h>
#include <string.h>
#include <event.h>
#include <stdlib.h>

#include "line.h"
#include "status.h"
#include "backend.h"
#include "launchd.h"

const char *line = "/dev/tty.usbserial";
const char *sock = "/tmp/marantz.sock";

struct ma_status status;
int line_fd;

int running;

void
quit_event (int fd, short what, void *cbarg) {
	running = 0;
	event_loopexit (NULL);
}

void
line_reader (int fd, short what, void *cbarg) {
	char buf[256];
	int res;

	if ((res = read_line (fd, buf, sizeof (buf))) > 0) {
		fprintf (stderr, "Read line %s\n", buf);
		update_status (fd, &status, buf);
	} else {
		if (res)
			err (1, "read_line");
		event_loopexit (NULL);
	}
}

int
main (int argc, char *argv[]) {
	struct event line_ev;
	struct event *backend_event;
	struct event term_ev;

	/* Believe it or not, but it seems both kqueue and poll engines are broken on OS X right now. */
	setenv ("EVENT_NOKQUEUE", "1", 0);
	setenv ("EVENT_NOPOLL", "1", 0);
	event_init();

	signal_set (&term_ev, -1, quit_event, NULL);
	signal_add (&term_ev, NULL);

#if 1
	launchd_init();
#else
	backend_event = backend_listen_local (sock);
	if (!backend_event)
		err (1, "backend_listen_local");
#endif

	line_fd = -1;
	while (running) {
		if (line_fd >= 0) {
			close (line_fd);
			fprintf (stderr, "EOF, reopening after sleep\n");
			sleep (1);
		}

		line_fd = open_line (line, O_RDWR);
		if (line_fd < 0)
			err (1, "open_line");

		event_set (&line_ev, line_fd, EV_READ | EV_PERSIST, line_reader, NULL);
		if (event_add (&line_ev, NULL))
			err (1, "event_add");

		enable_auto_status_layer (line_fd, &status, 1);
		status.known_fields = 0;

		if (event_dispatch ())
			err (1, "event_dispatch");
	}

	backend_close_listen (backend_event);
	return 0;
}
