/*
* File : receiver.cpp
*/

#include "dcomm.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <netdb.h>
#include <pthread.h>

/* Delay to adjust speed of consuming buffer, in milliseconds */
#define DELAY 500000

/* Define receive buffer size */
#define RXQSIZE 20

#define UPPER_LIMIT (RXQSIZE/2) - WINDOWSIZE
#define LOWER_LIMIT (UPPER_LIMIT/2)

Byte temp[FRAMESIZE];
Byte rxbuf[RXQSIZE][FRAMESIZE];
QTYPE rcvq = { 0, 0, 0, RXQSIZE, rxbuf };
QTYPE *rxq = &rcvq;
Byte sent_xonxoff = XON;
Boolean send_xon = true, send_xoff = false;

struct sockaddr_in serv_addr, cli_addr;
int cli_len = sizeof cli_addr;

char sig[2];
int byteCount = 0, consumeCount = 0;

/* Socket */
int sockfd; // listen on sock_fd

// Least idx not ACKed
int lastIdx = 0;

/* Functions declaration */
static Byte *rcvchar(int sockfd, QTYPE *queue);
static Byte *q_get(QTYPE *, Byte *);
static void *childProcess(void *);
char getChecksum(char *c, int start, int end);
bool isAck(char* c);

int main(int argc, char *argv[]) {
	Byte c;

	// printf("%s %s\n",argv[0], argv[1]);

	if (argc < 2) {
		printf("Argument kurang\n");
		return 1;
	}

	/*
	Insert code here to bind socket to the port number given in argv[1].
	*/
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	memset(rcvq.data,0,sizeof rcvq.data);
	memset((char *) &serv_addr, 0, sizeof serv_addr);
	serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); //assign server address
	serv_addr.sin_port = htons(atoi(argv[1])); //assign port number
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		printf("BIND ERROR\n");
		exit(1);
 	}
	printf("Binding pada port %d ...\n", atoi(argv[1]));

	/* Initialize XON/XOFF flags */
	sent_xonxoff = XON;

	/* Create child process */
	pthread_t child_thread;

	if(pthread_create(&child_thread, NULL, childProcess, &rcvq)){
		fprintf(stderr, "Error creating thread\n");
		return 1;
	}

	// PARENT PROCESS (Receive byte from socket)
	while (true) {
		c = *(rcvchar(sockfd, rxq));

		byteCount++;
		printf("Menerima byte ke-%d.\n", byteCount);

		/* Quit on end of file */
		if (c == Endfile) {
			printf("Exit Parent\n");
			break;
		}
	}

	// join thread
	if(pthread_join(child_thread,NULL)){
		fprintf(stderr,"Error joining thread\n");
		return 2;
	}

	return 0;
}

static Byte *rcvchar(int sockfd, QTYPE *queue)
{
	/*
	Insert code here.

	Read a character from socket and put it to the receive
	buffer.

	If the number of characters in the receive buffer is above
	certain level, then send XOFF and set a flag (why?).

	Return a pointer to the buffer where data is put.
	*/

	/* code sblom
	if (queue->count >= UPPER_LIMIT && !send_xoff) {
		printf("Buffer > minimum upperlimit\n");
		send_xon = false;
		send_xoff = true;
		sent_xonxoff = XOFF;

		sig[0] = sent_xonxoff;
		if (sendto(sockfd, sig, strlen(sig), 0,  (struct sockaddr*) &cli_addr, cli_len) > 0) {
			printf("Mengirim XOFF.\n");
		} else {
			printf("Gagal mengirim XOFF.\n");
		}
	}



	queue->data[queue->rear] = temp;
	queue->rear = ((queue->rear) + 1) % RXQSIZE;
	(queue->count)++;

	*/

	int len = recvfrom(sockfd, temp, FRAMESIZE, 0,(struct sockaddr*) &cli_addr, (socklen_t*) &cli_len);
	if (len < 0) {
		printf("Failed to read from socket\n");
	}

	// check checksum + send ACK/NAK
	if (temp[0] == SOH && temp[5] == STX && temp[len - 2] == ETX && temp[len - 1] == getChecksum(temp, 3, len - 2) && toInt(temp) < lastIdx + WINDOWSIZE) {
		//send ACK
		char sig[];
		

		if (sendto(sockfd, sig, strlen(sig), 0,  (struct sockaddr*) &cli_addr, cli_len) > 0) {
			printf("Mengirim ACK %d.\n",lastIdx);
		} else {
			printf("Gagal mengirim ACK.\n");
		}

		// add data to buffer
		queue->data[toInt(temp)%RXQSIZE] = temp; // toInt blom!!
	} else {
		//send NAK

		if (sendto(sockfd, sig, strlen(sig), 0,  (struct sockaddr*) &cli_addr, cli_len) > 0) {
			printf("Mengirim NAK.\n");
		} else {
			printf("Gagal mengirim NAK.\n");
		}
	}

	// slide sliding window
	while (isACK()) { //implement isACK <- blom!!
		queue->rear = ((queue->rear) + 1) % RXQSIZE;
		(queue->count)++;
		lastIdx++;
	}

	// check if need send XOFF
	if (queue->count >= UPPER_LIMIT && !send_xoff) {
		printf("Buffer > minimum upperlimit\n");
		send_xon = false;
		send_xoff = true;
		sent_xonxoff = XOFF;

		sig[0] = sent_xonxoff;
		if (sendto(sockfd, sig, strlen(sig), 0,  (struct sockaddr*) &cli_addr, cli_len) > 0) {
			printf("Mengirim XOFF.\n");
		} else {
			printf("Gagal mengirim XOFF.\n");
		}
	}

	return temp;
}

static void *childProcess(void * param) {

	QTYPE *rcvq_ptr = (QTYPE *)param;

	while (true) {
		/* Nothing in the queue */
		if (!queue->count) continue;

		// Retrieve data from buffer, save it to "current" and "data"
		Byte* current = &queue->data[queue->front];

		int currentFrameNumber = toFrame(current + 1).intVersion;
		if (!((currentFrameNumber < lastIdx) || (currentFrameNumber >= lastIdx && lastIdx + WINDOWSIZE <= currentFrameNumber && lastIdx + WINDOWSIZE > lastIdx)))
		{
			continue;
		}

		// consume 
		for (int i = 6; i < FRAMESIZE - 2; ++i)
		{
			if (current[i] == Endfile) {
				printf("ENDFILE\n");
				pthread_exit(0);
			}
			consumeCount++;
			printf("Mengkonsumsi byte ke-%d: '%c'.\n", consumeCount, current[i]);
			usleep(DELAY);
		}

		// Increment front index and check for wraparound.
		memset(queue->front,0,sizeof queue->front);
		queue->front = ((queue->front) + 1) % RXQSIZE;
		(queue->count)--;

		// If the number of characters in the receive buffer is below
		// certain level, then send XON.
		if (queue->count < LOWER_LIMIT && !send_xon) {
			printf("Buffer < maximum lowerlimit\n");
			send_xon = true;
			sent_xonxoff = XON;

			sig[0] = sent_xonxoff;
			// if signal need checksum add here
			if (sendto(sockfd, sig, strlen(sig), 0,  (struct sockaddr*) &cli_addr, cli_len) > 0) {
				printf("Mengirim XON.\n");
			} else {
				printf("Gagal mengirim XON.\n");
			}
		}
	}
	pthread_exit(0);
}

char getChecksum(char *c, int start, int end) {
  char checksum = 0;

  for (int i = start; i < end; ++i) {
    checksum ^= c[i];
  }

  return checksum;
}


bool isAck() {
	Byte* nextData = &queue->data[((queue->rear) + 1) % RXQSIZE];
	return nextData[0] != 0;
}

char* createACKPacket(int frameNumber) {
	char* ret;
	memset(ret,0,6*sizeof(char));
	ret[0] = ACK;
	char* frameNumberChar = toFrame(frameNumber).charVersion;
	ret[1] = frameNumberChar[0];
	ret[2] = frameNumberChar[1];
	ret[3] = frameNumberChar[2];
	ret[4] = frameNumberChar[3];
	ret[5] = getChecksum(ret + 1, 1, 5);
	return ret;
}