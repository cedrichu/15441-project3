/******************************************************************************
* http_parser.c                                                               *
*                                                                             *
* Description: This file contains the C source code for a http parser. It     *
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
#include <time.h>
#include "http_parser.h"


#define ISspace(x) isspace((int)(x))
#define SERVER_STRING "Server: Liso/1.0\r\n"

char WWW[] = "./tmp/www";


//According to the file extension, extract mime typr
int get_mime_type(char * mime, const char *filename) 
{
    
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) 
    	return 0;
    
    switch(dot[1]){
    	case 'h':
    	case 'c':
    	 strcpy(mime, "text/");
         strcat(mime, dot+1); 
         break;          
    	case 'p':
    	case 'j':
    	case 'g':
    	 strcpy(mime, "image/");
    	 strcat(mime, dot+1);
    	 break;
    	default:
    	 break;
    }
  return 1;
}

//return information for Date
void get_date(char * date, int size)
{

time_t now = time(0);
struct tm tm = *gmtime(&now);
strftime(date, size, "%a, %d %b %Y %H:%M:%S %Z", &tm);
}

void get_last_modified(char * date, int size, HTTPResponse* response)
{

  struct stat b;
  if (!stat(response->path, &b)) 
  {
  strftime(date, size, "%a, %d %b %Y %H:%M:%S %Z", localtime( &b.st_mtime));
  }
}


//read one line of received request 
int http_getline(char* buf, int size, char* src_buf)
{
 int i = 0;
 char c = '\0';
  
 while ((i < size -1) && (c != '\n'))
 {
    c = src_buf[i];
 	buf[i] = c;
 	i++;
 }
 buf[i] = '\0';
 
 return i;
 
}

//Parse the request
int http_getrequest(char* buf, int size, char* src_buf)
{
 int i = 0;
 int j = 0;

 while (ISspace(src_buf[j]))
  j++; 

 while (!ISspace(src_buf[i+j]) && (i < size-1))
 {
  buf[i] = src_buf[i+j];
  i++;
 }
 buf[i] = '\0';
 
 return i+j;
}  



//Request handler
void http_request(char* readbuf, HTTPResponse* response)
{
 char buf[1024];
 char method[255];
 char url[255];
 char path[512];
 char info[255];

 int readbuf_ind = 0;
 int buf_ind = 0;
 
//init HTTPResponse struct
response->write_byte = 0;
response->file_size = 0;
response->file =NULL;
response->header_size = 0;
strcpy(response->method,"");
strcpy(response->path,"");
//init HTTPResponse struct

readbuf_ind += http_getline(buf, sizeof(buf), readbuf+readbuf_ind);
buf_ind += http_getrequest(method, sizeof(method), buf+buf_ind);

strcpy(response->method, method); 
 if (strcasecmp(method, "GET") && strcasecmp(method, "POST") && strcasecmp(method, "HEAD"))
 {
  strcpy(response->method,"Unimplemented");
  return;
 }

buf_ind += http_getrequest(url, sizeof(url), buf+buf_ind);

sprintf(path, "%s%s",WWW, url);
if (path[strlen(path) - 1] == '/')
  strcat(path, "index.html");

if (!strcasecmp(method, "GET") || !strcasecmp(method, "HEAD"))  
 strcpy(response->path, path);

buf_ind += http_getrequest(info, sizeof(info), buf+buf_ind);
if(strcasecmp(info, "HTTP/1.1"))
  {
     strcpy(response->method,"Unsupported");
     return;
  }
}

//Response handler
void http_response(char* writebuf, int writebuf_size,  HTTPResponse* response)
{

if (!strcasecmp(response->method, "GET"))  
   http_GETHEAD_response(writebuf, writebuf_size, response);
else if(!strcasecmp(response->method, "HEAD"))
   http_GETHEAD_response(writebuf, writebuf_size, response);
else if(!strcasecmp(response->method, "POST"))
   http_POST_response(writebuf, writebuf_size, response);
else if(!strcasecmp(response->method, "Unimplemented"))
   http_unimplemented(writebuf, writebuf_size, response);
else if(!strcasecmp(response->method, "Unsupported"))
   http_unsupported(writebuf, writebuf_size, response);

}

//GET&HEAD Response handler 
void http_GETHEAD_response(char* writebuf, int writebuf_size, HTTPResponse* response)
{
 FILE *resource = NULL;
 
 resource = fopen(response->path, "r");
 if (resource == NULL)
  http_not_found(writebuf, writebuf_size, response);
 else
 {
  http_header(writebuf, writebuf_size, resource, response);
  if(!strcasecmp(response->method, "GET"))
   http_content(writebuf, writebuf_size, response);
 }

}

//POST Response handler
void http_POST_response(char* writebuf, int writebuf_size, HTTPResponse* response)
{
 char* buf = writebuf;
 char date[500];
 
 get_date(date, sizeof(date));
 strcpy(buf, "HTTP/1.1 200 OK\r\n");
 strcat(buf, SERVER_STRING);
 sprintf(buf+strlen(buf), "Date: %s\r\n", date);
 sprintf(buf+strlen(buf), "Connection: close\r\n");
 strcat(buf, "\r\n");
 response->header_size = strlen(buf);
 response->write_byte += strlen(buf);
 
}


//Process GET&HEAD header for response
void http_header(char* writebuf, int writebuf_size, FILE *resource, HTTPResponse* response)
{
 char* buf = writebuf;
 char mime[20];
 char date[500];
 char LastModified[500];
 long int size;

 get_mime_type(mime, response->path);
 get_date(date, sizeof(date));
 
 
 fseek(resource, 0L, SEEK_END);
 size = ftell(resource);
 fseek(resource, 0L, SEEK_SET);
 get_last_modified(LastModified, sizeof(LastModified), response);

 strcpy(buf, "HTTP/1.1 200 OK\r\n");
 sprintf(buf+strlen(buf), "Date: %s\r\n", date);
 strcat(buf, SERVER_STRING);
 sprintf(buf+strlen(buf), "Last-Modified: %s\r\n", LastModified);
 strcat(buf, "Connection: close\r\n");
 sprintf(buf+strlen(buf), "Content-Type: %s\r\n", mime);
 sprintf(buf+strlen(buf), "Content-Length: %ld\r\n", size);
 strcat(buf, "\r\n");
 
 if(!strcasecmp(response->method, "GET"))
  response->file_size = size;
 response->file = resource;
 response->header_size = strlen(buf);
 response->write_byte += strlen(buf);
 
}


//Process content for GET Response
void http_content(char* writebuf, int writebuf_size, HTTPResponse* response)
{
 char* buf = writebuf;
 size_t read_byte = 0;
 int buf_index = response->write_byte%writebuf_size;

read_byte = fread(buf+buf_index, 1, writebuf_size-buf_index, response->file);
response->write_byte += read_byte;
}





void http_not_found(char* writebuf, int writebuf_size, HTTPResponse* response)
{
 char* buf = writebuf;

 sprintf(buf, "HTTP/1.1 404 NOT FOUND\r\n");
 sprintf(buf+strlen(buf), SERVER_STRING);
 strcat(buf, "Connection: close\r\n");
 sprintf(buf+strlen(buf), "Content-Type: text/html\r\n");
 sprintf(buf+strlen(buf), "\r\n");
 sprintf(buf+strlen(buf), "<HTML><TITLE>Not Found</TITLE>\r\n");
 sprintf(buf+strlen(buf), "<BODY><P>The server could not fulfill\r\n");
 sprintf(buf+strlen(buf), "your request because the resource specified\r\n");
 sprintf(buf+strlen(buf), "is unavailable or nonexistent.\r\n");
 sprintf(buf+strlen(buf), "</BODY></HTML>\r\n");
 strcat(buf, "\r\n");
 
 response->header_size = strlen(buf);
 response->write_byte += strlen(buf);
 
 }

void http_unimplemented(char* writebuf, int writebuf_size, HTTPResponse* response)
{
 char* buf = writebuf;

 sprintf(buf, "HTTP/1.1 501 Method Not Implemented\r\n");
 sprintf(buf+strlen(buf), SERVER_STRING);
 strcat(buf, "Connection: close\r\n");
 sprintf(buf+strlen(buf), "Content-Type: text/html\r\n");
 sprintf(buf+strlen(buf), "\r\n");
 sprintf(buf+strlen(buf), "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
 sprintf(buf+strlen(buf), "</TITLE></HEAD>\r\n");
 sprintf(buf+strlen(buf), "<BODY><P>HTTP request method not supported.\r\n");
 sprintf(buf+strlen(buf), "</BODY></HTML>\r\n");
 strcat(buf, "\r\n");

 response->header_size = strlen(buf);
 response->write_byte += strlen(buf);
 
 }

 void http_unsupported(char* writebuf, int writebuf_size, HTTPResponse* response)
{
 char* buf = writebuf;

 sprintf(buf, "HTTP/1.1 505 Version Not Supported\r\n");
 sprintf(buf+strlen(buf), SERVER_STRING);
 strcat(buf, "Connection: close\r\n");
 sprintf(buf+strlen(buf), "Content-Type: text/html\r\n");
 sprintf(buf+strlen(buf), "\r\n");
 sprintf(buf+strlen(buf), "<HTML><HEAD><TITLE>Version Not Supported\r\n");
 sprintf(buf+strlen(buf), "</TITLE></HEAD>\r\n");
 sprintf(buf+strlen(buf), "<BODY><P>HTTP Version Not Supported.\r\n");
 sprintf(buf+strlen(buf), "</BODY></HTML>\r\n");
 strcat(buf, "\r\n");

 response->header_size = strlen(buf);
 response->write_byte += strlen(buf);
 
 }




