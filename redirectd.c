#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>     /* definition of OPEN_MAX */
#include <sys/ioctl.h>


#include "redirectd.h"

#define PORT 80
#define IP_ADDRESS "10.0.1.103"
#define REDIRECT_TO "admin2.wikarekare.org" //"10.0.1.102"
#define LINUX

int run = 1;

/*Become a background daemon process*/
void reassociate(char *tty_name)
{
int fd;
	if (fork())
		_exit(0);                       /* kill parent */
#ifdef __APPLE__
	setpgrp();
#else
	setpgrp(0,0);
#endif
	close(0);
#ifndef POSIX
#ifdef TIOCNOTTY
	if((fd = open( "/dev/tty", O_RDWR, 0)) >= 0)
	{	
		(void)ioctl(fd, TIOCNOTTY, (caddr_t)0);
		close(fd);
	}
#endif /*TIOCNOTTY*/
#else  /*POSIX*/
	(void) setsid();
#endif /*POSIX*/
#ifdef XXX
	close(1);
	close(2);
#endif
	if(tty_name)
		(void)open(tty_name, O_RDWR, 0);
	else
		(void)open("/dev/null",O_RDWR,0);
	(void)dup2(0,1);
	(void)dup2(0,2); 
}

/*Be a good citizen and don't leave zombie children wandering the system*/
static void catch_children
(
    int sig 
#ifndef LINUX
	, //Ugly, but functional. Yes this really is a ',' by itself.
    int code,
    struct sigcontext *scp
#endif
)
{
int status;
int ThePid;

    /*SIGCHLD signals are blocked  by the system when entering this function*/
    while((ThePid = wait3(&status,WNOHANG,0)) != 0 && ThePid != -1);
    /*SIGCHLD signals are unblocked  by the system when leaving this function*/
#ifdef LINUX
	signal(sig, catch_children);
#endif
}

int do_reload = 0;
/*Set do_reload flag on SIGHUP*/
static void catch_SIGHUP
(
    int sig 
#ifndef LINUX
	,
    int code,
    struct sigcontext *scp
#endif
)
{
	do_reload = 1;
#ifdef LINUX
	signal(sig, catch_SIGHUP);
#endif
}

/*The work happens here*/
void main_loop()
{
int s;
int sd;
struct sockaddr_in record;
char buf[1024];
int i, j;
char *p;
int  on = 1;
int result = 0;
struct sockaddr_in address;
socklen_t addr_len;

    if((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {   printf("socket creation failed %d\n",errno);
        exit(0);
    }
    else
    	printf("Socket opened %d\n", s);
    	
    if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(int)) == -1)
	printf("setsockopt failed: ignoring this\n");

    record.sin_family = AF_INET;
    record.sin_addr.s_addr = inet_addr(IP_ADDRESS); //INADDR_ANY;
    record.sin_port = htons(PORT);
    if(bind(s, (struct sockaddr *)&record, sizeof(record)) == -1)
    {   printf("bind failed %d\n",errno);
        exit(0);
    }
    if(listen(s,5) == -1)
    {   printf("listen failed %d\n",errno);
        exit(0);
    }

		
	signal(SIGHUP, catch_SIGHUP);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, catch_children);
		
    for(;run == 1;)
    {
	addr_len = (socklen_t)sizeof(address);
		if(do_reload)
		{
			do_reload = 0;
		//	reload(); //rereads the files.
		}
		
       	if((sd = accept(s, (struct sockaddr *)&address, &addr_len)) == -1)
       	{   	//perror("accept failed");
           	continue;
       	}

#ifndef TEST_IT
		if((result = fork()) == -1)
		{
			close(sd);
			continue;
		}
#endif
		if(result == 0)
		{
		
#ifndef TEST_IT
			close(s); /*close the listening socket for the child*/
			signal(SIGPIPE, SIG_DFL);
#endif
			for(j = 0; (i = read(sd, &buf[j], 1024 - j)) > 0 && j < 1024; j += i)
			{
				buf[j + i] = '\0';
#ifdef TEST_IT
	       		printf("%s", buf);
	       		fflush(stdout);
#endif
				if( ( (p = strrchr(buf, '\n')) && p != buf 
					&& ( *(p-1) == '\n'
						|| (*(p-1) == '\r' &&  p != &buf[1]  && *(p-2) == '\n') ) )
						|| ( (p = strrchr(buf, '\r')) && p != buf 
						&& ( *(p-1) == '\r'
						|| (*(p-1) == '\n' &&  p != &buf[1]  && *(p-2) == '\r') ) ) )
	       		{
		   			Parse_and_send_response(sd, buf);
		   			break;
		   		}
			}
       		close(sd);
#ifndef TEST_IT
			exit(0);	
#endif
		}
		else
			close(sd);
    }
    
    shutdown(s,0);
    close(s);
}

/*Quick and dirty parse of http request*/
/*Only accept GET and GET_HEAD*/
void Parse_and_send_response(int sd, char * buf)
{
char url[128];

	if(buf == 0)
	{
		send_error(sd, 400);
		return;
	}

	switch( token_to_num(Commands, strtok(buf, " \t")) )
	{
		case GET:
			//send_redirect_header(sd, strtok(0,  " \t"));
			send_307_redirect_header(sd, strtok(0,  " \t"));
			break;
		case GET_HEAD:
			//send_redirect_header(sd, strtok(0,  " \t"));
			send_307_redirect_header(sd, strtok(0,  " \t"));
			break;
		default:
			send_error(sd, 400);
			break;
	}
}

/*Error HTML message*/
void send_error(int sd, int error)
{
FILE *fp;
time_t t;

	if((fp = fdopen(sd, "w")) != 0)
	{
		time(&t);
		fprintf(fp, "HTTP/1.0 %d Not Found\n", error);
		fprintf(fp, "Date: %s", ctime(&t));
		fprintf(fp, "Server: gedserv/1.0\n");
		fprintf(fp, "Content-type: text/html\n\n");

		if(error == 404)
		{
			fprintf(fp, "<HEAD><TITLE>404 Not Found</TITLE></HEAD>\n");
			fprintf(fp, "<BODY><H1>404 Not Found</H1>\n");
			fprintf(fp, "The requested URL was not found on this server.\n");
			fprintf(fp, "</BODY>\n");
		}
		else
		{
			fprintf(fp, "<HEAD><TITLE>%d Error</TITLE></HEAD>\n", error);
			fprintf(fp, "<BODY><H1>Invalid Request</H1>\n");
			fprintf(fp, "Your client requested a transmission method other than those allowed by this server.\n");
			fprintf(fp, "</BODY>\n");
		}
		fflush(fp);
		fclose(fp);
	}
	else
		perror("fdopen");
	
}

/*send 307 redirect, which is a preserving version of 302 temporary redirect*/
void send_307_redirect_header(int sd, char *url)
{
FILE *fp;
  if( url[0] == '/' )
    url++;
  if((fp = fdopen(sd, "w")) != 0)
  {
    fprintf(fp,"HTTP/1.1 307 Found\n");
    fprintf(fp,"Location: //%s/%s\n", REDIRECT_TO, url);
    fflush(fp);
    fclose(fp);
  }
  else
    perror("fdopen");
}

/*Redirect HTML message*/
void send_redirect_header(int sd, char *url)
{
time_t t;
char time_Buff[32];
struct tm result;
FILE *fp;

  if( url[0] == '/' )
    url++;
          
	if((fp = fdopen(sd, "w")) != 0)
	{
  	time(&t);
  	fprintf(fp, "HTTP/1.0 200\n");
  	fprintf(fp, "Date: %s", asctime_r(gmtime_r(&t, &result), time_Buff ));
  	fprintf(fp, "Server: gedserv/1.0\n");
  	fprintf(fp, "Content-type: text/html\n");

  	fprintf(fp, "Last-modified: %s\n", asctime_r(gmtime_r(&t, &result), time_Buff ));
    fprintf(fp,"<head>\n<title>Webmaster redirect</title>\n");
    fprintf(fp,"<META http-equiv=\"refresh\" content=\"0;URL=http://%s/%s\">\n", REDIRECT_TO, url);
    fprintf(fp,"</head>\n");
		fflush(fp);
		fclose(fp);
	}
	else
		perror("fdopen");
}



int token_to_num(token_table *table, char *type)
{
token_table *t;

	if(table == 0 || type == 0) // check for obvious errors
		return 0;
		
	for(t = table; t->token; t++)
		if( strcmp(type, t->token) == 0)
			return t->tokenid;
	return 0;
}



int main(int argc , char **argv)
{
	
	reassociate(NULL);

	main_loop();

	return 0;
}



