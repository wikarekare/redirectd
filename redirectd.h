#ifndef REDIRECT_H
#define REDIRECT_H
#include <stdio.h>

typedef struct { char * token; int tokenid; } token_table;

#define GET 1
#define GET_HEAD 2

token_table Commands[] =
{
	"GET", GET,
	"HEAD", GET_HEAD,
	0, 0
};

void reassociate(char *tty_name);
void main_loop();
void send_error(int sd, int error);
void Parse_and_send_response(int sd,char * buf);
void send_307_redirect_header(int sd, char * url);
void send_redirect_header(int sd, char * url);
int token_to_num(token_table *table, char *type);

#endif
