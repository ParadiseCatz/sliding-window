/*
* File : dcomm.h
*/
#ifndef _DCOMM_H_
#define _DCOMM_H_

/* ASCII Const */
#define SOH 1 /* Start of Header Character */
#define STX 2 /* Start of Text Character */
#define ETX 3 /* End of Text Character */
#define ENQ 5 /* Enquiry Character */
#define ACK 6 /* Acknowledgement */
#define BEL 7 /* Message Error Warning */
#define CR 13 /* Carriage Return */
#define LF 10 /* Line Feed */
#define NAK 21 /* Negative Acknowledgement */
#define Endfile 26 /* End of file character */
#define ESC 27 /* ESC key */

/* XON/XOFF protocol */
#define XON (0x11)
#define XOFF (0x13)

/* Const */
#define BYTESIZE 256 /* The maximum value of a byte */
#define MAXLEN 1024 /* Maximum messages length */
#define FRAMESIZE 16 /* Maximum frame size */
#define WINDOWSIZE 4 /* Window size for sliding window */

typedef bool Boolean;

typedef char Byte;

union Frame {
  int intVersion;
  char charVersion[4];
};

typedef struct QTYPE
{
	unsigned int count;
	unsigned int front;
	unsigned int rear;
	unsigned int maxsize;
	Byte *data;
} QTYPE;

typedef struct MESGB
{
	Byte checksum;
	Byte msgno;
	Byte *data;
} MESGB;


Frame toFrame(char* c) {
  Frame ret;
  ret.charVersion[0] = c[0];
  ret.charVersion[1] = c[1];
  ret.charVersion[2] = c[2];
  ret.charVersion[3] = c[3];
  return ret;
}

Frame toFrame(int intVersion) {
  Frame ret;
  ret.intVersion = intVersion;
  return ret;
}

int toInt(char* c) {
  Frame ret;
  ret.charVersion[0] = c[1];
  ret.charVersion[0] = c[2];
  ret.charVersion[0] = c[3];
  ret.charVersion[0] = c[4];

  return ret.intVersion;

}

bool isAck(char* c) {
  

}


#endif
