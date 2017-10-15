/*Non-Canonical Input Processing*/
#include "datalink_layer.h"

volatile int STOP=FALSE;
unsigned char flag_attempts=1;
unsigned char flag_alarm=1;
unsigned char flag_error=0;

struct termios oldtio,newtio;



void alarm_handler(){
	flag_attempts++;
	if(flag_attempts <4){
		printf("RETRY: %u\n", flag_attempts);
	}
  if(flag_attempts == 3){
    flag_error = 1;
  }
	flag_alarm=1;
}



void stateMachine(unsigned char c, int* state, unsigned char* trama){

	switch(*state){
		case S0:
			if(c == FLAG){
				*state = S1;
			}
			break;
		case S1:
			if(c != FLAG){
				*state = S2;
				trama[0] = c;
			}
			break;
		case S2:
			if(c != FLAG){
				*state = S3;
				trama[1] = c;
			}
			break;
		case S3:
			if(c != FLAG){
				trama[2] = c;
				if((trama[0]^trama[1]) != trama[2]){

					*state = S0;
				}
				else{
					*state=S4;
				}

			}
			break;
		case S4:
			if(c == FLAG){
				STOP = TRUE;
        alarm(0);
				flag_alarm=0;
			}
			else{
				*state = S0;
			}
			break;


	}
}


int set_writer(int* fd){

  unsigned char SET[5] = {FLAG, ADDR, CW, BCCW, FLAG};
  unsigned char elem;
	int res;
  unsigned char trama[3];
  int state=0;
	(void) signal(SIGALRM, alarm_handler);
  while(flag_attempts < 4 && flag_alarm == 1){
      res = write(*fd,SET,5);
      printf("%d bytes written\n", res);

      alarm(3);
      flag_alarm=0;

    // Wait for UA signal.

      while(STOP == FALSE && flag_alarm == 0){
					res = read(*fd,&elem,1);
       		if(res >0) {
          		stateMachine(elem, &state, trama);
       		}
      }
  }

  if(flag_error == 1){
     printf("Can't connect to the reader\n");
     return FALSE;
  }
  else{
    printf("%u%u%u\n",trama[0],trama[1],trama[2]);
    return TRUE;
  }

}

int set_reader(int* fd){

  unsigned char UA[5] = {FLAG, ADDR, CR, BCCR, FLAG};
  char elem;
	int res;
  unsigned char trama[3];
  int state=0;
  while (STOP==FALSE) {       /* loop for input */
      res = read(*fd,&elem,1);

      if(res>0){
        stateMachine(elem, &state, trama);
      }
    }
  printf("%u%u%u\n",trama[0],trama[1],trama[2]);

	res = write(*fd,UA,5);
	sleep(1);

	printf("%d bytes written\n", res);

	return TRUE;
}


/* SET Serial Port Initilizations */

void set_serial_port(char* port, int* fd){

	int c;
    int i, sum = 0, speed = 0;

    /*
      Open serial port device for reading and writing and not as controlling tty
      because we don't want to get killed if linenoise sends CTRL-C.
    */

    *fd = open(port, O_RDWR | O_NOCTTY );

	if (*fd <0) {perror(port); exit(-1); }

    if (tcgetattr(*fd,&oldtio) == -1) { /* save current port settings */
      perror("tcgetattr");
      exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 1;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 0;   /* blocking read until 5 chars received */


    /*
      VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
      leitura do(s) próximo(s) caracter(es)
    */

    tcflush(*fd, TCIOFLUSH);

    if ( tcsetattr(*fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");

}


int close_serial_port(int* fd){

	if ( tcsetattr(*fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    close(*fd);
    return 0;
}


int LLOPEN(char* port, char* mode){

  int fd;
  int result;
  set_serial_port(port, &fd);

  if(strcmp(mode,"r") == 0){
    result = set_reader(&fd);

  }
  else if(strcmp(mode,"w") == 0){
    result = set_writer(&fd);
  }

  if(result  == TRUE){
	return fd;
  }
  else{
		LLCLOSE(&fd);
	return -1;
  }

}


int send_package(int* fd, unsigned char* msg, int* length){
	int i=0;
	unsigned char bcc2 = 0x00;
	msg = (unsigned char *) realloc(msg, *length+1);
	for(i; i<*length; i++){
		bcc2 ^=msg[i];
	}
	msg[*length] = bcc2;
	*length = *length+1;
	unsigned char* stuffed_message = byte_stuffing(msg, length);
	unsigned char* full_message = add_control_message(stuffed_message, length);

	return LLWRITE(fd,full_message,*length);
}


/*int send_rr(int* fd, unsigned char N){
	unsigned char* rr[] = {FLAG, ADDR, N^1, ADDR^(N^1), FLAG};
	LLWRITE(fd, rr, 5);
}*/



int get_package(int* fd, unsigned char* msg){

	int length = LLREAD(fd, msg);
	unsigned char* control_message = verify_rmsg_connection(msg, &length);
	if(control_message == NULL)
		return -1;
	unsigned char* data_message = verify_bcc2(control_message, &length);
	msg = (unsigned char*) realloc(msg, length);
	memcpy(msg, data_message, length);
	return length;
}

unsigned char* verify_bcc2(unsigned char* control_message, int* length){
	unsigned char* destuffed_message = byte_destuffing(control_message, length);
	int i=0;
	unsigned char control_bcc2 = 0x00;
	for(i; i<*length-1; i++){
		control_bcc2 ^= destuffed_message[i];
	}
	if(control_bcc2 != destuffed_message[*length-1])
		return NULL;

	i=0;
	unsigned char* data_message = (unsigned char*) malloc(*length-1);
	for(i; i<*length-1; i++){
			data_message[i] = destuffed_message[i];
	}
	*length = *length-1;
	free(destuffed_message);
	return data_message;
}



unsigned char* verify_rmsg_connection(unsigned char* msg, int* length){

	if((msg[1]^msg[2]) != msg[3]){
		return NULL;
	}

	unsigned char* control_message = (unsigned char*) malloc(*length-5);
	int i=4;
	int j=0;
	for(i; i<*length-1; i++, j++){

		control_message[j] = msg[i];
	}
	free(msg);
	*length = *length-5;
	return control_message;
}


unsigned char* add_control_message(unsigned char* msg, int* length){
	unsigned char* full_message = (unsigned char*) malloc(*length+5);
	*length = *length+5;
	int i=0;
	full_message[0] = FLAG;
	full_message[1] = 0x03;
	full_message[2] = 0x00;
	full_message[3] = full_message[1]^full_message[2];
	for(i; i<*length; i++){
		full_message[i+4] = msg[i];
	}
	full_message[*length-1] = FLAG;
	free(msg);
	return full_message;

}

/*int send_message(int *fd, char* msg, int length){
		int i=0;
		unsigned char* bcc2 = (unsigned char*);
		char* final_msg;
		for(i; i<length; i++){
			bcc2 ^=msg[i];
		}
		msg = (char *) realloc(msg, strlen(msg)+sizeof(int));
		strcat(msg, bcc2);
		byte_stuffing(msg);
		final_msg = (char *) malloc(strlen(msg)+4);
		/*strcat(final_msg, 0x)
		LLWRITE(fd, msg)

}*/

unsigned char* byte_stuffing(unsigned char* msg, int* length){
	unsigned char* str;
	int i=0;
	int j=0;
	int new_length = *length;
	str = (unsigned char *) malloc(*length);

	for(i; i < *length; i++, j++){
		if(msg[i] ==  0x7e){
			str = (unsigned char *) realloc(str, new_length+1);
			str[j] = 0x7d;
			str[j+1] = 0x5e;
			new_length++;
			j++;
		}
		else if(msg[i] == 0x7d){
			str = (unsigned char *) realloc(str, new_length+1);
			str[j] = 0x7d;
			str[j+1]= 0x5d;
			new_length++;
			j++;
		}
		else{
			str[j] = msg[i];
		}
	}

	*length = new_length;
	free(msg);
	return str;
}

unsigned char* byte_destuffing(unsigned char* msg, int* length){
	unsigned char* str;
	int i=0;
	int new_length = 0;

	for(i; i<*length; i++){
		new_length++;
		str = (unsigned char *) realloc(str, new_length);
		if(msg[i] == 0x7d){
			if(msg[i+1] == 0x5e){
				str[new_length-1] = 0x7e;
				i++;
			}
	 		else if(msg[i+1] == 0x5d){
				str[new_length -1] = 0x7d;
				i++;
			}
		}
		else{
			str[new_length-1] = msg[i];
		}

	}
	*length = new_length;
	return str;
}


int LLWRITE(int* fd, char* msg, int length){
	int i=0;
	for(i=0;i<length; i++){
		printf("Valor: %x\n", msg[i]);
	}
	int res = write(*fd, msg, length);
	sleep(1);
	if(res>0 && res == length){
		return TRUE;
	}
	else{
		return FALSE;
	}
}

int get_readed_values(unsigned char elem, int* state){

	switch (*state) {
		case S0:
			if(elem == FLAG){
				*state = S1;
			}
			break;
		case S1:
			if(elem == FLAG){
				STOP = TRUE;
			}
			break;
	}
}



int LLREAD(int* fd,unsigned char* msg){

	unsigned char elem;
	int state = S0;
	int res;
	int msg_length=0;
	STOP = FALSE;
	while (STOP==FALSE) {       /* loop for input */
			res = read(*fd,&elem,1);
			if(res>0){
				get_readed_values(elem, &state);
				msg_length++;
				msg = realloc(msg, msg_length);
				msg[msg_length-1] = elem;
			}
		}
		/*int i=0;
		for(i; i<msg_length; i++){
			printf("READ: %x\n",msg[i]);
		}*/
		return msg_length;
}

void LLCLOSE(int* fd){
	close_serial_port(fd);
}
