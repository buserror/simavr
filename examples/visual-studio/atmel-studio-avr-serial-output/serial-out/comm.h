/*
 * COMM.h
 *
 * Created: 7/14/2014 7:54:12 PM
 *  Author: mark
 */ 


#ifndef COMM_H_
#define COMM_H_


void comm_init(uint16_t baud);
void comm_init_115200();

void comm_putc(unsigned char c);
unsigned char comm_getc();
unsigned char comm_kbhit();




#endif /* COMM_H_ */