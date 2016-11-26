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
#include <thread>
#include <ctime>
#include <chrono>
#include <vector>

#include "dcomm.h"

#define LISTENQ 8 /*maximum number of client connections */
#define FINISHED 29 /*signal symbol to child process fork*/
#define UNFINISHED 30

#define SENDDELAY 200000 /*delay when sending byte*/
#define WINDOWSIZE 123
#define TIMEOUTDELAY 5000000
#define TIMEOUTDELAYSTEP 500000
#define MAXPACKET 1024

using namespace std;

FILE* fp;
char buf[MAXLEN];

int sockfd, portno, pid, n;
socklen_t client;

struct sockaddr_in servaddr;
struct hostent* server;

int bufferPos = 0;
char frameNum = 0;

// shared varible
int swithOnOff = XON;
int switchFinish = UNFINISHED;
char lastACK = 0;
bool forceTimeout[MAXPACKET];
vector<vector<char> > bufferArchive;
// end shared variable

void sender(FILE*);
void listener();
void pushToBuffer(char);
void resend(char);
void forceSend();
char getChecksum(char*,int,int);

int main(int argc, char** argv) {
  /*Check Argument*/
  if (argc < 4) {
    printf("Argument must be 3\n");
    exit(1);
  }

  /*Convert argument port from string to int*/
  portno = atoi(argv[2]);

  /*Open file*/
  fp = fopen(argv[3], "r");

  if (fp == NULL) {
    printf("ERROR OPENING FILE\n");
    exit(1);
  }


  /*creation of the socket*/
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  if (sockfd < 0) {
    printf("SOCKET ERROR\n");
    exit(1);
  }

  /*reset memory*/
  bzero((char*)&servaddr, sizeof(servaddr));

  /*servaddr attribute*/
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr(argv[1]);
  servaddr.sin_port = htons(portno);

  /*Create Thread Process*/
  thread senderThread (sender, fp);     // spawn new thread that calls foo()
  thread listenerThread (listener);

  senderThread.join();
  listenerThread.join();


}


void sender(FILE* fp) {
  char currentChar;

  while (fscanf(fp, "%c", &currentChar) != EOF) {
    bool allow = false;
    int showWait = 0;

    /*Waiting for XON*/
    while (swithOnOff == XOFF || frameNum - lastACK > WINDOWSIZE) {
      if (!allow) {
        if (showWait > 100000) {
          printf("Waiting for XON \n");
          usleep(SENDDELAY);
          showWait = 0;
        }

        if (swithOnOff == XON) {
          allow = true;
        }

        showWait++;
      }
    }

    pushToBuffer(currentChar);
  }

  /*Send End Signal*/
  switchFinish = FINISHED;
  pushToBuffer(Endfile);

  if (bufferPos) {
    forceSend();
  }

  printf("Exiting Sender Thread\n");
  usleep(5000000);

  /*Close Socket*/
  close(sockfd);
}

// force socket send even when buffer is not full
void forceSend() {
  buf[bufferPos++] = ETX;
  buf[bufferPos++] = getChecksum(buf, 3, MAXLEN - 2);
  vector<char> tmpBuf;

  for (int i = 0; i < strlen(buf); ++i) {
    tmpBuf.push_back(buf[i]);
  }

  bufferArchive.push_back(tmpBuf);
  sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&servaddr, sizeof(servaddr));
  bzero(buf, MAXLEN);
  bufferPos = 0;
  frameNum++;
}

// delay socket send until buffer full
void pushToBuffer(char c) {
  if (bufferPos == 0) {
    buf[bufferPos++] = SOH;
    buf[bufferPos++] = frameNum;
    buf[bufferPos++] = STX;
  }

  buf[bufferPos++] = c;

  if (bufferPos == MAXLEN - 2) {
    buf[bufferPos++] = ETX;
    buf[bufferPos++] = getChecksum(buf, 3, MAXLEN - 2);
    vector<char> tmpBuf;

    for (int i = 0; i < strlen(buf); ++i) {
      tmpBuf.push_back(buf[i]);
    }

    bufferArchive.push_back(tmpBuf);
    sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&servaddr, sizeof(servaddr));
    bzero(buf, MAXLEN);
    bufferPos = 0;
    frameNum++;
  }
}

//timer countdown for each packet
void timer(char thisFrameNum) {
  chrono::steady_clock::time_point startTime = chrono::steady_clock::now();
  chrono::steady_clock::time_point currentTime = chrono::steady_clock::now();

  while (lastACK <= thisFrameNum) {
    while (chrono::duration_cast<chrono::microseconds>(currentTime - startTime).count() < TIMEOUTDELAY && !forceTimeout[thisFrameNum]) {
      usleep(TIMEOUTDELAYSTEP);
      currentTime = chrono::steady_clock::now();
    }

    if (forceTimeout[thisFrameNum]) {
      forceTimeout[thisFrameNum] = false;
      resend(thisFrameNum);
      startTime = chrono::steady_clock::now();
      continue;
    }

    if (lastACK <= thisFrameNum)
      return;

    resend(thisFrameNum);
    startTime = chrono::steady_clock::now();
  }
}

// resenc packet using buffer archive
void resend(char thisFrameNum) {
  char thisBuf[MAXLEN];

  for (int i = 0; i < bufferArchive[thisFrameNum].size(); ++i) {
    thisBuf[i] = bufferArchive[thisFrameNum][i];
  }

  sendto(sockfd, thisBuf, strlen(thisBuf), 0, (struct sockaddr*)&servaddr, sizeof(servaddr));
}

void listener() {
  while (switchFinish != FINISHED) {
    int serv_len = sizeof(servaddr);
    char thisBuf[MAXLEN];

    /*Receive signal*/
    n = recvfrom(sockfd, thisBuf, strlen(thisBuf), 0, (struct sockaddr*)&servaddr, (socklen_t*)&serv_len);

    if (n < 0) {
      perror("ERROR reading from socket");
      exit(1);
    }

    /*Change Last Signal Receive*/
    char signal = thisBuf[0];

    switch (signal) {
    case XOFF: printf("XOFF accepted\n"); swithOnOff = XOFF; break;

    case XON: printf("XON accepted\n"); swithOnOff = XON; break;

    case ACK: 
      printf("ACK accepted\n"); 
      if (getChecksum(thisBuf, 1, 2) == thisBuf[2]) {
        printf("ACK checksum OK\n");
        lastACK = max(lastACK, thisBuf[1]);
      } else {
        printf("ACK checksum FAILED\n");
      }
      break;

    case NAK: 
      printf("NAK accepted\n"); 
      if (getChecksum(thisBuf, 1, 2) == thisBuf[2]) {
        printf("NAK checksum OK\n");
        forceTimeout[thisBuf[1]] = true;
      } else {
        printf("NAK checksum FAILED\n");
      }
      break;

    default: printf("ERROR get unknown signal %d\n", signal);
    }
  }

  printf("Exiting Listener Thread\n");
}

char getChecksum(char *c, int start, int end) {
  char checksum = 0;
  for (int i = start; i < end; ++i)
  {
    checksum ^= c[i];
  }
  return checksum;
}
