/* httpd.c */

#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LISTENADDR "0.0.0.0"

/* structures */
struct sHttpRequest {
	char method[8];
	char url[128];
};
typedef struct sHttpRequest httpreq;

struct sFile {
	char filename[64];
	char *fc;	/* fc = file content */
	int size;
};
typedef struct sFile File;

/* global */
char *error;

/* returns 0 on error, or it returns a socket fd */
int srv_init(int portno)
{
	int s;
	struct sockaddr_in srv;	// intializing server

	s = socket(AF_INET, SOCK_STREAM, 0);	// intializing socket
	if (s < 0)
	{
		error = "socket() error";
		return 0;
	}

	/* setting properties of server according to the socket */
	srv.sin_family = AF_INET;
	srv.sin_addr.s_addr = inet_addr(LISTENADDR);
	srv.sin_port = htons(portno);

	// binding socket and server
	if(bind(s, (struct sockaddr *)&srv, sizeof(srv)))	// if otherthan 0, binding failed
	{
		error = "bind() error";
		close(s);	// close server
		return 0;
	}

	// socket listening for up to 5 connections at once (backlog), additional connections will be in waiting queue
	if (listen(s, 5))
	{
		close(s);	// close server
		error = "listen() error";
		return 0;
	}

	return s;	// returns socket file descriptor
}

/* retuns 0 on error, or returns the new client's socket fd */
int cli_accept(int s)
{
	int c;	// client socket fd
	socklen_t addrlen;
	struct sockaddr_in cli;

	addrlen = 0;
	memset(&cli, 0, sizeof(cli));
	c = accept(s, (struct sockaddr *)&cli, &addrlen); // returns client socket fd
	// accept accepts any incoming client request to our socket s, set cli with info of client address and addrlen with cli size

	/* accept is a blocking function, it waits for a client connection in queue where there is it fetches the first client
		create a socket fd for client and returns it */

	if (c < 0)
	{
		error = "accept() error";
		return 0;
	}

	return c;
}

/* return 0 on error, or retun the data */
char *cli_read(int c) 
{
	static char buf[512];
	memset(buf, 0, 512);
	if (read(c, buf, 511) < 0)
	{
		error = "read() error";
		return 0;
	}
	else
		return buf;
}


/* returns 0 on error or it returns httpreq structure*/
httpreq *parse_http(char *str)
{
	httpreq *req;
	char *p;

	req = malloc(sizeof(httpreq));
	memset(req, 0, sizeof(httpreq));

	for (p = str; p && *p != ' '; p++);
	if (*p == ' ')
		*p = 0;
	else
	{
		error = "parse_http() NOSPACE error";
		free(req);
		return 0;
	}

	strncpy(req->method, str, 7);

	for (str = ++p; p && *p != ' '; p++);
        if (*p == ' ')
                *p = 0;
        else
        {
                error = "parse_http() 2ND NOSPACE error";
                free(req);
                return 0;
        }

        strncpy(req->url, str, 127);

	return req;
}

void http_headers(int c, int code)
{
	int n;
	char buf[512];
	memset(buf, 0, 512);

	snprintf(buf, 511,
		"HTTP/1.0 %d OK\n"
		"Server: httpd.c\n"
		"X-Frame-Options: SAMEORIGIN\n"
		"Content-Language: en\n"
		"Expires: -1\n", code);

	n = strlen(buf);
	write(c, buf, n);

	return;
}

void http_response(int c, char *contenttype, char *data)
{
	char buf[512];
	int n;

	n = strlen(data);
	memset(buf, 0, 512);
	snprintf(buf, 511, 
		"Content-Type: %s\n" 
		"Content-Length: %d\n"
		"\n%s\n",
		contenttype, n, data);

	n = strlen(buf);
	write(c, buf, n);
	return;
}


/* 0 - error or File structure */
File *readfile(char *filename)
{
	char buf[512];
	char *p;
	int n, x, fd;
	File *f;

	fd = open(filename, O_RDONLY);

	if (fd < 0)
		return 0;

	f = malloc(sizeof(struct sFile));
	if (!f)
	{
		close(fd);
		return 0;
	}

	strncpy(f->filename, filename, 63);
	f->fc  = malloc(0);

	x = 0;	/* bytes read */
	while(1)
	{
		memset(buf, 0, 512);
		n = read(fd, buf, 512);
		f->fc = realloc(f->fc, x + n);
		memcpy((f->fc)+x, buf, n);
		x += n;

		if (!n)
			break;
		else if (x == -1)
		{
			close(fd);
			free(f->fc);
			free(f);

			return 0;
		}

	}
	f->size = x;
	close(fd);
	return f;
}

/* 1 = ok , 0 = error */
int sendfile(int c, char *contenttype, File *file)
{
	char buf[512];
	char *p;
	int n, x;

	if (!file)
		return 0;
	
	memset(buf, 0, 512);
	snprintf(buf, 511, 
		"Content-Type: %s\n" 
		"Content-Length: %d\n\n",
		contenttype, file->size);

	
	n = strlen(buf);
	write(c, buf, n);

	n = file -> size;
	p = file->fc;
	while (1)
	{
		x = write(c, p, (n < 512) ? n : 512);

		if (x < 1)
			return 0;

		n -= x;
		
		if (n < 1)
			break;
		else
			p += x;
	}

	return 1;
}

void send404error(int c, char *res) {
	res = "File not found";
	http_headers(c, 404);	/* 404 - file not found */
	http_response(c, "text/plain", res);
}

char *getfileextension(const char *filename)
{
	const char *dot = strchr(filename, '.');
	if (!dot || dot == filename)
	{
		return "";
	}
	return (char *)dot + 1;
}


void cli_conn(int s, int c)
{
	httpreq *req;
	char *p;
	char *res;
	char str[96];
	File *f;
	char *contenttype;
	char *extension;

	p = cli_read(c);	// c is client socket fd
	if (!p)
	{
		fprintf(stderr, "%s\n", error);
		close(c);
		return;
	}

	req = parse_http(p);
	if (!req)
	{
		fprintf(stderr, "%s\n", error);
		close(c);
		return;
	}

	if (strstr(req->url, "..")) {
		http_headers(c, 300);	/* 300-series = access denied */
		res = "Access denied";
		http_response(c, "text/plain", res);
	}
	else if (!strcmp(req->method, "GET") && !strcmp(req->url, "/"))
	{
		f = readfile("./index.html");	// getting opened file fd
		if (!f)
			send404error(c, res);
		else
		{
			http_headers(c, 200);
			if (!sendfile(c, "text/html", f))
			{
				res = "HTTP server error";
				http_response(c, "text/plain", res);
			}
		}	
	}
	else if (!strcmp(req->method, "GET") && !strncmp(req->url, "/", 1))	// strncmp checks for first n chracter match
	{
		memset(str, 0, 96);
		snprintf(str, 95, ".%s", req->url);// copying in str : .${req->url} because open() require relative path

		f = readfile(str);	// getting opened file fd

		if (!f)
			send404error(c, res);
		else
		{
			extension = getfileextension(req->url);

			if (!strcmp(extension, "html"))		contenttype = "text/html";
			else if (!strcmp(extension, "css")) 	contenttype = "text/css";
			else if (!strcmp(extension, "js"))	contenttype = "text/javascript";
			else if (!strcmp(extension, "png"))	contenttype = "image/png";
			else if (!strcmp(extension, "jpg"))	contenttype = "image/jpg"; 
			else if (!strcmp(extension, "ico"))	contenttype = "image/x-icon";
			else {
				send404error(c, res);
				free(req);
				close(c);
				return;
			}

			http_headers(c, 200);

			if (!sendfile(c, contenttype, f))
			{
				res = "HTTP server error";
				http_response(c, "text/plain", res);
			}
		}
	}
	else
		send404error(c, res);

	free(req);
	close(c);
	return;
}

int main(int argc, char *argv[])
{
	int s, c;
	char *port;

	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <listening port>\n", argv[0]);
		return -1;
	}
	else
		port = argv[1];

	s = srv_init(atoi(port));	// atoi to change char* to int
	if (!s)
	{
		fprintf(stderr, "%s\n", error);
		return -1;
	}

	printf("Listening on %s:%s\n", LISTENADDR, port);
	while (1)
	{
		c = cli_accept(s);
		if (!c)
		{
			fprintf(stderr, "%s\n", error);
			continue;
		}

		printf("\nIncoming connection\n");

		/* fork for main process: returns new process id for the new process: returns 0 */
		if (!fork())
		{
			cli_conn(s, c);
			return 0;
		}

	}

	return -1;
}
