#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "i2c_master.h"

static inline struct msg *msg_current( i2c_master *m ) {
    return &m->msgs->msg[ m->current_msg ];
}

static inline struct msg *msg_preview( i2c_master *m ) {
    return &m->msgs->msg[ m->current_msg + 1 ];
}

static inline int msg_hasNext( i2c_master *m ) {
    return ( m->current_msg + 1 ) < m->msgs->nmsg;
}

static inline void msg_moveNext( i2c_master *m ) {
    m->current_msg++;
}

static inline void msg_free( i2c_master *m ) {
    free( m->msgs->msg );
    m->msgs->msg = NULL;
    free( m->msgs );
    m->msgs = NULL;
}

static void i2c_master_send_hook( struct avr_irq_t *irq, uint32_t value, void *param ) {
    ( ( void ) irq );

    i2c_master *p = ( i2c_master * ) param;
    avr_twi_msg_irq_t v;
    v.u.v = value;

    if( !p->selected ) {
        return;
    }

    switch( v.u.twi.msg ) {
        case( TWI_COND_ACK | TWI_COND_ADDR ):

            if( p->index >= msg_current( p )->size ) {
                if( msg_hasNext( p ) && msg_current( p )->mode == msg_preview( p )->mode ) {
                    p->index = 0;
                    msg_moveNext( p );
                }
                else if( msg_hasNext( p ) && msg_current( p )->mode != msg_preview( p )->mode ) {
                    p->index = 0;
                    msg_moveNext( p );
                    if( p->verbose )
                        printf( "Send start to 0x%02X\n", p->selected );
                    avr_raise_irq( p->irq + TWI_IRQ_INPUT,
                                   avr_twi_irq_msg( TWI_COND_START | TWI_COND_ADDR |
                                                        ( msg_current( p )->mode == TWI_WRITE
                                                              ? TWI_COND_WRITE
                                                              : TWI_COND_READ ),
                                                    p->selected, 1 ) );
                    break;
                }
            }

            if( p->index < msg_current( p )->size ) {
                if( msg_current( p )->mode == TWI_WRITE ) {
                    char c = msg_current( p )->buffer[ p->index++ ];
                    if( p->verbose )
                        printf( "Send Databyte: '%c' (0x%02X)\n", c, c );
                    avr_raise_irq( p->irq + TWI_IRQ_INPUT,
                                   avr_twi_irq_msg( msg_current( p )->mode == TWI_WRITE
                                                        ? TWI_COND_WRITE
                                                        : TWI_COND_READ,
                                                    p->selected, c ) );
                }
            }
            else {
                if( msg_current( p )->mode == TWI_WRITE ) {
                    if( p->verbose )
                        printf( "Send stop!\n" );
                    avr_raise_irq( p->irq + TWI_IRQ_INPUT,
                                   avr_twi_irq_msg( TWI_COND_STOP, p->selected, 0 ) );
                }
                else {
                    if( p->verbose )
                        printf( "Send NACK!\n" );
                    avr_raise_irq( p->irq + TWI_IRQ_INPUT,
                                   avr_twi_irq_msg( TWI_COND_READ, p->selected, 0 ) );
                }

                p->selected = 0;
                msg_free( p );
            }
            break;
        case( TWI_COND_ACK | TWI_COND_READ ):
        case( TWI_COND_ACK | TWI_COND_ADDR | TWI_COND_READ ):
            msg_current( p )->buffer[ p->index++ ] = v.u.twi.data;
            if( p->verbose )
                printf( "Receive Databyte: '%c' (0x%02X)\n", v.u.twi.data, v.u.twi.data );
            avr_raise_irq(
                p->irq + TWI_IRQ_INPUT,
                avr_twi_irq_msg( TWI_COND_READ |
                                     ( p->index < msg_current( p )->size ? TWI_COND_ACK : 0 ),
                                 p->selected, 1 ) );
            break;
        case TWI_COND_ACK: break;
        default:
            if( p->verbose )
                printf( "Undefined state: 0x%02X (0x%08X)\n", v.u.twi.msg, value );
            avr_raise_irq( p->irq + TWI_IRQ_INPUT,
                           avr_twi_irq_msg( TWI_COND_STOP, p->selected, 0 ) );

            p->selected = 0;
            msg_free( p );
            break;
    }
}

static const char *_master_irq_names[ 2 ] = {
    [TWI_IRQ_INPUT] = "8>master.out",
    [TWI_IRQ_OUTPUT] = "32<master.in",
};

void i2c_master_init( avr_t *avr, i2c_master *p ) {
    p->avr = avr;
    p->irq = avr_alloc_irq( &avr->irq_pool, 0, 2, _master_irq_names );
    p->selected = 0;
}

void i2c_master_attach( avr_t *avr, i2c_master *p, uint32_t i2c_irq_base ) {
    avr_connect_irq( p->irq + TWI_IRQ_INPUT, avr_io_getirq( avr, i2c_irq_base, TWI_IRQ_INPUT ) );
    avr_connect_irq( avr_io_getirq( avr, i2c_irq_base, TWI_IRQ_OUTPUT ), p->irq + TWI_IRQ_OUTPUT );

    avr_irq_register_notify( p->irq + TWI_IRQ_OUTPUT, i2c_master_send_hook, p );
}

void twi_startTransmission( avr_t *avr, i2c_master *m, struct nmsg *msgs ) {
    ( ( void ) avr );
    m->current_msg = 0;
    m->index = 0;
    m->msgs = msgs;

    if( !msg_hasNext( m ) ) {
        return;
    }

    m->selected = msg_current( m )->address;
    if( msg_current( m )->mode == TWI_WRITE ) {
        // avr_irq_register_notify( m->irq + TWI_IRQ_OUTPUT, i2c_master_send_hook, m );
    }

    if( m->verbose )
        printf( "Send start to 0x%02X\n", m->selected );
    avr_raise_irq(
        m->irq + TWI_IRQ_INPUT,
        avr_twi_irq_msg( TWI_COND_START | TWI_COND_ADDR | TWI_COND_WRITE, m->selected, 1 ) );
}
