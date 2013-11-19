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

int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}


int main(int argc, char* argv[])
{
    

 logfile = fopen( logfilename, "w" );   
 
 /*------------------Proxy Server Part-------------*/
    
    ssize_t readret, writeret;
    char* buf_read[FD_SIZE];
    int bufread_ind[BUF_SIZE];
    char* buf_write[FD_SIZE];
    int bufwrite_ind[BUF_SIZE];
   
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

    
     fclose(logfile);
     int maxConn = 0;
     fd_set act_conn;
     FD_ZERO(&act_conn);
     FD_SET(sock, &act_conn);
     maxConn = sock;
     int client_to_proxy_client_map[FD_SIZE];
     memset(client_to_proxy_client_map, 0, sizeof(client_to_proxy_client_map));
     int proxy_client_to_client_map[FD_SIZE];
     memset(proxy_client_to_client_map, 0, sizeof(proxy_client_to_client_map));

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
                   bufread_ind[client_sock] = bufwrite_ind[client_sock] = 0;
                   buf_read[client_sock] = (char *)malloc(BUF_SIZE*sizeof(char));
                   buf_write[client_sock] = (char *)malloc(BUF_SIZE*sizeof(char));
                   memset(buf_read[client_sock], 0, BUF_SIZE);
                   memset(buf_write[client_sock], 0, BUF_SIZE);
                   maxConn = max(maxConn, client_sock);
                  
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
                   bufread_ind[proxy_client_sock] = bufwrite_ind[proxy_client_sock] = 0;
                   buf_read[proxy_client_sock] = (char *)malloc(BUF_SIZE*sizeof(char));
                   buf_write[proxy_client_sock] = (char *)malloc(BUF_SIZE*sizeof(char));
                   memset(buf_read[proxy_client_sock], 0, BUF_SIZE);
                   memset(buf_write[proxy_client_sock], 0, BUF_SIZE);
                   maxConn = max(maxConn, proxy_client_sock);

                   client_to_proxy_client_map[client_sock] = proxy_client_sock; 
                   proxy_client_to_client_map[proxy_client_sock] = client_sock; 

                }
               }

          }
         else
         {
            if(FD_ISSET(conn_i, &readfds))
            {            
              if(client_to_proxy_client_map[conn_i] != 0)
              { 
                  readret = 0; 
                  char buf_read_tmp;         
                  if((readret = read(conn_i, &buf_read_tmp, 1)) < 1)
                  {   

                      if(readret == 0)
                      {
                          close_socket(conn_i);                      
                          free(buf_read[conn_i]);
                          free(buf_write[conn_i]);
                          FD_CLR(conn_i, &act_conn);                         
                      }                     
                      if (readret == -1)
                      {
                         close_socket(conn_i);
                         close_socket(sock);
                         FD_CLR(conn_i, &act_conn);
                         strcpy(err, "Error reading from client socket.\n");
                         logging(err);
                         fprintf(stderr, "%s", err);
                         return EXIT_FAILURE;
                      }
                  }
                  else
                  {

                    //Support Pipeline: When receiving '\r\n\r\n', lisod begin to process this request
                     strcpy(buf_read[conn_i]+bufread_ind[conn_i], &buf_read_tmp);
                     bufread_ind[conn_i]++;
                    
                    if(buf_read_tmp == '\r')
                    {   
                      char Peek[3];
                      if(recv(conn_i, &Peek, 3, MSG_PEEK))
                      {
                        if(Peek[0] == '\n' && (Peek[1] == '\r' && Peek[2] == '\n'))
                        {
                          read(conn_i, buf_read[conn_i]+bufread_ind[conn_i], 3);
                          bufread_ind[conn_i]+= 3;

                          if ((writeret = write(client_to_proxy_client_map[conn_i], buf_read[conn_i], bufread_ind[conn_i]) ) < 1)
                          {   
                            if (writeret == -1)
                            {                           
                              strcpy(err, "Error sending to web server.\n");
                              logging(err);
                              fprintf(stderr, "%s", err);
                              return EXIT_FAILURE;
                            }
                          }
                          
                          memset(buf_read[conn_i], 0, BUF_SIZE);// reset read buffer memory
                          bufread_ind[conn_i] = 0;
              
                        } 
                      }
                    }
                  }
               }
               else{
                
                  readret = 0; 
                  if((readret = read(conn_i, buf_read[conn_i], BUF_SIZE)) < 1)
                  {   
                      
                      if(readret == -1)
                      {
                       close_socket(conn_i);
                       FD_CLR(conn_i, &act_conn);
                       strcpy(err, "Error reading from web server.\n");
                       logging(err);
                       fprintf(stderr, "%s", err);
                       return EXIT_FAILURE;
                      }     
                   }
                  else
                  {
                     if ((writeret = write(proxy_client_to_client_map[conn_i], buf_read[conn_i], readret )) < 1)
                      {   
                        if (writeret == -1)
                        {
                          close_socket(proxy_client_to_client_map[conn_i]);
                          close_socket(sock);
                          strcpy(err, "Error sending to client socket.\n");
                          logging(err);
                          fprintf(stderr, "%s", err);
                          return EXIT_FAILURE;
                        }
                      }
                      memset(buf_read[conn_i], 0, BUF_SIZE);   
                      
                  }

                }


            }
          }  
       }   
    }

    close_socket(sock);
    return EXIT_SUCCESS;
}
