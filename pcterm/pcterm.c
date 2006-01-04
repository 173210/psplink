/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * pcterm.c - PSPLINK wifi pc terminal
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
#include <readline/readline.h>
#include <readline/history.h>
#include <errno.h>
#include <signal.h>

#define DEFAULT_PORT 23

struct Args
{
	const char *ip;
	unsigned short port;
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
	int sock;
	enum State state;
	int promptwait;
	char line_buffer[16*1024];
	int  line_pos;
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

		len = fixed_write(g_context.sock, buf, strlen(buf));
		if(len < 0)
		{
			close(g_context.sock);
			g_context.sock = -1;
			g_context.state = STATE_IDLE;
		}

		len = fixed_write(g_context.sock, "\r\n", 2);
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
	if ((buf) && (*buf))
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
			return;
		}

		/* Remove the handler and prompt */
		rl_callback_handler_remove();
		rl_callback_handler_install("", cli_handler);
		g_context.promptwait = 1;


		execute_line(buf);
	}
}

int init_readline(void)
{
	rl_bind_key('\t', rl_insert);
	rl_callback_handler_install("", cli_handler);
	g_context.promptwait = 1;

	return 1;
}

int parse_args(int argc, char **argv, struct Args *args)
{
	memset(args, 0, sizeof(*args));
	args->port = DEFAULT_PORT;

	while(1)
	{
		int ch;
		int error = 0;

		ch = getopt(argc, argv, "p:");
		if(ch < 0)
		{
			break;
		}

		switch(ch)
		{
			case 'p': args->port = atoi(optarg);
					  break;
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

void shell(void)
{
	fd_set readset, readsave;
	fd_set writeset, writesave;

	printf("Opening connection to %s port %d\n", g_context.args.ip, g_context.args.port);
	if(!init_sockaddr(&g_context.serv, g_context.args.ip, g_context.args.port))
	{
		return;
	}

	init_readline();

	FD_ZERO(&readsave);
	FD_SET(STDIN_FILENO, &readsave);
	FD_ZERO(&writesave);

	while(!g_context.exit)
	{
		int ret;

		if(g_context.state == STATE_IDLE)
		{
			g_context.sock = socket(PF_INET, SOCK_STREAM, 0);
			if(g_context.sock < 0)
			{
				perror("socket");
				break;
			}
			set_socknonblock(g_context.sock, 1);
			if(connect(g_context.sock, (struct sockaddr *) &g_context.serv, sizeof(g_context.serv)) < 0)
			{
				if(errno != EINPROGRESS)
				{
					perror("connect");
					break;
				}

				FD_SET(g_context.sock, &writesave);
				g_context.state = STATE_CONNECTING;
			}
			else
			{
				FD_SET(g_context.sock, &readsave);
				g_context.state = STATE_CONNECTED;
			}
		}

		readset = readsave;
		writeset = writesave;
		ret = select(FD_SETSIZE, &readset, &writeset, NULL, NULL);
		if(ret < 0)
		{
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

			if(g_context.state == STATE_CONNECTING)
			{
				if(FD_ISSET(g_context.sock, &writeset))
				{
					int err;
					socklen_t len;
					len = sizeof(err);

					if(getsockopt(g_context.sock, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
					{
						perror("getsockopt");
						break;
					}

					if(err != 0)
					{
						errno = err;
						perror("getsockopt");
						break;
					}

					set_socknonblock(g_context.sock, 0);
					g_context.state = STATE_CONNECTED;
					FD_SET(g_context.sock, &readsave);
					FD_CLR(g_context.sock, &writesave);
				}
			}
		}
	}

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

int setup_signals(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_call;
	sa.sa_flags = SA_RESTART;
	(void) sigaction(SIGINT, &sa, NULL);
	(void) sigaction(SIGTERM, &sa, NULL);

	return 0;
}

int main(int argc, char **argv)
{
	memset(&g_context, 0, sizeof(g_context));
	g_context.sock = -1;
	if(parse_args(argc, argv, &g_context.args))
	{
		setup_signals();
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
