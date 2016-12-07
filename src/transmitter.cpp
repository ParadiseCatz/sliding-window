#include <stdlib.h>
#include "mutex"
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
#include <algorithm>

#include <regex.h>

#include "dcomm.h"

#define LISTENQ 8 /*maximum number of client connections */
#define FINISHED 29 /*signal symbol to child process fork*/
#define UNFINISHED 30

#define SENDDELAY 200000 /*delay when sending byte*/
#define TIMEOUTDELAY 5000000
#define TIMEOUTDELAYSTEP 500000

using namespace std;

FILE* fp;
char buf[MAXLEN];

int sockfd, portno, pid, n;
socklen_t client;

struct sockaddr_in servaddr;
struct hostent* server;

int bufferPos = 0;
Frame frameNum;

// shared varible
int swithOnOff = XON;
int switchFinish = UNFINISHED;
int lastACK;
vector<bool> forceTimeout;
vector<vector<char> > bufferArchive;
// end shared variable

void sender(FILE*);
void listener();
void pushToBuffer(char);
void packetTimer(int);
void resend(int);
void forceSend();
char getChecksum(char*, int, int);
Frame toFrame(char*);
Frame toFrame(int);


bool is_ip_address;

mutex resend_mutex;

int main(int argc, char** argv) {
  /*Check Argument*/
  if (argc < 4) {
    printf("ERROR usage ./transmitter <receiver addr> <reciever port> <filepath>\n");
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
  // servaddr.sin_family = AF_INET;
  // servaddr.sin_addr.s_addr = inet_addr(argv[1]);
  // servaddr.sin_port = htons(portno);

int r;
  regex_t reg;

    if (r = regcomp(&reg, "^(\\d{0,3}\\.){3}\\d{0,3}$", REG_NOSUB | REG_EXTENDED) ){
        char errbuf[1024];
        regerror(r, &reg, errbuf, sizeof(errbuf));
        printf("error: %s\n", errbuf);
        return 1;
    }
    
    if (regexec(&reg, argv[1],0,NULL,0) != REG_NOMATCH){
    is_ip_address = true;
  }

    if ( is_ip_address ) {
        servaddr.sin_addr.s_addr = inet_addr(argv[1]);

        printf("Membuat socket untuk koneksi ke %s:%s ...", argv[1], argv[2]);
    } else {
        /* Map host name to ip address */
        server = gethostbyname(argv[1]);

        if (!server) {
           fprintf(stderr,"ERROR, no such host\n");
           exit(0);
        }

        bcopy((char *)server->h_addr, (char *)&servaddr.sin_addr.s_addr, server->h_length);
    }
    servaddr.sin_port = htons(portno);



  /*Create Thread Process*/
  printf("CREATING THREAD\n");
  thread senderThread (sender, fp);     // spawn new thread that calls foo()
  thread listenerThread (listener);

  senderThread.join();
  listenerThread.join();


}


void sender(FILE* fp) {
  printf("START SENDER THREAD\n");
  char currentChar;

  int charCounter = 0;
  while (fscanf(fp, "%c", &currentChar) != EOF) {
    bool allow = false;
    int showWait = 0;

    /*Waiting for XON*/
    while (swithOnOff == XOFF || frameNum.intVersion - lastACK >= WINDOWSIZE) {
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
    printf("PUSHING CHAR no. %d: %c\n", charCounter, currentChar);
    charCounter++;
    pushToBuffer(currentChar);
  }

  /*Send End Signal*/
  pushToBuffer(Endfile);

  if (bufferPos) {
    forceSend();
  }

  while (lastACK < frameNum.intVersion) {}

  printf("Exiting Sender Thread\n");
  switchFinish = FINISHED;
  /*Close Socket*/
  shutdown(sockfd, SHUT_RDWR);
  close(sockfd);
}

// force socket send even when buffer is not full
void forceSend() {
  // create footer
  char checksum = getChecksum(buf, 6, bufferPos);
  buf[bufferPos++] = ETX;
  buf[bufferPos++] = checksum;
  vector<char> tmpBuf;

  for (int i = 0; i < bufferPos; ++i) {
    tmpBuf.push_back(buf[i]);
  }

  bufferArchive.push_back(tmpBuf);
  forceTimeout.push_back(false);
  sendto(sockfd, buf, bufferPos, 0, (struct sockaddr*)&servaddr, sizeof(servaddr));
  thread thisPacketTimer (packetTimer, frameNum.intVersion);
  thisPacketTimer.detach();
  bzero(buf, MAXLEN);
  bufferPos = 0;
  frameNum.intVersion++;
}

// delay socket send until buffer full
void pushToBuffer(char c) {
  // create header
  if (bufferPos == 0) {
    buf[bufferPos++] = SOH;
    buf[bufferPos++] = frameNum.charVersion[0];
    buf[bufferPos++] = frameNum.charVersion[1];
    buf[bufferPos++] = frameNum.charVersion[2];
    buf[bufferPos++] = frameNum.charVersion[3];
    buf[bufferPos++] = STX;
  }

  // insert data
  buf[bufferPos++] = c;

  // create footer
  if (bufferPos == FRAMESIZE - 2) {
    buf[bufferPos++] = ETX;
    buf[bufferPos++] = getChecksum(buf, 6, FRAMESIZE - 2);
    vector<char> tmpBuf;
    for (int i = 0; i < bufferPos; ++i) {
      tmpBuf.push_back(buf[i]);
    }

    bufferArchive.push_back(tmpBuf);
    forceTimeout.push_back(false);
    for (int i=0;i<FRAMESIZE;i++) {
      printf("Isi %d %d\n",i,buf[i]);
    }
    if(sendto(sockfd, buf, bufferPos, 0, (struct sockaddr*)&servaddr, sizeof(servaddr))>0){
      printf("sesuatu\n");
    }else{
      printf("seduatu\n");
    }
    printf("AKKA");
    thread thisPacketTimer (packetTimer, frameNum.intVersion);
    thisPacketTimer.detach();
    bzero(buf, MAXLEN);
    bufferPos = 0;
    frameNum.intVersion++;
  }
}

//timer countdown for each packet
void packetTimer(int thisFrameNum) {
  auto startTime = chrono::steady_clock::now();
  auto currentTime = chrono::steady_clock::now();

  while (lastACK <= thisFrameNum) {
    while (
      chrono::duration_cast<chrono::microseconds>(currentTime - startTime).count() < TIMEOUTDELAY &&
      !forceTimeout[thisFrameNum]
    ) {
      usleep(TIMEOUTDELAYSTEP);
      currentTime = chrono::steady_clock::now();
    }

    if (forceTimeout[thisFrameNum]) {
      forceTimeout[thisFrameNum] = false;
      resend(thisFrameNum);
      startTime = chrono::steady_clock::now();
      continue;
    }

    if (lastACK > thisFrameNum) {
      return;
    }

    resend(thisFrameNum);
    startTime = chrono::steady_clock::now();
  }
}

// resend packet using buffer archive
void resend(int thisFrameNum) {
  lock_guard<mutex> guard(resend_mutex);
  printf("RESEND %d\n", thisFrameNum);
  char thisBuf[bufferArchive[thisFrameNum].size()];

  for (int i = 0; i < bufferArchive[thisFrameNum].size(); ++i) {
    thisBuf[i] = bufferArchive[thisFrameNum][i];
  }
  sendto(sockfd, thisBuf, bufferArchive[thisFrameNum].size(), 0, (struct sockaddr*)&servaddr, sizeof(servaddr));
}

void listener() {
  printf("START LISTENER THREAD\n");
  while (switchFinish != FINISHED) {
    int serv_len = sizeof(servaddr);
    char thisBuf[6];

    /*Receive signal*/
    n = recvfrom(sockfd, thisBuf, 6, 0, (struct sockaddr*)&servaddr, (socklen_t*)&serv_len);

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

      if (getChecksum(thisBuf, 1, 5) == thisBuf[5]) {
        printf("ACK checksum OK\n");
        lastACK = max(lastACK, toFrame(thisBuf + 1).intVersion);
      } else {
        printf("ACK checksum FAILED\n");
      }

      break;

    case NAK:
      printf("NAK accepted\n");

      if (getChecksum(thisBuf, 1, 5) == thisBuf[5]) {
        printf("NAK checksum OK\n");
        forceTimeout[toFrame(thisBuf + 1).intVersion] = true;
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

  for (int i = start; i < end; ++i) {
    checksum ^= c[i];
  }

  return checksum;
}
