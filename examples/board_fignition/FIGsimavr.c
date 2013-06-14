/*
	FIGsimavr.c

	Copyright 2013 Michael Hughes <squirmyworms@embarqmail.com>

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <SDL/SDL.h>
#include <sys/time.h>

//#define USE_PTHREAD
//#define USE_VCD_FILE
//#define USE_AVR_GDB

#define INLINE 

#ifdef USE_PTHREAD
#include <pthread.h>
#endif

#include "sim_avr.h"
#include "avr_ioport.h"
#include "sim_elf.h"
#include "avr_uart.h"
#include "sim_irq.h"
#include "fifo_declare.h"
#include "avr_spi.h"
#include "avr_timer.h"

#ifdef USE_AVR_GDB
#include "sim_gdb.h"
#endif

#ifdef USE_VCD_FILE
#include "sim_vcd_file.h"
#endif

#define kScreenWidth	320
#define kScreenHeight	200

#define kVideoBuffWidth		25
#define kVideoBuffHeight	24

#define	kVideoPixelWidth	(kVideoBuffWidth<<3)
#define kVideoScanlines		(kVideoBuffHeight<<3)
#define kVideoBufferSize	((kVideoBuffWidth*kVideoScanlines)<<2)

#define kVideoPixelTop		((kScreenHeight>>1)-(kVideoScanlines>>1))
#define kVideoPixelLeft		((kScreenWidth>>1)-(kVideoPixelWidth>>1))


#define kFrequency	((uint32_t)(20UL*1000UL*1000UL))
#define	kRefresh	50
#define	kRefreshCycles	((kFrequency)/kRefresh)
#define kScanlineCycles	(kRefreshCycles/kVideoScanlines)

double dtime;

enum {
	IRQ_KBD_ROW1=0,
	IRQ_KBD_ROW2,
	IRQ_KBD_COUNT
};

typedef struct kbd_fignition_t {
	avr_irq_t*	irq;
	struct avr_t*	avr;
	char		row1_out;
	char		row2_out;
}kbd_fignition_t;

kbd_fignition_t kbd_fignition;

enum {
	IRQ_SPI_BYTE_IN=0,
	IRQ_SPI_BYTE_OUT,
	IRQ_SPI_SRAM_CS,
	IRQ_SPI_FLASH_CS,
	IRQ_SPI_COUNT
};

typedef struct spi_chip_t {
	unsigned char	state;
	unsigned char	status;
	unsigned char	command;
	unsigned short	address;
	unsigned char	data[8192];
}spi_chip_t;

typedef struct spi_fignition_t {
	avr_irq_t*		irq;
	struct avr_t*		avr;

	struct spi_chip_t*	spi_chip;

	struct spi_chip_t	sram;
	struct spi_chip_t	flash;
}spi_fignition_t;	

spi_fignition_t	spi_fignition;

enum {
	IRQ_VIDEO_UART_BYTE_IN=0,
	IRQ_VIDEO_TIMER_PWM0,
	IRQ_VIDEO_TIMER_PWM1,
	IRQ_VIDEO_TIMER_COMPB,
	IRQ_VIDEO_COUNT
};

DECLARE_FIFO(uint8_t,video_fignition_fifo,(1<<16));

typedef struct video_fignition_t {
	avr_irq_t*		irq;
	struct avr_t*		avr;
	SDL_Surface*		surface;

	struct video_buffer_t {
		uint8_t			data[kVideoBufferSize];
		uint8_t			x;
		uint8_t			y;
	}buffer;
	int			needRefresh;
	uint32_t		frame;
}video_fignition_t;


avr_t*		avr;
int		state;

#ifdef USE_VCD_FILE
avr_vcd_t	vcd_file;
#endif

video_fignition_t video_fignition;

#if 0
static inline uint64_t get_cycles()
{
    uint64_t n;
    __asm__ __volatile__ ("rdtsc" : "=A"(n));
    return n;
}
static inline uint64_t get_dtime(void) { return(get_cycles()); }
#else
static INLINE uint64_t get_dtime(void) {
	struct timeval	t;
	uint64_t	dsec;
	uint64_t	dusec;

	gettimeofday(&t, (struct timezone *)NULL);

	dsec=t.tv_sec;
	dusec=t.tv_usec;

	return(((dsec*1000*1000)+dusec));
//	return(dusec);
}
#endif

static inline void _PutBWPixel(int x, int y, unsigned long pixel, SDL_Surface* surface) {
	int	bpp=surface->format->BytesPerPixel;
	unsigned char *dst;

	x=(x>(kScreenWidth-1)?x-kScreenWidth:x);
	y=(y>(kScreenHeight-1)?y-kScreenHeight:y);

	dst=(unsigned char *)surface->pixels+y*surface->pitch+x*bpp;
	pixel=(pixel?0xffffffff:0x00000000);

	switch(bpp) {
		case	1:
			*dst=pixel;
			break;
		case	2:
			*(unsigned short *)dst=pixel;
			break;
		case	3:
			dst[0]=pixel;
			dst[1]=pixel;
			dst[2]=pixel;
			break;
		case	4:
			*(unsigned long *)dst=pixel;
			break;
	}
}


static inline void PutBWPixel(int x, int y, unsigned long pixel, SDL_Surface* surface) {
	int px, py;

	px=kVideoPixelLeft+x;
//	px=x<<2;
	py=kVideoPixelTop+y;
//	py=(y<<1)+y;
//	py=(y<<2);

	_PutBWPixel(px,py,pixel,surface);
#if 0
	_PutBWPixel(px+1,py,pixel,surface);
	_PutBWPixel(px+2,py,pixel,surface);
	_PutBWPixel(px+3,py,pixel,surface);

	py++;

	_PutBWPixel(px,py,pixel,surface);
	_PutBWPixel(px+1,py,pixel,surface);
	_PutBWPixel(px+2,py,pixel,surface);
	_PutBWPixel(px+3,py,pixel,surface);

	py++;

	_PutBWPixel(px,py,pixel,surface);
	_PutBWPixel(px+1,py,pixel,surface);
	_PutBWPixel(px+2,py,pixel,surface);
	_PutBWPixel(px+3,py,pixel,surface);

	py++;

	_PutBWPixel(px,py,pixel,surface);
	_PutBWPixel(px+1,py,pixel,surface);
	_PutBWPixel(px+2,py,pixel,surface);
	_PutBWPixel(px+3,py,pixel,surface);
#endif
}

static void video_fignition_uart_byte_in_hook(struct avr_irq_t* irq, uint32_t value, void* param) {
	video_fignition_t*	p=(video_fignition_t*)param;

	p->buffer.data[(p->buffer.y<<5)+p->buffer.x]=(0xff&value);

	p->buffer.x++;	

	if(kVideoBuffWidth<=p->buffer.x) {
		p->buffer.x=0;
		p->buffer.y++;
		if(kVideoScanlines<p->buffer.y) {
			p->buffer.y=0;
			p->needRefresh=1;
		}
	}
}

static void video_fignition_sync_in_hook(struct avr_irq_t* irq, uint32_t value, void* param) {
	video_fignition_t*	p=(video_fignition_t*)param;
	static	long		syncCache;

	if(00==value)
		syncCache=0;
	else
		syncCache=syncCache<<8|(value&0xff);

	printf("[video_fignition_sync_in_hook] %d %04x\n", value, syncCache);

//	if(0x0b==value) {
//	if(0x000B0549==(syncCache&0x00ffffff)) {
//		p->scanLine=0;
//		p->scanRow=0;
//		p->refreshNeeded=1;
//	}
}

static void VideoScan(struct video_fignition_t* p) {
	uint8_t		*pp, pd;
	uint8_t		px,py;

	SDL_LockSurface(p->surface);

	for(py=0; py<=kVideoScanlines; py++) {
		PutBWPixel(-(kVideoPixelLeft-2), py, p->frame&0x01,p->surface);

		pp=&p->buffer.data[py<<5];

		for(px=0; px<=kVideoPixelWidth;) {	

			pd=*pp++;
			
			PutBWPixel(px++, py, pd&0x80,p->surface); pd<<=1;
			PutBWPixel(px++, py, pd&0x80,p->surface); pd<<=1;
			PutBWPixel(px++, py, pd&0x80,p->surface); pd<<=1;
			PutBWPixel(px++, py, pd&0x80,p->surface); pd<<=1;
			PutBWPixel(px++, py, pd&0x80,p->surface); pd<<=1;
			PutBWPixel(px++, py, pd&0x80,p->surface); pd<<=1;
			PutBWPixel(px++, py, pd&0x80,p->surface); pd<<=1;
			PutBWPixel(px++, py, pd&0x80,p->surface); pd<<=1;
		}
	}

	SDL_UnlockSurface(p->surface);
	p->needRefresh=0;
	p->frame++;
}

void video_fignition_connect(video_fignition_t* p, char uart) {
	uint32_t	f=0;
	avr_ioctl(p->avr,AVR_IOCTL_UART_GET_FLAGS(uart), &f);
	f&=~AVR_UART_FLAG_STDIO;
	avr_ioctl(p->avr,AVR_IOCTL_UART_SET_FLAGS(uart), &f);
	
	avr_connect_irq(avr_io_getirq(p->avr, AVR_IOCTL_UART_GETIRQ(uart), UART_IRQ_OUTPUT), p->irq+IRQ_VIDEO_UART_BYTE_IN);

	avr_connect_irq(avr_io_getirq(p->avr, AVR_IOCTL_TIMER_GETIRQ('2'), TIMER_IRQ_OUT_PWM0), p->irq+IRQ_VIDEO_TIMER_PWM0);
	avr_connect_irq(avr_io_getirq(p->avr, AVR_IOCTL_TIMER_GETIRQ('2'), TIMER_IRQ_OUT_PWM1), p->irq+IRQ_VIDEO_TIMER_PWM1);
//	avr_connect_irq(avr_io_getirq(p->avr, AVR_IOCTL_TIMER_GETIRQ('2'), TIMER_IRQ_OUT_COMP), p->irq+IRQ_VIDEO_TIMER_COMP);
//	avr_connect_irq(avr_io_getirq(p->avr, AVR_IOCTL_TIMER_GETIRQ('2'), TIMER_IRQ_OUT_COMP+AVR_TIMER_COMPA), p->irq+IRQ_UART_SYNC_IN);
//	avr_connect_irq(avr_io_getirq(p->avr, AVR_IOCTL_TIMER_GETIRQ('2'), TIMER_IRQ_OUT_COMP+AVR_TIMER_COMPB), p->irq+IRQ_VIDEO_TIMER_COMPB);
//	avr_connect_irq(avr_io_getirq(p->avr, AVR_IOCTL_TIMER_GETIRQ('2'), TIMER_IRQ_OUT_COMP+AVR_TIMER_COMPC), p->irq+IRQ_UART_SYNC_IN);
//	avr_connect_irq(avr_io_getirq(p->avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 3), p->irq+IRQ_UART_SYNC_IN);
}

static const char* uart_irq_names[IRQ_VIDEO_COUNT]={
	[IRQ_VIDEO_UART_BYTE_IN]="8<video_fignition.in",
	[IRQ_VIDEO_TIMER_PWM0]="8<timer.pwm0.sync.in",
	[IRQ_VIDEO_TIMER_PWM1]="8<timer.pwm1.sync.in",
	[IRQ_VIDEO_TIMER_COMPB]="1<timer.compb.sync.in"
};

void video_fignition_init(struct avr_t* avr, video_fignition_t* p, SDL_Surface* surface) {
	p->avr=avr;
	p->irq=avr_alloc_irq(&avr->irq_pool, 0, IRQ_VIDEO_COUNT, uart_irq_names);
	avr_irq_register_notify(p->irq+IRQ_VIDEO_UART_BYTE_IN, video_fignition_uart_byte_in_hook, p);
	avr_irq_register_notify(p->irq+IRQ_VIDEO_TIMER_PWM0, video_fignition_sync_in_hook, p);
	avr_irq_register_notify(p->irq+IRQ_VIDEO_TIMER_PWM1, video_fignition_sync_in_hook, p);
	avr_irq_register_notify(p->irq+IRQ_VIDEO_TIMER_COMPB, video_fignition_sync_in_hook, p);
	

	p->surface=surface;
	p->buffer.x=0;
	p->buffer.y=0;
	p->needRefresh=1;
}

#define SRAM_WRSR_CMD	0x01

static uint32_t spi_sram_proc(spi_fignition_t* p, uint32_t value) {
	spi_chip_t*	sram=&p->sram;

	printf("[spi_sram_proc] -- state:%02x, command:%02x value:%02x\n", sram->state, sram->command, value);

	switch(sram->command) {
		case	SRAM_WRSR_CMD:
			printf("[spi_sram_proc] -- WRSR:%02x (%02x)\n", value, sram->status);
			sram->status=value;
			sram->command=0;
		default:
			sram->command=value;
	}

	return(0);
}

static uint32_t spi_flash_proc(spi_fignition_t* p, uint32_t value) {
	return(0);
}

void spi_output_hook(struct avr_irq_t* irq, uint32_t value, void* param) {
	uint32_t		in=value;
	spi_fignition_t*	p=(spi_fignition_t*)param;

	avr_irq_t*	spi_in_irq=avr_io_getirq(p->avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_INPUT);

	value=0;
	if(&p->sram==p->spi_chip)
		value=spi_sram_proc(p, in);
	else if(&p->flash==p->spi_chip)
		value=spi_flash_proc(p, in);

	printf("[spi_output_hook] -- out:%02x in:%02x\n", in, value);

	avr_raise_irq(spi_in_irq, value);
}

void spi_sram_cs_hook(struct avr_irq_t* irq, uint32_t value, void* param) {
	spi_fignition_t*	p=(spi_fignition_t*)param;

	printf("[spi_sram_cs_hook] -- value:%02x\n", value);

	if(value) {
		p->spi_chip=&p->sram;
	} else {
		p->spi_chip=NULL;
	}
}

void spi_flash_cs_hook(struct avr_irq_t* irq, uint32_t value, void* param) {
	spi_fignition_t*	p=(spi_fignition_t*)param;

	printf("[spi_flash_cs_hook] -- value:%02x\n", value);

	if(value) {
		p->spi_chip=&p->flash;
	} else {
		p->spi_chip=NULL;
	}
}

void fig_portb_hook(struct avr_irq_t* irq, uint32_t value, void* param) {
	spi_fignition_t*	p=(spi_fignition_t*)param;

	printf("[fig_portb_hook] -- value:%02x\n", value);
	p->avr->state=cpu_Stopped;
}

static const char* spi_irq_names[IRQ_SPI_COUNT]={
	[IRQ_SPI_BYTE_IN]="8<spi_fignition.in",
	[IRQ_SPI_BYTE_OUT]="8>spi_fignition.out",
	[IRQ_SPI_SRAM_CS]="=sram.cs",
	[IRQ_SPI_FLASH_CS]="=flash.cs",
};

void spi_fignition_init(struct avr_t* avr, spi_fignition_t* p) {
	p->avr=avr;
	p->irq=avr_alloc_irq(&avr->irq_pool, 0, IRQ_SPI_COUNT, spi_irq_names);
	avr_irq_register_notify(p->irq+IRQ_SPI_BYTE_IN, spi_output_hook, p);
	avr_irq_register_notify(p->irq+IRQ_SPI_SRAM_CS, spi_sram_cs_hook, p);
	avr_irq_register_notify(p->irq+IRQ_SPI_FLASH_CS, spi_flash_cs_hook, p);

	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_OUTPUT), p->irq+IRQ_SPI_BYTE_IN);
	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), IOPORT_IRQ_PIN2), p->irq+IRQ_SPI_SRAM_CS);
	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), IOPORT_IRQ_PIN3), p->irq+IRQ_SPI_FLASH_CS);

	p->spi_chip=NULL;
	p->sram.state=0;
	p->sram.status=0;
	p->sram.command=0;
}

void kbd_row1_hook(struct avr_irq_t* irq, uint32_t value, void* param) {
	kbd_fignition_t*	p=(kbd_fignition_t *)param;

	printf("[kbd_row1_hook] -- value:%02x\n", value);

//	avr_raise_irq(avr_io_getirq(p->avr, AVR_IOCTL_IOPORT_GETIRQ('C'), IOPORT_IRQ_PIN_ALL), value);

//	if(0x41==spi_fignition.sram.status)
//		avr->state=cpu_Done;
}

void kbd_row2_hook(struct avr_irq_t* irq, uint32_t value, void* param) {
	kbd_fignition_t*	p=(kbd_fignition_t*)param;

	printf("[kbd_row2_hook] -- value:%02x\n", value);

//	avr_raise_irq(avr_io_getirq(p->avr, AVR_IOCTL_IOPORT_GETIRQ('C'), IOPORT_IRQ_PIN_ALL), value);
}

static const char* kbd_irq_names[IRQ_KBD_COUNT]={
	[IRQ_KBD_ROW1]="1>kbd_row1.out",
	[IRQ_KBD_ROW2]="1>kbd_row2.out",
};

void kbd_fignition_init(struct avr_t* avr, kbd_fignition_t* p) {
	p->avr=avr;
	p->irq=avr_alloc_irq(&avr->irq_pool, 0, IRQ_KBD_COUNT, kbd_irq_names);

	avr_irq_register_notify(p->irq+IRQ_KBD_ROW1, kbd_row1_hook, p);
	avr_irq_register_notify(p->irq+IRQ_KBD_ROW2, kbd_row2_hook, p);

	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), IOPORT_IRQ_PIN7), p->irq+IRQ_KBD_ROW1);
	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), IOPORT_IRQ_PIN0), p->irq+IRQ_KBD_ROW2);

	p->row1_out=0;
	p->row2_out=0;
}

void SDLInit(int argc, char* argv[], SDL_Surface** surface) {
	SDL_Init(SDL_INIT_VIDEO);

//	*surface=SDL_SetVideoMode(kScreenWidth, kScreenHeight, 16, SDL_FULLSCREEN|SDL_DOUBLEBUF|SDL_HWSURFACE);
	*surface=SDL_SetVideoMode(kScreenWidth, kScreenHeight, 16, SDL_SWSURFACE);
	if(*surface==NULL)
		exit(0);

	SDL_EnableKeyRepeat(125, 50);
}

typedef struct fignition_thread_t {
	avr_t*			avr;
	pthread_t		thread;
	avr_cycle_count_t	run_cycles;
	uint64_t		elapsed_dtime;
}fignition_thread_t;

fignition_thread_t fig_thread;

void* avr_run_thread(void* param) {
	fignition_thread_t*	p=(fignition_thread_t*)param;
	avr_t*			avr=p->avr;
	uint64_t		prev_dtime, now_dtime;
	avr_cycle_count_t	run_cycles=p->run_cycles;
	avr_cycle_count_t	last_cycle=avr->cycle+run_cycles;

#ifdef USE_PTHREAD
thread_loop:
#endif
	prev_dtime=get_dtime();

	while(last_cycle>avr->cycle) {
		avr_run(avr);
	}

	now_dtime=get_dtime();

	if(now_dtime>prev_dtime)
		p->elapsed_dtime+=now_dtime-prev_dtime;
	else
		p->elapsed_dtime+=prev_dtime-now_dtime;

#ifdef USE_PTHREAD
	last_cycle+=run_cycles;
	goto thread_loop;
#endif

	return(0);
}

char kbd_unescape(char scancode) {
	switch(scancode) {
		case	11:	scancode=11;	break;	// up
		case	10:	scancode=10;	break;	// down
		case	21:	scancode=9;	break;	// right
//  Are these correct???
		case	8:	scancode=7;	break;	// left (swapped with BS)
		case	127:	scancode=8;	break;
	}
	return(scancode);
}

char kbd_fmap[256];

char kbd_figgicode(char scancode) {
	if(scancode>0x7F)
		return(0xFF);

	return(kbd_fmap[scancode]);
}

static void fig_callback_sleep_override(avr_t * avr, avr_cycle_count_t howLong) {
}

void catch_sig(int sign)
{
	printf("\n\n\n\nsignal caught, simavr terminating\n\n");

	avr->state=cpu_Done;

	if (avr)
		avr_terminate(avr);

	exit(0);
}

extern void avr_core_run_many(avr_t* avr);

int main(int argc, char *argv[])
{
	elf_firmware_t	f;
	const char*	fname="FIGnitionPAL.elf";

	uint32_t	clock;
	uint16_t	scancode;
	uint32_t	nextRefresh;

	SDL_Surface* surface;
	SDL_Event event;

	signal(SIGINT, catch_sig);
	signal(SIGTERM, catch_sig);

	elf_read_firmware(fname, &f);
	
	strcpy(f.mmcu, "atmega168");
	f.frequency=kFrequency;

	printf("firmware %s f=%d mmcu=%s\n", fname, (int)f.frequency, f.mmcu);

	avr=NULL;	
	avr = avr_make_mcu_by_name(f.mmcu);
	if (!avr) {
		fprintf(stderr, "%s: AVR '%s' not known\n", argv[0], f.mmcu);
		exit(1);
	}
	avr_init(avr);
	avr_load_firmware(avr, &f);

	avr->cycle=0ULL;

	avr->run=avr_core_run_many;
	avr->sleep=fig_callback_sleep_override;
//	avr->log=LOG_TRACE;

#ifdef USE_AVR_GDB
	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	avr->state = cpu_Stopped;
	avr_gdb_init(avr);
#endif

	SDLInit(argc, argv, &surface);

	video_fignition_init(avr, &video_fignition, surface);
	video_fignition_connect(&video_fignition, '0');

	spi_fignition_init(avr, &spi_fignition);
	kbd_fignition_init(avr, &kbd_fignition);

#ifdef USE_VCD_FILE
	avr_vcd_init(avr, "gtkwave_output.vcd", &vcd_file, 100000);
	avr_vcd_add_signal(&vcd_file, avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), IOPORT_IRQ_PIN_ALL), 8, "uart");
//	avr_vcd_add_signal(&vcd_file, avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), IOPORT_IRQ_PIN5), 1, "oc2b");

	avr_vcd_start(&vcd_file);
#endif

	state=cpu_Running;

	fig_thread.avr=avr;
	fig_thread.run_cycles=kRefreshCycles;
	fig_thread.elapsed_dtime=0ULL;

#ifdef USE_PTHREAD
	pthread_create(&fig_thread.thread, NULL, avr_run_thread, &fig_thread);
	printf("main running");
#endif

	nextRefresh=clock=0;

	while((state!=cpu_Done)&&(state!=cpu_Crashed)) {
		clock++;
#ifndef USE_PTHREAD
		avr_run_thread(&fig_thread);
#endif

		if((nextRefresh<clock) || video_fignition.needRefresh) {
			uint64_t eacdt=(1000*fig_thread.elapsed_dtime)/(1+avr->cycle);
			printf("[avr_run_thread] - cycle: %016llu ecdt: %016llu eacdt: %016llu 1/eacdt: %08.3f\n", 
				avr->cycle, fig_thread.elapsed_dtime, eacdt, ((float)1/eacdt));

			VideoScan(&video_fignition);
			SDL_Flip(surface);
#ifndef USE_PTHREAD
			nextRefresh+=30;
#else
			nextRefresh=fig_thread.runCycles;
#endif
		}

		SDL_PollEvent(&event);
		switch (event.type) {
			case	SDL_QUIT:
				state=cpu_Done;
				break;
			case	SDL_KEYDOWN:
				scancode=event.key.keysym.scancode;
				if(0x1b==scancode)
					state=cpu_Done;

				scancode=kbd_unescape(scancode);
				scancode=kbd_figgicode(scancode);
				break;

		}
#ifdef	USE_PTHREAD
		usleep(5);
#endif
	}

#ifdef USE_VCD_FILE
	avr_vcd_close(&vcd_file);
#endif

	avr_terminate(avr);
	return(0);
}

