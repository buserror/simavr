enum usb_regs
{
	usbcon = 0,
	udcon = 8,
	udint = 9,
	udien = 10,
	udaddr = 11,
	udfnuml = 12,
	udfnumh = 13,
	udmfn = 14,
//     _res=15,
	ueintx = 16,
	uenum = 17,
	uerst = 18,
	ueconx = 19,
	uecfg0x = 20,
	uecfg1x = 21,
	uesta0x = 22,
	uesta1x = 23,
	ueienx = 24,
	uedatx = 25,
	uebclx = 26,
//     _res2=27,
	ueint = 28,
	otgtcon = 29,
};

union _ueintx {
	struct {
		uint8_t txini :1;
		uint8_t stalledi :1;
		uint8_t rxouti :1;
		uint8_t rxstpi :1;
		uint8_t nakouti :1;
		uint8_t rwal :1;
		uint8_t nakini :1;
		uint8_t fifocon :1;
	};
	uint8_t v;
};

struct _epstate {
	union _ueintx ueintx;
	uint8_t dummy1;
	uint8_t dummy2;
	union {
		struct {
			uint8_t epen :1;
			uint8_t res :2;
			uint8_t rstdt :1;
			uint8_t stallrqc :1;
			uint8_t stallrq :1;
		};
		uint8_t v;
	} ueconx;
	union {
		struct {
			uint8_t epdir :1;
			uint8_t res :5;
			uint8_t eptype :2;
		};
		uint8_t v;
	} uecfg0x;
	union {
		struct {
			uint8_t res :1;
			uint8_t alloc :1;
			uint8_t epbk1 :2;
			uint8_t epsize :3;
			uint8_t res2 :1;
		};
		uint8_t v;
	} uecfg1x;
	union {
		struct {
			uint8_t nbusybk :2;
			uint8_t dtseq :2;
			uint8_t res :1;
			uint8_t underfi :1;
			uint8_t overfi :1;
			uint8_t cfgok :1;
		};
		uint8_t v;
	} uesta0x;
	union {
		struct {
			uint8_t curbk :2;
			uint8_t ctrldir :1;
			uint8_t res :5;
		};
		uint8_t v;
	} uesta1x;
	union {
		struct {
			uint8_t txine :1;
			uint8_t stallede :1;
			uint8_t rxoute :1;
			uint8_t rxstpe :1;
			uint8_t nakoute :1;
			uint8_t res :1;
			uint8_t nakine :1;
			uint8_t flerre :1;
		};
		uint8_t v;
	} ueienx;

	struct {
		uint8_t bytes[64];
		uint8_t tail;
	} bank[2];
	uint8_t current_bank;
};

struct usb_internal_state {
	pthread_mutex_t mutex;
    pthread_cond_t cpu_action;
	struct _epstate ep_state[5];
	avr_int_vector_t com_vect;
	avr_int_vector_t gen_vect;
};

enum usbints {
	suspi = 0, sofi = 2, eorsti = 3, wakeupi = 4, eorsmi = 5, uprsmi = 6
};
