/******************************************************************************
* http_parser.h                                                               *
*                                                                             *
* Description: This file contains the C header file for a http parser. It     *
* defines response handler and request handler and other supporting function  *
*                                                                             *
*                                                                             *
* Authors: Hsu-Chieh Hu <hsuchieh@andrew.cmu.edu>,                            *
*                                                                             *
*******************************************************************************/


#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>

typedef struct 
{
	/* data */
	char method[255];
	char path[255];
	
	long int write_byte;
	
	int header_size;
	long int file_size;
	FILE *file;
	
}HTTPResponse;



int http_getline(char *, int, char *);
int http_getrequest(char *, int, char *);
void http_request(char *, HTTPResponse *);

int get_mime_type(char *, const char *);
void get_date(char *, int);
void http_response(char*, int, HTTPResponse*);
void http_GETHEAD_response(char*, int, HTTPResponse *);
void http_POST_response(char*, int, HTTPResponse *);
void http_header(char*, int,  FILE *, HTTPResponse *);
void http_content(char*, int, HTTPResponse *);
void http_not_found(char*, int, HTTPResponse * );
void http_unimplemented(char*, int, HTTPResponse * );
void http_unsupported(char*, int, HTTPResponse * );


