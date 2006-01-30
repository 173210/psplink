/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * pcterm.c - PSPLINK pc terminal
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <limits.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <errno.h>
#include <signal.h>

#ifdef SERIAL_SUPPORT
#include <termios.h>
#endif

#define DEFAULT_PORT 23
#define HISTORY_FILE ".pcterm.hist"
#define CONNECT_RETRIES 5
#define BAUD_RATE 115200

struct Args
{
	const char *ip;
	const char *hist;
	unsigned short port;
	unsigned int baud;
	int serialmode;
	int retries;
};

enum State
{
	STATE_IDLE = 0,
	STATE_CONNECTING,
	STATE_CONNECTED
};

struct GlobalContext
{
	struct Args args;
	struct sockaddr_in serv;
	int exit;
	int conn_sanity;
	fd_set readsave;
	fd_set writesave;
	int sock;
	enum State state;
	int promptwait;
	char history_file[PATH_MAX];
};

struct GlobalContext g_context;

int fixed_write(int s, const void *buf, int len)
{
	int written = 0;

	while(written < len)
	{
		int ret;

		ret = write(s, buf+written, len-written);
		if(ret < 0)
		{
			if(errno != EINTR)
			{
				perror("write");
				written = -1;
				break;
			}
		}
		else
		{
			written += ret;
		}
	}

	return written;
}

void execute_line(const char *buf)
{
	if(g_context.state == STATE_CONNECTED)
	{
		int len;

		len = strlen(buf);

		if(len > 0)
		{
			len = fixed_write(g_context.sock, buf, len);
			if(len < 0)
			{
				close(g_context.sock);
				g_context.sock = -1;
				g_context.state = STATE_IDLE;
			}
		}

		len = fixed_write(g_context.sock, "\n", 1);

		if(len < 0)
		{
			close(g_context.sock);
			g_context.sock = -1;
			g_context.state = STATE_IDLE;
		}
	}
}

void cli_handler(char *buf)
{
	if((buf) && (*buf))
	{
		add_history(rl_line_buffer);
		if(strcmp(rl_line_buffer, "exit") == 0)
		{
			g_context.exit = 1;
		}
		else if(strcmp(rl_line_buffer, "close") == 0)
		{
			/* Exit without exiting the psplink shell */
			g_context.exit = 1;
			rl_callback_handler_remove();
			rl_callback_handler_install("", cli_handler);
			return;
		}

		/* Remove the handler and prompt */
		rl_callback_handler_remove();
		rl_callback_handler_install("", cli_handler);
		g_context.promptwait = 1;

		execute_line(buf);
	}
}

int cli_reset()
{
	if(g_context.args.serialmode)
	{
		if(g_context.sock >= 0)
		{
			char ch = 18;
			write(g_context.sock, &ch, 1);
		}
	}
	else
	{
		execute_line("reset");
	}

	return 0;
}

int cli_step()
{
	if(g_context.args.serialmode)
	{
		if(g_context.sock >= 0)
		{
			char ch = 19;
			write(g_context.sock, &ch, 1);
		}
	}
	else
	{
		execute_line("step");
	}

	return 0;
}

int cli_skip()
{
	if(g_context.args.serialmode)
	{
		if(g_context.sock >= 0)
		{
			char ch = 19;
			write(g_context.sock, &ch, 1);
		}
	}
	else
	{
		execute_line("skip");
	}

	return 0;
}

int init_readline(void)
{
	rl_bind_key('\t', rl_insert);
	rl_bind_key_in_map(META('r'), cli_reset, emacs_standard_keymap);
	rl_bind_key_in_map(META('s'), cli_step, emacs_standard_keymap);
	rl_bind_key_in_map(META('k'), cli_skip, emacs_standard_keymap);
	rl_callback_handler_install("", cli_handler);
	g_context.promptwait = 1;

	return 1;
}

#ifdef SERIAL_SUPPORT
speed_t map_int_to_speed(int baud)
{
	speed_t ret = 0;

	switch(baud)
	{
		case 4800: ret = B4800;
				   break;
		case 9600: ret = B9600;
				   break;
		case 19200: ret = B19200;
				   break;
		case 38400: ret = B38400;
				   break;
		case 57600: ret = B57600;
				   break;
		case 115200: ret = B115200;
				   break;
		default: fprintf(stderr, "Unsupport baud rate %d\n", baud);
				 break;
	};

	return ret;
}
#endif

int parse_args(int argc, char **argv, struct Args *args)
{
	memset(args, 0, sizeof(*args));
	args->port = DEFAULT_PORT;
	args->retries = CONNECT_RETRIES;
	args->baud = map_int_to_speed(BAUD_RATE);

	while(1)
	{
		int ch;
		int error = 0;

#ifdef SERIAL_SUPPORT
		ch = getopt(argc, argv, "sp:h:r:b:");
#else
		ch = getopt(argc, argv, "p:h:r:");
#endif
		if(ch < 0)
		{
			break;
		}

		switch(ch)
		{
			case 'p': args->port = atoi(optarg);
					  break;
			case 'h': args->hist = optarg;
					  break;
			case 'r': args->retries = atoi(optarg);
					  break;
#ifdef SERIAL_SUPPORT
			case 'b': args->baud = map_int_to_speed(atoi(optarg));
					  if(args->baud == 0)
					  {
						  error = 1;
					  }
					  break;
			case 's': args->serialmode = 1;
					  break;
#endif
			default : error = 1;
					  break;
		};

		if(error)
		{
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if(argc < 1)
	{
		printf("Must specify and IP address\n");
		return 0;
	}

	args->ip = argv[0];

	return 1;
}

void print_help(void)
{
	fprintf(stderr, "PCTerm Help\n");
	fprintf(stderr, "Usage: pcterm [options] ipaddr\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "-p port     : Specify the port number\n");
	fprintf(stderr, "-h history  : Specify the history file (default ~/%s)\n", HISTORY_FILE);
	fprintf(stderr, "-r retries  : Number of connection retries (default %d)\n", CONNECT_RETRIES);
#ifdef SERIAL_SUPPORT
	fprintf(stderr, "-b baud     : Specify the baud rate (default %d)\n", BAUD_RATE);
	fprintf(stderr, "-s          : Set serial mode\n");
#endif
}

int init_sockaddr(struct sockaddr_in *name, const char *ipaddr, unsigned short port)
{
	struct hostent *hostinfo;

	name->sin_family = AF_INET;
	name->sin_port = htons(port);
	hostinfo = gethostbyname(ipaddr);
	if(hostinfo == NULL)
	{
		fprintf(stderr, "Unknown host %s\n", ipaddr);
		return 0;
	}
	name->sin_addr = *(struct in_addr *) hostinfo->h_addr;

	return 1;
}

int set_socknonblock(int sock, int block)
{
	int fopt;

	fopt = fcntl(sock, F_GETFL);
	if(block)
	{
		fopt |= O_NONBLOCK;
	}
	else
	{
		fopt &= ~O_NONBLOCK;
	}

	return fcntl(sock, F_SETFL, fopt);
}

int read_socket(int sock)
{
	static char linebuf[16*1024];
	static int pos = 0;
	char buf[1024];
	char prompt[1024];
	int len;
	int promptfind = 0;

	len = read(sock, buf, sizeof(buf)-1);
	if(len < 0)
	{
		perror("read");
		return -1;
	}

	/* EOF */
	if(len == 0)
	{
		return -1;
	}

	buf[len] = 0;

	if(g_context.promptwait)
	{
		char *start;

		if(pos == 0)
		{
			start = strchr(buf, 0xFF);
			if(start)
			{
				char *end;

				end = strchr(start+1, 0xFF);
				if(end)
				{
					*end = 0;
					strcpy(prompt, start+1);
					strcpy(start, end+1);
					promptfind = 1;
				}
				else
				{
					*start = 0;
					strcpy(linebuf, start+1);
					pos = strlen(linebuf);
				}
			}
		}
		else
		{
			char *end;

			end = strchr(buf, 0xFF);
			if(end)
			{
				*end = 0;
				snprintf(prompt, sizeof(prompt), "%s%s", linebuf, buf);
				strcpy(buf, end+1);
				pos = 0;
				promptfind = 1;
			}
			else
			{
				strncat(linebuf, buf, sizeof(linebuf) - pos);
				linebuf[sizeof(linebuf)-1] = 0;
				pos = strlen(linebuf);
			}
		}
	}
	
	printf("%s", buf);
	fflush(stdout);

	if(promptfind)
	{
		printf("\n");
		rl_callback_handler_remove();
		rl_callback_handler_install(prompt, cli_handler);
	}

	return len;
}

int on_idle(void)
{
#ifdef SERIAL_SUPPORT
	if(g_context.args.serialmode)
	{
		struct termios options;

		g_context.sock = open(g_context.args.ip, O_RDWR | O_NOCTTY | O_NDELAY);
		if(g_context.sock == -1)
		{
			perror("Unable to open serial port - ");
			return 0;
		}
		else
		{
			fcntl(g_context.sock, F_SETFL, 0);
		}

		tcgetattr(g_context.sock, &options);
		cfsetispeed(&options, g_context.args.baud);
		cfsetospeed(&options, g_context.args.baud);
		options.c_cflag &= ~PARENB;
		options.c_cflag &= ~CSTOPB;
		options.c_cflag &= ~CSIZE;
		options.c_cflag |= CS8;
		options.c_cflag &= ~CRTSCTS;
		options.c_cflag |= (CLOCAL | CREAD);

		options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
		options.c_iflag &= ~(IXON | IXOFF | IXANY);
		options.c_iflag |= IGNCR;

		options.c_oflag &= ~OPOST;

		tcsetattr(g_context.sock, TCSANOW, &options);
		FD_SET(g_context.sock, &g_context.readsave);
		g_context.state = STATE_CONNECTED;
		write(g_context.sock, "\n", 1);
	}
	else
#endif
	{
		g_context.sock = socket(PF_INET, SOCK_STREAM, 0);
		if(g_context.sock < 0)
		{
			perror("socket");
			return 0;
		}
		set_socknonblock(g_context.sock, 1);
		if(connect(g_context.sock, (struct sockaddr *) &g_context.serv, sizeof(g_context.serv)) < 0)
		{
			if(errno != EINPROGRESS)
			{
				perror("connect");
				return 0;
			}

			FD_SET(g_context.sock, &g_context.writesave);
			g_context.state = STATE_CONNECTING;
		}
		else
		{
			set_socknonblock(g_context.sock, 0);
			FD_SET(g_context.sock, &g_context.readsave);
			g_context.state = STATE_CONNECTED;
		}
	}

	return 1;
}

int on_connecting(void)
{
	int err;
	socklen_t len;
	len = sizeof(err);

	if(getsockopt(g_context.sock, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
	{
		perror("getsockopt");
		return 0;
	}

	if(err != 0)
	{
		errno = err;
		if(g_context.conn_sanity >= g_context.args.retries)
		{
			perror("getsockopt");
			return 0;
		}
		else
		{
			printf("Retrying connection\n");
			g_context.conn_sanity++;
			close(g_context.sock);
			g_context.sock = -1;
			g_context.state = STATE_IDLE;
			return 1;
		}
	}

	g_context.conn_sanity = 0;
	set_socknonblock(g_context.sock, 0);
	g_context.state = STATE_CONNECTED;
	FD_SET(g_context.sock, &g_context.readsave);
	FD_CLR(g_context.sock, &g_context.writesave);
	write(g_context.sock, "\n", 1);

	return 1;
}

void shell(void)
{
	fd_set readset, writeset;

	if(g_context.args.serialmode)
	{
		printf("Opening %s baud %d\n", g_context.args.ip, g_context.args.baud);
	}
	else
	{
		printf("Opening connection to %s port %d\n", g_context.args.ip, g_context.args.port);
		if(!init_sockaddr(&g_context.serv, g_context.args.ip, g_context.args.port))
		{
			return;
		}
	}

	init_readline();
	read_history(g_context.history_file);
	history_set_pos(history_length);

	FD_ZERO(&g_context.readsave);
	FD_SET(STDIN_FILENO, &g_context.readsave);
	FD_ZERO(&g_context.writesave);

	while(!g_context.exit)
	{
		int ret;

		if(g_context.state == STATE_IDLE)
		{
			if(!on_idle())
			{
				break;
			}
		}

		readset = g_context.readsave;
		writeset = g_context.writesave;
		ret = select(FD_SETSIZE, &readset, &writeset, NULL, NULL);
		if(ret < 0)
		{
			if(errno == EINTR)
			{
				continue;
			}

			perror("select");
			break;
		}
		else if(ret == 0)
		{
			continue;
		}
		else
		{
			if(FD_ISSET(STDIN_FILENO, &readset))
			{
				rl_callback_read_char();
			}

			if(g_context.state == STATE_CONNECTED)
			{
				if(FD_ISSET(g_context.sock, &readset))
				{
					/* Do read */
					if(read_socket(g_context.sock) < 0)
					{
						close(g_context.sock);
						g_context.sock = -1;
						g_context.state = STATE_IDLE;
					}
				}
			}

			/* If connecting (never occurs if using serial) */
			if(g_context.state == STATE_CONNECTING)
			{
				if(FD_ISSET(g_context.sock, &writeset))
				{
					if(!on_connecting())
					{
						break;
					}
				}
			}
		}
	}

	write_history(g_context.history_file);
	rl_callback_handler_remove();
}

void sig_call(int sig)
{
	if((sig == SIGINT) || (sig == SIGTERM))
	{
		printf("Exiting\n");
		if(g_context.sock >= 0)
		{
			close(g_context.sock);
			g_context.sock = -1;
		}
		rl_callback_handler_remove();
		exit(0);
	}
}

void build_histfile(void)
{
	if(g_context.args.hist == NULL)
	{
		char *home;

		home = getenv("HOME");
		if(home == NULL)
		{
			snprintf(g_context.history_file, PATH_MAX, "%s", HISTORY_FILE);
		}
		else
		{
			snprintf(g_context.history_file, PATH_MAX, "%s/%s", home, HISTORY_FILE);
		}
	}
	else
	{
		snprintf(g_context.history_file, PATH_MAX, "%s", g_context.args.hist);
	}
}

int main(int argc, char **argv)
{
	memset(&g_context, 0, sizeof(g_context));
	g_context.sock = -1;
	if(parse_args(argc, argv, &g_context.args))
	{
		build_histfile();
		shell();
		if(g_context.sock >= 0)
		{
			close(g_context.sock);
		}
	}
	else
	{
		print_help();
	}

	return 0;
}
