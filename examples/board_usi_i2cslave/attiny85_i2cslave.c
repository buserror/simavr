#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>

#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "attiny85");
// Use a General Purpose I/O Register as a "virtual UART"
static int v_putchar(char c)
{
	GPIOR0 = c; // simavr can "watch" this register
	return 0;
}

static int v_puts(char *s)
{
	for (char *c = s; (*c) != '\0'; c++) {
		v_putchar(*c);
	}
	return 0;
}

/*******************************************************************************
 * Callbacks and data handling
 */

typedef enum {
    USII2CSLV_WAITING,
    USII2CSLV_RECEIVING_ADDRESS,
    USII2CSLV_ACKING_ADDRESS_FOR_TX,
    USII2CSLV_SENDING_DATA,
    USII2CSLV_RECEIVING_ACK_FOR_TX,
    USII2CSLV_ACKING_GEN_CALL,
    USII2CSLV_ACKING_RX_DATA,
    USII2CSLV_RECEIVING_DATA
} UsiI2cSlv_TrfState_t;

static volatile UsiI2cSlv_TrfState_t i2cStatus;
static const uint8_t i2cAddress = 0x42;

//Set i2cRxBuffer or i2cTxBuffer to NULL pointers to disable rx from or tx to master
#define BUF_SIZE 25
static uint8_t mainBuffer[BUF_SIZE * 2], loopBuf[BUF_SIZE];
static uint8_t *i2cRxBuffer = mainBuffer, *i2cTxBuffer = (mainBuffer + BUF_SIZE);
static volatile uint8_t loopBufDatLen = 0, txCount = 0, rxCount = 0;
static uint8_t rxBufLen = BUF_SIZE, txBufLen = 0;

static inline void printLoopBuf(void)
{
	v_puts("Got ");
	v_putchar('0' + ((loopBufDatLen / 10) % 10));
	v_putchar('0' + (loopBufDatLen % 10));
	v_puts(" bytes: ");
	for (int i = 0; i < loopBufDatLen; i++) {
		v_putchar('|'); v_putchar(loopBuf[i]);
	}
	v_putchar('|'); v_putchar('\r');
	loopBufDatLen = 0;
}

static inline uint8_t loadTxCallback(void)
{
	static uint8_t offset = 'B';
	if (txCount) { //Data was sent out from buffer
		v_puts("Tx needs > ");
		v_putchar('0' + ((txCount / 10) % 10));
		v_putchar('0' + (txCount % 10));
		v_putchar('\r');
		offset += txCount;
	}
	for (uint8_t i = 0; i < 4; i++) { //Do not load more than BUF_SIZE
		i2cTxBuffer[i] = i + offset;
	}
	txBufLen = 4;
	txCount = 0; //Reset counter
	return txBufLen; //The amount of data loaded
}

static inline void txDoneCallback(void) {
	if (txCount) { //Data was sent out from buffer
		txBufLen = 0; //To avoid sending the same tx data, by not reusing buffer
		v_puts("Tx done ");
		v_putchar('0' + ((txCount / 10) % 10));
		v_putchar('0' + (txCount % 10));
		v_putchar('\r');
		txCount = 0; //Reset counter
	}
}

static inline void rcvDoneCallback(void) {
	if (rxCount) { //Data available in buffer
		if (!loopBufDatLen) { //Last rx data has been consumed
			//Copy data into loopBuf for consumption in main loop
			for (int i = 0; i < rxCount; i++) {
				loopBuf[i] = i2cRxBuffer[i];
			}
			loopBufDatLen = rxCount;
		}
        PORTB ^= (1 << PB1); //Toggle PB1
		rxCount = 0; //Reset counter
	}
}

/*******************************************************************************
 * USI I2C slave peripheral handling
 */

#if defined(__AVR_ATtiny2313__)
	#  define DDR_USI             DDRB
	#  define PORT_USI            PORTB
	#  define PIN_USI             PINB
	#  define PORT_USI_SDA        PB5
	#  define PORT_USI_SCL        PB7
	#  define PIN_USI_SDA         PINB5
	#  define PIN_USI_SCL         PINB7
	#  define USI_OVERFLOW_VECTOR USI_OVERFLOW_vect
#elif defined(__AVR_ATtiny84__) | defined(__AVR_ATtiny44__)
	#  define DDR_USI             DDRA
	#  define PORT_USI            PORTA
	#  define PIN_USI             PINA
	#  define PORT_USI_SDA        PORTA6
	#  define PORT_USI_SCL        PORTA4
	#  define PIN_USI_SDA         PINA6
	#  define PIN_USI_SCL         PINA4
	#  define USI_OVERFLOW_VECTOR USI_OVF_vect
#elif defined(__AVR_ATtiny25__) | defined(__AVR_ATtiny45__) |		\
		defined(__AVR_ATtiny85__) | defined(__AVR_ATtiny261__) |	\
		defined(__AVR_ATtiny461__) | defined(__AVR_ATtiny861__)
	#  define DDR_USI             DDRB
	#  define PORT_USI            PORTB
	#  define PIN_USI             PINB
	#  define PORT_USI_SDA        PB0
	#  define PORT_USI_SCL        PB2
	#  define PIN_USI_SDA         PINB0
	#  define PIN_USI_SCL         PINB2
	#  define USI_OVERFLOW_VECTOR USI_OVF_vect
#endif
#  define USI_START_VECTOR    USI_START_vect

static inline void UsiI2cSlv_InitPins(void)
{
	// Configuring the pins as inputs
    DDR_USI  &= ~(_BV(PORT_USI_SDA) | _BV(PORT_USI_SCL));
	// Setting the pins pulled-up
    PORT_USI |=  _BV(PORT_USI_SDA);// | _BV(PORT_USI_SCL); leave SCL out for simulation
}

static inline void UsiI2cSlv_InitPeri(void)
{
	i2cStatus = USII2CSLV_WAITING;
    USICR = (1 << USISIE) | //Enable Start cond interrupt
			(0 << USIOIE) | //Disable Overflow interrupt
			(1 << USIWM1) | (0 << USIWM0) | //Normal 2 wire mode
            (1 << USICS1) | (0 << USICS0) | //External clock source (+ve trigger)
			(0 << USICLK) | //Clock Strobe (software trigger)
			(0 << USITC); //Software Clock Strobe
	// Clear all status flags and reset overflow counter
    USISR = (1 << USISIF) | (1 << USIOIF) | (1 << USIPF) |
			(1 << USIDC) | (0x0 << USICNT0);
}

void UsiI2cSlv_Init(void)
{
	UsiI2cSlv_InitPins();
	UsiI2cSlv_InitPeri();
}

static inline void UsiI2cSlv_CheckLastTrf(void)
{
	rcvDoneCallback();
	txDoneCallback();
}

/**
 * @brief USI Start Condition detected interrupt handler
 * @note Start Condition = SDA falling edge when SCL is high
 */
ISR(USI_START_VECTOR)
{
	// Checking for immediate Stop after start
	while (PIN_USI & (1 << PIN_USI_SCL)) {
		if (PIN_USI & (1 << PIN_USI_SDA)) {
			// SDA going high during SCL high means immediate STOP after
			UsiI2cSlv_InitPeri();
			goto USI_START_VECTOR_EXIT;
		}
	}
	// Start receiving address from master with clock stretching
    i2cStatus = USII2CSLV_RECEIVING_ADDRESS;
    USICR = (1 << USISIE) | //Enable Start cond interrupt (for repeated start)
			(1 << USIOIE) | //Enable Overflow interrupt (counts 16 SCL edges)
			(1 << USIWM1) | (1 << USIWM0) | //Clock-stretching 2 wire mode
            (1 << USICS1) | (0 << USICS0) | //External clock source (+ve trigger)
			(0 << USICLK) | //Clock Strobe (software trigger)
			(0 << USITC); //Software Clock Strobe
	// Clear all status flags and reset overflow counter
    USISR = (1 << USISIF) | (1 << USIOIF) | (1 << USIPF) |
			(1 << USIDC) | (0x0 << USICNT0);
	USI_START_VECTOR_EXIT:
	UsiI2cSlv_CheckLastTrf();
}

static inline void SET_USI_TO_SEND_ACK(UsiI2cSlv_TrfState_t newStatus)
{
	i2cStatus = newStatus;
	USIDR	=	0;
	PORT_USI&= ~_BV(PORT_USI_SDA);
	DDR_USI	|=	_BV(PORT_USI_SDA);
	USISR	=	(0 << USISIF)  | (1 << USIOIF) | (1 << USIPF) |
				(1 << USIDC)| (0x0E << USICNT0);
}

static inline void SET_USI_TO_RECEIVE_ACK()
{
	i2cStatus = USII2CSLV_RECEIVING_ACK_FOR_TX;
	DDR_USI	&= ~(1 << PORT_USI_SDA);
	PORT_USI|=	_BV(PORT_USI_SDA);
	USIDR	=	0;
	USISR	=	(0 << USISIF)   | (1 << USIOIF) | (1 << USIPF) |
				(1 << USIDC) | (0x0E << USICNT0);
}

static inline void SET_USI_TO_TWI_START_CONDITION_MODE()
{
	i2cStatus = USII2CSLV_WAITING;
    DDR_USI &= ~(1 << PORT_USI_SDA);
	PORT_USI|=	_BV(PORT_USI_SDA);
    USICR = (1 << USISIE) | //Enable Start condfition interrupt
			(0 << USIOIE) | //DIsable Overflow interrupt
			(1 << USIWM1) | (0 << USIWM0) | //Normal 2 wire mode
            (1 << USICS1) | (0 << USICS0) | //External clock source (+ve trigger)
			(0 << USICLK) | //Clock Strobe (software trigger)
			(0 << USITC); //Software Clock Strobe
	// Clear all status flags except StartCond and reset overflow counter
    USISR = (0 << USISIF) | (1 << USIOIF) | (1 << USIPF) |
			(1 << USIDC) | (0x0 << USICNT0);
}

static inline void SET_USI_TO_SEND_DATA()
{
	i2cStatus = USII2CSLV_SENDING_DATA;
    DDR_USI |= (1 << PORT_USI_SDA);
    USISR    = (0 << USISIF)    | (1 << USIOIF) |
               (1 << USIPF) | ( 1 << USIDC) | ( 0x0 << USICNT0 );
}

static inline void SET_USI_TO_RECEIVE_DATA()
{
	i2cStatus = USII2CSLV_RECEIVING_DATA;
	DDR_USI &= ~(1 << PORT_USI_SDA);
	PORT_USI|=	_BV(PORT_USI_SDA);
	USISR	=	(0 << USISIF)   | (1 << USIOIF) | (1 << USIPF) |
				(1 << USIDC) | (0x0 << USICNT0);
}

ISR(USI_OVERFLOW_VECTOR)
{
    switch (i2cStatus) {
	case (USII2CSLV_RECEIVING_ADDRESS):
		if (USIDR == 0) {	// General Call
			SET_USI_TO_SEND_ACK(USII2CSLV_ACKING_GEN_CALL);
			break;
		} else if ((USIDR >> 1) == i2cAddress) {
			if (USIDR & 0x01) {	//R => Master Reading from slave
				if (
					(i2cTxBuffer) && //Tx configured and
					((txBufLen) || //Buffer not sent or
					(loadTxCallback())) //Buffer refreshed
				) {
					SET_USI_TO_SEND_ACK(USII2CSLV_ACKING_ADDRESS_FOR_TX);
					break;
				}
			} else {	//W => Master Writing to slave
				if (i2cRxBuffer) {	//Rx configured
					SET_USI_TO_SEND_ACK(USII2CSLV_ACKING_RX_DATA);
					break;
				}
			}
			//At this point NACK is needed, as read or write is not possible
		} else {/* Address does not match, so don't touch bus */}
		SET_USI_TO_TWI_START_CONDITION_MODE();
		break;
	case (USII2CSLV_RECEIVING_ACK_FOR_TX):
		if (USIDR) {	// Got a NACK from master
			txCount--; //Last byte not taken
			txDoneCallback();
			SET_USI_TO_TWI_START_CONDITION_MODE();
			break;
		}
		__attribute__((fallthrough));
	case (USII2CSLV_ACKING_ADDRESS_FOR_TX):
		if (i2cTxBuffer) {
			if (txCount >= txBufLen) {
				loadTxCallback();
			}
			if (txCount < txBufLen) {
				USIDR = i2cTxBuffer[txCount];
				txCount++;
				SET_USI_TO_SEND_DATA();	//st = USII2CSLV_SENDING_DATA
				break;
			}
		}
		SET_USI_TO_TWI_START_CONDITION_MODE();
		//This results in HiZ SDA, so master receives 0xFF from bus
		break;
	case (USII2CSLV_SENDING_DATA):
		SET_USI_TO_RECEIVE_ACK();	//st = USII2CSLV_RECEIVING_ACK_FOR_TX
		break;
	case (USII2CSLV_ACKING_GEN_CALL):
		SET_USI_TO_TWI_START_CONDITION_MODE();
		//This results in a NACK, making master stop sending more data
		break;
	case (USII2CSLV_ACKING_RX_DATA):
		SET_USI_TO_RECEIVE_DATA();	//st = USII2CSLV_RECEIVING_DATA
		break;
	case (USII2CSLV_RECEIVING_DATA):
		if (i2cRxBuffer) {	//RxInto buffer configured
			if (rxCount < rxBufLen) {
				i2cRxBuffer[rxCount] = USIDR;
				rxCount++;
				SET_USI_TO_SEND_ACK(USII2CSLV_ACKING_RX_DATA);
				break;
			} else {	//Buffer is full
				rcvDoneCallback();
			}
		}
		SET_USI_TO_TWI_START_CONDITION_MODE();
		//This results in a NACK, making master stop sending more data
		break;
	default:
		SET_USI_TO_TWI_START_CONDITION_MODE();
		break;
    }
}

/*******************************************************************************
 * Main function
 */

int main(void)
{
    // Set PB1 as output (Toggled on data reception)
    DDRB |= (1 << PB1);

	v_puts("Initializing...\r");
	UsiI2cSlv_Init();

	v_puts("sei\r");
	sei();

    while (1) {
		if (loopBufDatLen)
			printLoopBuf();
	}

    return 0;
}
