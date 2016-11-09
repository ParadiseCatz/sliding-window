#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <netdb.h>
#include <stdbool.h>
#include <arpa/inet.h>

#include "dcomm.h"

#define LISTENQ 8 /*maximum number of client connections */
#define FINISHED 29 /*signal symbol to child process fork*/
#define UNFINISHED 30 

#define SENDDELAY 200000 /*delay when sending byte*/

FILE *fp;
char buf[MAXLEN];

int ShmID;
int *lastSignalRecv;

int sockfd,portno,pid,n;
socklen_t client;

struct sockaddr_in servaddr;
struct hostent *server;

int main (int argc, char **argv)
{

  /*Create share Memory*/
  ShmID = shmget(IPC_PRIVATE, 4*sizeof(int), IPC_CREAT | 0666);
  if (ShmID < 0) {
        printf("*** shmget error (server) ***\n");
        exit(1);
  }
  lastSignalRecv= (int *) shmat(ShmID, NULL, 0);
  lastSignalRecv[0]=XON;
  lastSignalRecv[1]=UNFINISHED;


  /*Check Argument*/
 if(argc<4) {
    printf("Argument must be 3\n");
    exit(1);
 }

 /*Convert argument port from string to int*/
 portno = atoi(argv[2]); 

 /*creation of the socket*/
 sockfd = socket (AF_INET, SOCK_DGRAM, 0);
 if (sockfd < 0)
 {
   printf("SOCKET ERROR\n");
   exit(1);
 }

 /*reset memory*/
 bzero((char *) &servaddr, sizeof(servaddr));

 /*servaddr attribute*/
 servaddr.sin_family = AF_INET;
 servaddr.sin_addr.s_addr = inet_addr(argv[1]);
 servaddr.sin_port = htons(portno);

 //// Jika menggunakan hostname di argv[1]
 // server = gethostbyname(argv[1]);

 // if (server == NULL) {
 //  fprintf(stderr,"ERROR, no such host\n");
 //  exit(0);
 // }
 // bcopy((char*)server->h_addr, (char*)&servaddr.sin_addr.s_addr, server->h_length);


 /*Open file*/
 fp = fopen(argv[3],"r");
 if (fp == NULL) {
    printf("ERROR OPENING FILE\n");
    exit(1);
 }

 /*Create Fork Process*/
 int counter = 1;
 if (fork()) {
  while(fscanf(fp,"%c",buf) != EOF) {
    bool allow = false;
    int showWait = 0;

    /*Waiting for XON*/
    while(lastSignalRecv[0] == XOFF) {
    if (!allow) {
      if (showWait>100000) {
        printf("Waiting for XON \n");
        usleep(SENDDELAY);
        showWait = 0;
      }
      if (lastSignalRecv[0] == XON) {
        allow = true;
      }
      showWait++;
    }
   }

  /*Send byte to receiver*/
   printf("Mengirim byte ke-%d: '%s'\n",counter,buf);
   sendto(sockfd,buf,strlen(buf),0,(struct sockaddr*)&servaddr,sizeof(servaddr));
   usleep(SENDDELAY);
   bzero(buf,MAXLEN);
   counter++;
  }
  
  /*Send End Signal*/
  lastSignalRecv[1]=FINISHED;
  buf[0] = Endfile;
  sendto(sockfd,buf,strlen(buf),0,(struct sockaddr*)&servaddr,sizeof(servaddr));
  printf("Exiting parent\n");
  usleep(5000000);
  
  /*Remove shared memory*/
  shmdt((void *) lastSignalRecv);
  shmctl(ShmID, IPC_RMID, NULL);

  /*Close Socket*/
  close (sockfd);
  } else {
    /* Child Process*/
    while(lastSignalRecv[1]!=FINISHED) {
      int serv_len = sizeof(servaddr);
      char _buf[MAXLEN];

      /*Receive signal*/
      n = recvfrom(sockfd,_buf,strlen(_buf),0,(struct sockaddr*)&servaddr,(socklen_t*) &serv_len);

      if (n < 0) {
         perror("ERROR reading from socket");
         exit(1);
      }

      /*Change Last Signal Receive*/
      lastSignalRecv[0] = _buf[0];
      if (lastSignalRecv[0] == XOFF) {
          printf("XOFF accepted\n");
      } else if (lastSignalRecv[0] == XON) {
          printf("XON accepted\n");
      }

    }
    printf("Exiting child...\n");
    exit(0);
  }
}
