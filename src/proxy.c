/******************************************************************************
* echo_server.c                                                               *
*                                                                             *
* Description: This file contains the C source code for an echo server.  The  *
*              server runs on a hard-coded port and simply write back anything*
*              sent to it by connected clients.  It does not support          *
*              concurrent clients.                                            *
*                                                                             *
* Authors: Hsu-Chieh Hu <hsuchieh@andrew.cmu.edu>                             *
*                                                                             *
*******************************************************************************/

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <netdb.h>
#include "http_parser.h"


#define BUF_SIZE 8192
#define FD_SIZE 1000

#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

//Client

char logfilename[20] = "proxy_server_log.txt";
FILE *logfile  = NULL;
double ALPHA = 0.7;
int LISTEN_PORT = 4433;
char VIDEO[40] = "big_buck_bunny.f4m";
char VIDEO_NOLIST[40] = "big_buck_bunny_nolist.f4m";
//Server
char FAKE_IP[20] = "1.0.0.1";
char SERVER_IP[20] = "3.0.0.1";
char SERVER_PORT[20] = "8080";
//char* DNS_IP;
//int DNS_PORT;


void logging(char* buf){

 FILE* file = fopen( logfilename, "a" );
 fprintf(file, "%s", buf);
 fclose(file);

}

int main(int argc, char* argv[])
{
    

 logfile = fopen( logfilename, "w" );   
 
 /*------------------Proxy Server Part-------------*/
    
   fprintf(stdout, "----- Proxy Start-----\n");
    
    int sock, client_sock;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    char err[1024];

    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(logfile, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(LISTEN_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)))
    {
        close_socket(sock);
        fprintf(logfile, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }


    if (listen(sock, 5))
    {
        close_socket(sock);
        fprintf(logfile, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }

/*---------------------Proxy Client Part START----------------------------------*/
  
    int status, proxy_client_sock;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    struct addrinfo *servinfo; //will point to the results
    struct sockaddr_in addr_proxy_client;
    
    hints.ai_family = AF_INET;  //don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; //TCP stream sockets
    hints.ai_flags = AI_PASSIVE; //fill in my IP for me


    if ((status = getaddrinfo(SERVER_IP, SERVER_PORT, &hints, &servinfo)) != 0) 
    {
        fprintf(logfile, "getaddrinfo error SERVER IP: %s \n", gai_strerror(status));
        return EXIT_FAILURE;
    }
    
    addr_proxy_client.sin_family = AF_INET;
    inet_pton(AF_INET, FAKE_IP, &(addr_proxy_client.sin_addr));
    
/*---------------------Proxy Client Part END----------------------------------*/
     
     SockData sock_data[FD_SIZE];
     memset(sock_data, 0, sizeof(sock_data));
     SockData* sdata;
     int bitrate_no = 4;
     double bitrate[4] = {10, 100, 500, 1000}; 
     
     fclose(logfile);
     int maxConn = 0;
     fd_set act_conn;
     FD_ZERO(&act_conn);
     FD_SET(sock, &act_conn);
     maxConn = sock;
     ssize_t readret, writeret;
    

    /* finally, loop waiting for input and then write it back */
    while (1)
    {
         
      fd_set readfds, writefds;
      FD_ZERO(&readfds);
      FD_ZERO(&writefds);
      FD_SET(sock, &readfds);
      int conn_i;

      readfds = act_conn;
      writefds = act_conn;

      if (select(maxConn+1, &readfds, &writefds, NULL, NULL) == -1) 
      {
            strcpy(err, "Select() return error.\n");
            logging(err);
            fprintf(stderr, "%s", err);
            exit(4);
      }
  
       for(conn_i = 0; conn_i <= maxConn; conn_i++)
       {
         sdata = &sock_data[conn_i];
         if(conn_i == sock )
         {
              if(FD_ISSET(conn_i, &readfds))
              {
                 cli_size = sizeof(cli_addr);
                 if ((client_sock = accept(conn_i, (struct sockaddr *) &cli_addr,
                                     &cli_size)) == -1)
                {
                   close(sock);
                   strcpy(err, "Error accepting connection.\n");
                   logging(err);
                   fprintf(stderr, "%s", err);
                   return EXIT_FAILURE;
                }
                else
                {
                   FD_SET(client_sock, &act_conn);
                   maxConn = max(maxConn, client_sock);
                   InitSockData(&sock_data[client_sock], client_sock);
                   sock_data[client_sock].type = CLIENT;
         
                  //Set up Porxy Client Socket for this client socket
                  if((proxy_client_sock = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
                  {
                      fprintf(logfile, "Proxy Client Socket failed");
                      printf("an error: %s\n", strerror(errno));
                      return EXIT_FAILURE;
                  }
    
                  if (bind(proxy_client_sock, (struct sockaddr *) &addr_proxy_client, sizeof(addr_proxy_client)))
                  {
                      close_socket(proxy_client_sock);
                      fprintf(logfile, "Failed binding Proxy Client Socket.\n");
                      printf("an error: %s\n", strerror(errno));
                      return EXIT_FAILURE;
                  }

                  if (connect (proxy_client_sock, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
                  {
                      fprintf(logfile, "Proxy Connect WEB Server");
                      printf("an error: %s\n", strerror(errno));
                      return EXIT_FAILURE;
                  }  

                   FD_SET(proxy_client_sock, &act_conn);
                   maxConn = max(maxConn, proxy_client_sock);                   
                   InitSockData(&sock_data[proxy_client_sock], proxy_client_sock);
                   sock_data[proxy_client_sock].type = PROXY;

                   sock_data[client_sock].paired_sock = proxy_client_sock;
                   sock_data[proxy_client_sock].paired_sock = client_sock;

                }
               }

          }
         else
         {
            if(FD_ISSET(conn_i, &readfds))
            {            
              if(sdata->type == CLIENT)
              { 
                  readret = 0; 
                  char buf_read_tmp;         
                  if((readret = read(conn_i, &buf_read_tmp, 1)) < 1)
                  {   

                      if(readret == 0)
                      {
                          
                          FD_CLR(conn_i, &act_conn);
                          FD_CLR(sdata->paired_sock, &act_conn);
                          FreeSockData(sdata);
                          FreeSockData(&sock_data[sdata->paired_sock]);
                          
                      }                     
                      if (readret == -1)
                      {
                         close_socket(sock);
                         close_socket(conn_i);
                         strcpy(err, "Error reading from client socket.\n");
                         logging(err);
                         fprintf(stderr, "%s", err);
                         return EXIT_FAILURE;
                      }
                  }
                  else
                  {

                    //Support Pipeline: When receiving '\r\n\r\n', lisod begin to process this request
                     strcpy(sdata->buf_read+sdata->bufread_ind, &buf_read_tmp);
                     sdata->bufread_ind++;
                    
                    if(buf_read_tmp == '\r')
                    {   
                      char Peek[3];
                      if(recv(conn_i, &Peek, 3, MSG_PEEK))
                      {
                        if(Peek[0] == '\n' && (Peek[1] == '\r' && Peek[2] == '\n'))
                        {
                          read(conn_i, sdata->buf_read+sdata->bufread_ind, 3);
                          sdata->bufread_ind+= 3;

                          if(ReplaceURI(sock_data+sdata->paired_sock,sdata, VIDEO, VIDEO_NOLIST) == 0)
                            if(BitrateSelection(sock_data+sdata->paired_sock, sdata, bitrate, bitrate_no) == 0)
                              {
                                strncpy(sock_data[sdata->paired_sock].buf_write, sdata->buf_read, sdata->bufread_ind);
                                sock_data[sdata->paired_sock].bufwrite_ind = sdata->bufread_ind;
                              }
                                                    
                          writeret = 0;
                          if ((writeret = write(sdata->paired_sock, sock_data[sdata->paired_sock].buf_write, sock_data[sdata->paired_sock].bufwrite_ind )) < 1)
                          {   
                            if (writeret == -1)
                            {                           
                              close_socket(sock);
                              close_socket(sdata->paired_sock);
                              strcpy(err, "Error sending to web server.\n");
                              logging(err);
                              fprintf(stderr, "%s", err);
                              return EXIT_FAILURE;
                            }
                          }
                          ResetSockData(sdata);
                          ResetSockData(&sock_data[sdata->paired_sock]);
              
                        } 
                      }
                    }
                  }
               }
               else{
                
                  readret = 0; 
                  if((readret = read(conn_i, sdata->buf_read, BUF_SIZE)) < 1)
                  {   
                      
                      if(readret == -1)
                      {
                       close_socket(sock);
                       close_socket(conn_i);
                       strcpy(err, "Error reading from web server.\n");
                       logging(err);
                       fprintf(stderr, "%s", err);
                       return EXIT_FAILURE;
                      }     
                   }
                  else
                  {   
                     sdata->bufread_ind = readret;                  
                     TputCalculation(sdata, ALPHA);
                     writeret = 0;
                     if ((writeret = write(sdata->paired_sock, sdata->buf_read, readret )) < 1)
                      {   
                        if (writeret == -1)
                        {
                          close_socket(sdata->paired_sock);
                          close_socket(sock);
                          strcpy(err, "Error sending to client socket.\n");
                          logging(err);
                          fprintf(stderr, "%s", err);
                          return EXIT_FAILURE;
                        }
                      }
                      ResetSockData(sdata);
                      ResetSockData(&sock_data[sdata->paired_sock]);  
                  }

                }


            }
          }  
       }   
    }

    close_socket(sock);
    return EXIT_SUCCESS;
}
