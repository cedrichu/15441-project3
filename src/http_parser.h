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
#include <sys/time.h>
#include <stdlib.h>


enum socktype {NONE, PROXY, CLIENT};

typedef struct $
{
	double bitrate;
    double tput_current;
    double tput_new;

}TputData;

typedef struct 
{
  struct timeval start;
  struct timeval stop;
  double duration;
  //should be cleared when chunk starts
  char chunkname[100];
  int chunksize;
  int remain_chunksize;

} BitrateData;     


typedef struct{

int sock;
char* buf_read;
char* buf_write;
int bufwrite_ind;
int bufread_ind;

enum socktype type;
int paired_sock;
BitrateData bitratedata;

} SockData;

int http_getline(char *, int, char *);
int http_getrequest(char *, int, char *);
int close_socket(int sock);
void InitSockData(SockData* sock_data, int sock);
void FreeSockData(SockData* sock_data);
void ResetSockData(SockData* sock_data);

int ReplaceURI(SockData* dst, SockData* src, char* search, char* replace);
int BitrateSelection(SockData* proxy, SockData* client, TputData* tput_data);
void TputCalculation(SockData* proxy, double alpha, TputData* tput_data, double* bitrate, int bitrate_no);
int ChunkStart(SockData* proxy);
int ChunkEnd(SockData* proxy);

