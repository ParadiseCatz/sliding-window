#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include <stdbool.h>

#include "dcomm.h"

#define LISTENQ 8 /*maximum number of client connections */


static void *sendSignal(void*);
FILE *fp;
char lastSignalRecv = XON;
char buf[MAXLEN];

int sockfd,portno,pid,n;
socklen_t client;

struct sockaddr_in servaddr;
struct hostent *server;

int main (int argc, char **argv)
{

 if(argc<4) {
    printf("Argument must be 3\n");
    exit(1);
 }

 portno = atoi(argv[2]); 

 //creation of the socket
 sockfd = socket (AF_INET, SOCK_DGRAM, 0);
 if (sockfd < 0)
 {
   printf("SOCKET ERROR\n");
   exit(1);
 }

 server = gethostbyname(argv[1]);

 if (server == NULL) {
  fprintf(stderr,"ERROR, no such host\n");
  exit(0);
 }

 bzero((char*) &servaddr,sizeof(servaddr));
 //preparation of the socket address
 servaddr.sin_family = AF_INET;
 bcopy((char*)server->h_addr, (char*)&servaddr.sin_addr.s_addr, server->h_length);
 servaddr.sin_port = htons(portno);

 if (connect (sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
 {
   printf("CONNECT ERROR\n");
   exit(1);
 }

 puts("Socket Connected\n");

 
 /* Send message to the server */
 n = write(sockfd, buf, strlen(buf));
 
 if (n < 0) {
      perror("ERROR writing to socket");
      exit(1);
 }

 printf("Parent Thread PID : %d\n",getpid());

 pthread_t signal_thread;

 if (pthread_create(&signal_thread,NULL,&sendSignal,NULL)) {
    printf("ERROR CREATING THREAD\n");
    exit(1);
 }


 fp = fopen(argv[3],"r");

 if (fp == NULL) {
    printf("ERROR OPENING FILE\n");
    exit(1);
 }

 int counter = 1;
 while(fscanf(fp,"%c",buf) != EOF) {
   bool allow = false;
   while(lastSignalRecv == XOFF) {
    if (!allow) {
      printf("Waiting XON\n");
      allow = true;
    }
   }
 printf("Mengirim byte ke-%d: '%s'\n",counter,buf);
 sendto(sockfd,buf,strlen(buf),0,(struct sockaddr*)&servaddr,sizeof(servaddr));
 bzero(buf,MAXLEN);
 counter++;
 }

 printf("Exiting parent\n");

 //close listening socket
 close (sockfd);

 return 0;
}

static void *sendSignal(void* param) {
  while(true) {
    int serv_len = sizeof(servaddr);
    char _buf[MAXLEN];

    n = recvfrom(sockfd,_buf,strlen(_buf),0,(struct sockaddr*)&servaddr,(socklen_t*) &serv_len);
    // buffer[n]=0;
    // fputs(_buffer,stdout);

    if (n < 0) {
       perror("ERROR reading from socket");
       exit(1);
    }

    lastSignalRecv = _buf[0];
    if (lastSignalRecv == XOFF) {
        printf("XOFF accepted\n");
    } else if (lastSignalRecv == XON) {
        printf("XON accepted\n");
    }

  }
  printf("Exiting child...\n");
  pthread_exit(0);  
}


