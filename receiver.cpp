/*
* File : receiver.cpp
*/

#include "dcomm.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <netdb.h>
#include <pthread.h>

/* Delay to adjust speed of consuming buffer, in milliseconds */
#define DELAY 500

/* Define receive buffer size */
#define RXQSIZE 8

#define UPPER_LIMIT RXQSIZE
#define LOWER_LIMIT (RXQSIZE/2)

Byte rxbuf[RXQSIZE];
QTYPE rcvq = { 0, 0, 0, RXQSIZE, rxbuf };
QTYPE *rxq = &rcvq;
Byte sent_xonxoff = XON;
Boolean send_xon = false, send_xoff = false;

struct sockaddr_in serv_addr, cli_addr;
int cli_len = sizeof cli_addr;

char sig[2];

/* Socket */
int sockfd; // listen on sock_fd

/* Functions declaration */
static Byte *rcvchar(int sockfd, QTYPE *queue);

static Byte *q_get(QTYPE *, Byte *);

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

	int byteCount = 0, consumeCount = 0;
	/* Create child process */
	if (fork()) { 	/*** IF PARENT PROCESS ***/
		// printf("Adsdfas\n");
		while (true) {
			c = *(rcvchar(sockfd, rxq));

			byteCount++;
			printf("Menerima byte ke-%d.\n", byteCount);
			/* Quit on end of file */
			if (c == Endfile) {
				exit(0);
			}
		}
	} else { 		/*** IF CHILD PROCESS ***/
		// printf("LOL\n");
		while (true) {
			/* Call q_get */
			Byte* data;
			Byte* ret = q_get(rxq, data);
			if (ret) {
				if (*data == Endfile) {
					printf("ENDFILE\n");
					exit(0);
				}
				printf("Mengkonsumsi byte ke-%d: '%c'.\n", consumeCount, data);
				consumeCount++;
			}

			/* Can introduce some delay here. */
			// sleep(DELAY);
		}
	}
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

	Byte temp[1];
	if (recvfrom(sockfd, temp, 1, 0,(struct sockaddr*) &cli_addr, (socklen_t*) &cli_len) < 0) {
		printf("Failed to read from socket\n");
	}

	queue->data[queue->rear] = temp[0];
	queue->rear = ((queue->rear) + 1) % RXQSIZE;
	(queue->count)++;

	return queue->data;
}

/* q_get returns a pointer to the buffer where data is read or NULL if
* buffer is empty.
*/
static Byte *q_get(QTYPE *queue, Byte *data)
{
	Byte *current;

	/* Nothing in the queue */
	if (!queue->count) return (NULL);
	printf("wewew\n");
	/*
	Insert code here.

	Retrieve data from buffer, save it to "current" and "data"

	If the number of characters in the receive buffer is below
	certain level, then send XON.

	Increment front index and check for wraparound.
	*/

	current = &queue->data[queue->front];
	data = current;

	(queue->count)--;

	if (queue->count < LOWER_LIMIT && !send_xon) {
		printf("Buffer < maximum lowerlimit\n");
		send_xon = true;
		send_xoff = false;
		sent_xonxoff = XON;

		sig[0] = sent_xonxoff;
		if (sendto(sockfd, sig, strlen(sig), 0,  (struct sockaddr*) &cli_addr, cli_len) > 0) {
			printf("Mengirim XON.\n");
		} else {
			printf("Gagal mengirim XON.\n");
		}
	}

	queue->front = ((queue->front) + 1) % RXQSIZE;

	return current;
}