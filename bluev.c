#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <string.h>

#define F_CPU  	20000000
#define BAUD 	57600

#define PRESCALE1 8
#define BITTIME(x)		( ((x) * F_CPU) / PRESCALE1 / BAUD)

/*-------------------------------------------------------------------------*/
#define INMSGSIZE 24
static unsigned char inmsgbuf[INMSGSIZE], inmsglen, inmsgcks;
static unsigned char slice = 4;

static unsigned char nochecksum = 0;
#define ANYSLICE
// Received character from bluetooth
static unsigned char inmsgstate;
// Save off characters until a valid V1 ESP packet is complete
ISR(USART_RX_vect)
{
    unsigned char c = UDR;

    switch (inmsgstate) {
    case 0:                    // SOF
        if (c == 0xaa) {
            inmsgcks = 0;
            inmsglen = 0;
            inmsgstate++;
        }
        break;
    case 1:                    // destination
        if ((c & 0xf0) == 0xd0)
            inmsgstate++;
        else
            inmsgstate = 0;
        break;
    case 2:                    // source
        if ((c & 0xf0) == 0xe0 
#ifndef ANYSLICE
	    && (c & 0x0f) <= 5 && (c & 0x0f) >= 3
#endif
	    )   // valid source ?
            inmsgstate++;
        else
            inmsgstate = 0;
        break;
    case 3:                    // ID through EOF
        if (inmsglen == 4 && c > 20) {  // validate length
            inmsgstate = 0;
            break;
        }
        if (inmsglen < 5)       // later tests require length
            break;

        if (!nochecksum)
            if (inmsgbuf[4] + 4 == inmsglen) {  // checksum byte
                if (inmsgcks != c)
                    inmsgstate = 0;
                break;
            }
        if (c == 0xab && inmsgbuf[4] + 5 == inmsglen) { // EOF - queue for send
            inmsgbuf[inmsglen] = c;     // avoid race 
            slice = inmsgbuf[2] & 0x0f;
            inmsgstate = 4;
        }
        break;
    case 4:                    // waiting for transmit - discard
        return;
    default:
        break;
    }
    if (inmsgstate && inmsglen < INMSGSIZE) {
        inmsgbuf[inmsglen++] = c;
        inmsgcks += c;
    }
    else                        // overflow - shouldn't happen given state machine
        inmsgstate = 0;
}

/*-------------------------------------------------------------------------*/
static unsigned int bitcnt;
static unsigned int frametime;
// Time Slice processor
// This counts character times to find the slot to transmit.  
// Each slot is 45 character times wide
// FIXME - doesn't try to resync or otherwise avoid collisions with other devices
ISR(TIMER1_COMPB_vect)
{
    OCR1B += BITTIME(10);
    frametime++;

    if (frametime < 45 * slice)
        return;

    if (inmsgstate != 4) {      // nothing for this frame
        TIMSK &= ~_BV(OCIE1B);  // slice processor off
        UCSRB &= ~_BV(TXEN);    // TX off - just in case
        frametime = 0;
        return;
    }

    if (
#ifdef ANYSLICE
	(slice == 0 && frametime == 1) || 
#endif
	frametime == 45 * slice) {      // At current time slice

        // Holdoff for late previous time slice
        if (bitcnt)
            frametime--;
        else
            UCSRB |= _BV(TXEN); // TX on
        return;
    }

    if (frametime >= 45 * (slice + 1)) {        // end of time slice
        TIMSK &= ~_BV(OCIE1B);  // slice processor off
        UCSRB &= ~_BV(TXEN);    // TX off
        frametime = 0;
        inmsgstate = 0;
        return;
    }

    if (!(frametime & 1)) {     // Data Out Pacing, every other frame until done
        unsigned char ptr = (frametime - (45 * slice + 1)) >> 1;
        if (ptr < inmsglen)
            UDR = inmsgbuf[ptr];
        if (ptr > inmsglen)
            UCSRB &= ~_BV(TXEN);        // TX off
    }
}

// This tracks the V1 infDisplayData packet to sync the ESP cycle
static unsigned char v1state, thislen;
unsigned char infDisp[] = "\xaa\xd8\xea\x31\x09";       // put in Flash?
void dostate(unsigned char val)
{
    // FIXME - hardcoded packet length
    // on the fly comparison of the first 5 bytes of the infDisplay packet
    if (v1state < 5) {
        v1state++;
        if (v1state == 3) { // Checksum or not?
            if (val == 0xea)
                infDisp[4] = 9, nochecksum = 0;
            else if (val == 0xe9)
                infDisp[4] = 8, nochecksum = 1;
            else
                v1state = 0;
            return;
        }
        if (val == infDisp[v1state-1])
            thislen = v1state;
        else
            v1state = 0;
        return;
    }
    thislen++;
    if (thislen == 11 && (val & 2)) {   // V1 TimeSlice holdoff
        v1state = 0;
        return;
    }
#if 0
    // FIXME? maybe validate checksum?
    if (thislen == 14 && val != ckcksum(inbuf, 14))
        return 0;
#endif
    if (val == 0xab && thislen == 6 + infDisp[4]) {     // EOF - start time slice sync
        frametime = 0;
        v1state = 0;
        OCR1B = OCR1A + BITTIME(10) + BITTIME(1) / 2;
        TIFR |= _BV(OCF1B);     /* clear compare match interrupt */
        TIMSK |= _BV(OCIE1B);   /* enable compare match interrupt */
        return;
    }
    if (thislen > 6 + infDisp[4])       // too long
        v1state = 0;
}

static unsigned char outchar;   // bitbang UART receive register
static unsigned char polarity;  // which edge are we looking for
//Stopbit for software UART
ISR(TIMER1_COMPA_vect)
{
    TIMSK &= ~_BV(OCIE1A);      // disable
    if (!polarity) {            // not break condition
        while (bitcnt < 10) {   // fill in one bits up to stop bit
            bitcnt++;
            outchar >>= 1;
            outchar |= 0x80;
        }
        dostate(outchar);
    }
    else {                      // break, reset things for next start bit
        TCCR1B &= ~_BV(ICES1);
        polarity = 0;
    }
    bitcnt = 0;
}

// Software UART via edges
ISR(TIMER1_CAPT_vect)
{
    static unsigned lastedge;
    TCCR1B ^= _BV(ICES1);
    /* toggle interrupt on rising/falling edge */
    if (!polarity && !bitcnt) { // start bit
        lastedge = ICR1;
        OCR1A = lastedge + BITTIME(9) + BITTIME(1) / 2;
        TIFR |= _BV(OCF1A);     /* clear compare match interrupt */
        TIMSK |= _BV(OCIE1A);   /* enable compare match interrupt */
        polarity = 1;
        bitcnt = 1;
        return;
    }
    unsigned thisedge = ICR1;
    unsigned width = thisedge - lastedge;
    lastedge = thisedge;
    width += BITTIME(1) / 2;    // round up
    while (width >= BITTIME(1)) {       // Shift in bits based on width
        width -= BITTIME(1);
        bitcnt++;
        outchar >>= 1;
        if (!polarity)
            outchar |= 0x80;
    }
    polarity ^= 1;
}

/*-------------------------------------------------------------------------*/
int main(void)
{
    /* power savings */
    PRR = _BV(PRUSI);           /* shut down USI */
    DIDR = _BV(AIN0D) | _BV(AIN1D);     /* disable digital input on analog */
    ACSR = _BV(ACD);            /* disable analog comparator */

    /* for testing
       inmsgstate = 4;
       strcpy_P( inmsgbuf, PSTR("\xaa\xda\xe4\x41\x01\xaa\xab") );
       inmsglen = 7;
     */
    v1state = inmsgstate = inmsglen = 0;

    // UART init
#include <util/setbaud.h>
    UBRRH = UBRRH_VALUE;
    UBRRL = UBRRL_VALUE;
#if USE_2X
    UCSRA |= (1 << U2X);
#else
    UCSRA &= ~(1 << U2X);
#endif
    UCSRC = _BV(UCSZ0) | _BV(UCSZ1);    // 8N1
    UCSRB = _BV(RXEN) | _BV(RXCIE);     // Enable Receive

    // Timer init
    GTCCR = _BV(PSR10);         /* reset prescaler */
    TCNT1 = 0;                  /* reset counter value */
    polarity = bitcnt = 0;

    // trigger on falling edge (default), noise cancel
    // lower 3 bits is div, off,1,8,64,256,1024,extfall,extris ; CS12,11,10
    TCCR1B = _BV(ICNC1) | _BV(CS11);

    // clear and enable Input Capture interrupt 
    TIFR |= _BV(ICF1) | _BV(TOV1) | _BV(OCF1A) | _BV(OCF1B);
    TIMSK |= _BV(ICIE1);        // enable input capture only

    sei();                      // enable interrupts
    // and sleep between events
    set_sleep_mode(SLEEP_MODE_IDLE);
    for (;;)
        sleep_mode();
}
