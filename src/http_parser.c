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

#define BUF_SIZE 8192
#define ISspace(x) isspace((int)(x))

int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}
int http_getline(char* buf, int size, char* src_buf)
{
 int i = 0;  
 while ((i < size -1) && (src_buf[i] != '\n'))
 {
  
  buf[i] = src_buf[i];;
  i++;
 }
 buf[i] = '\0';
 
 return i;
 
}
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

void InitSockData(SockData* data, int sock)
{
  data->sock = sock;
  data->bufread_ind = data->bufwrite_ind = 0;
  data->type = NONE;
  data->paired_sock = 0;
  data->buf_read = (char *)malloc(BUF_SIZE*sizeof(char));
  data->buf_write = (char *)malloc(BUF_SIZE*sizeof(char));  
  memset(data->buf_read, 0, BUF_SIZE*sizeof(char));
  memset(data->buf_write, 0, BUF_SIZE*sizeof(char));
  memset(&data->bitratedata, 0, sizeof(data->bitratedata));

}
void FreeSockData(SockData* data)
{
   close_socket(data->sock);
   free(data->buf_read);
   free(data->buf_write);
   memset(data, 0, sizeof(*data));
}
void ResetSockData(SockData* data)
{
   memset(data->buf_read, 0, BUF_SIZE*sizeof(char));
   memset(data->buf_write, 0, BUF_SIZE*sizeof(char));
   data->bufread_ind = data->bufwrite_ind = 0;
   
}

int ReplaceURI(SockData* dst, SockData* src, char* search, char* replace)
{
  char* char_ptr = NULL;
  char* modify = dst->buf_write;
  char* origin = src->buf_read;
  if( (char_ptr = strstr(origin, search)) != NULL)
  {
    strncpy(modify, origin, char_ptr-origin);
    modify[char_ptr-origin] = 0;
    sprintf(modify+(char_ptr-origin), "%s%s", replace, char_ptr+strlen(search));
    dst->bufwrite_ind = src->bufread_ind - strlen(search) + strlen(replace);
    return 1;
  }
  else
  return 0;

}
void BitrateSelection(SockData* proxy, SockData* client, double* bitrate, int bitrate_no)
{   

  int i;
  char str[15];
  char buf[40];
  char* head = NULL;
  if(strstr(client->buf_read, "Seg") != NULL && strstr(client->buf_read, "-Frag") != NULL)
  {
       if(proxy->bitratedata.tput_current == 0)
       {
          proxy->bitratedata.tput_current = *bitrate;
          proxy->bitratedata.bitrate = *bitrate;
       }
       else
       {
           for(i = 1; i < bitrate_no; i++)
           {
             if((proxy->bitratedata.tput_current/1.5) < *(bitrate+i))
                break;
           }
         proxy->bitratedata.bitrate = *(bitrate+i-1);
       }
     sprintf(str, "%d", (int)(proxy->bitratedata.bitrate));
     ReplaceURI(proxy, client, "1000" , str);
     head = strstr(proxy->buf_write, "/vod/");
     http_getrequest(buf, sizeof(buf), head+5);
     strcpy(proxy->bitratedata.chunkname, buf);
     time(&(proxy->bitratedata.timer_s));
   }
}


void TputCalculation(SockData* proxy, double alpha)
{
  double duration;
  
  if(proxy->type != PROXY)
    fprintf(stderr, "Tput Calculation Error.\n");

  if(ChunkStart(proxy) == 0)
  {    
    if(ChunkEnd(proxy) == 1)
    {
      time(&(proxy->bitratedata.timer_f));
      duration = difftime(proxy->bitratedata.timer_f, proxy->bitratedata.timer_s);
      proxy->bitratedata.tput_new = (double)(proxy->bitratedata.chunksize*8) / (duration*1000);
      proxy->bitratedata.tput_current = (alpha) * (proxy->bitratedata.tput_new) + (1-alpha) * (proxy->bitratedata.tput_current);
      //logging
      proxy->bitratedata.chunksize = 0;
      proxy->bitratedata.remain_chunksize = 0;
    }
  }
}
int ChunkStart(SockData* proxy)
{
  char* head = NULL;
  char buf[40];
  if((head = strstr(proxy->buf_read, "Content-Length:")) != NULL && strstr(proxy->buf_read, "video/f4f") != NULL)
   {
    http_getline(buf, sizeof(buf), head);
    head = strstr(buf, " "); 
    proxy->bitratedata.chunksize = atoi(head+1);
    proxy->bitratedata.remain_chunksize = proxy->bitratedata.chunksize;
    
    head = strstr(proxy->buf_read, "\r\n\r\n");
    head += 3;
    proxy->bitratedata.remain_chunksize = proxy->bitratedata.remain_chunksize - (proxy->buf_read+proxy->bufread_ind - head);
    return 1;
   }
  else
    return 0;

}

int ChunkEnd(SockData* proxy)
{
  if(proxy->bitratedata.chunksize > 0)
  {
    proxy->bitratedata.remain_chunksize = proxy->bitratedata.remain_chunksize - proxy->bufread_ind;
    if(proxy->bitratedata.remain_chunksize == 0)
      return 1;
    else
      return 0;
  }
  else
    return 0;

}
