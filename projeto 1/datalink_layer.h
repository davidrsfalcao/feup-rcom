#ifndef DATALINK_LAYER_H
#define DATALINK_LAYER_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1
#define FLAG 0x7E
#define ADDR 0x03
#define CW 0x03
#define CR 0x07
#define BCCW 0x00
#define BCCR 0x04
#define S0 0
#define S1 1
#define S2 2
#define S3 3
#define S4 4


void alarm_handler();
void stateMachine(unsigned char c, int* state,unsigned char* trama);
int set_writer(int* fd);
int set_reader(int* fd);
int LLOPEN(int* fd, char* mode);


#endif
