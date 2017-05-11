/*
 * serial_out.c
 *
 * Created: 3/4/2017 10:39:01 AM
 *  Author: mark
 */ 


#include "avrstuff.h"
#include "comm.h"


void prints( void(*printchar)(char) , char* c){
	
	while(*c){
		
		printchar(*c);
		c++;
	}
	
}

int main(void)
{
	int i=0;
	char buf[100];
	comm_init(9600);
	
    while(1)
    {
	
		sprintf(buf, " Hello, Sim-AVR on Windows! %d\n ", i++);
		prints(comm_putc, buf);
    }
}