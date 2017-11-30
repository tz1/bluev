#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

//#define F_CPU         18432000
#ifndef F_CPU
#define F_CPU  	16000000
#endif
#define BAUD 	57600

#define PRESCALE1 8
#define BITTIME(x)		( ((x) * F_CPU) / PRESCALE1 / BAUD)

/*-------------------------------------------------------------------------*/
#define INMSGSIZE 24
unsigned char inmsgbuf[INMSGSIZE];
volatile unsigned char inmsglen;
volatile unsigned char inmsgstate;

static unsigned char inmsgcks;
static volatile unsigned char slice = 4;
volatile unsigned char transp = 0;

static volatile unsigned char nochecksum = 0;
#define ANYSLICE
// Received character from bluetooth
static void inchar(unsigned char c)
{
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
          )                     // valid source ?
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

// Save off characters until a valid V1 ESP packet is complete
ISR(USART2_RX_vect)
{
    inchar(UDR2);
}

unsigned char inbuf[256], v1buf[256];
volatile unsigned char v1head = 0, v1tail = 0, inhead = 0, intail = 0;
ISR(USART0_RX_vect)
{
    if (transp) {
        inchar(UDR0);
        return;
    }
    inbuf[inhead++] = UDR0;
}

/*-------------------------------------------------------------------------*/
static volatile unsigned int bitcnt;
static unsigned int frametime;
// Time Slice processor
// This counts character times to find the slot to transmit.  
// Each slot is 45 character times wide
// FIXME - doesn't try to resync or otherwise avoid collisions with other devices
ISR(TIMER4_COMPB_vect)
{
    OCR4B += BITTIME(10);
    frametime++;

    if (frametime < 45 * slice)
        return;

    if (inmsgstate != 4) {      // nothing for this frame
        TIMSK4 &= ~_BV(OCIE4B); // slice processor off
        UCSR1B &= ~_BV(TXEN0);  // TX off - just in case
        frametime = 0;
        return;
    }

    if (
#ifdef ANYSLICE
      (slice == 0 && frametime == 1) ||
#endif
      frametime == 45 * slice) {        // At current time slice

        // Holdoff for late previous time slice
        if (bitcnt)
            frametime--;
        else
            UCSR1B |= _BV(TXEN0);       // TX on
        return;
    }

    if (frametime >= 45 * (slice + 1)) {        // end of time slice
        TIMSK4 &= ~_BV(OCIE4B); // slice processor off
        UCSR1B &= ~_BV(TXEN0);  // TX off
        frametime = 0;
        inmsgstate = 0;
        return;
    }

    if (!(frametime & 1)) {     // Data Out Pacing, every other frame until done
        unsigned char ptr = (frametime - (45 * slice + 1)) >> 1;
        if (ptr < inmsglen)
            UDR1 = inmsgbuf[ptr];
        if (ptr > inmsglen)
            UCSR1B &= ~_BV(TXEN0);      // TX off
    }
}

// This tracks the V1 infDisplayData packet to sync the ESP cycle
static volatile unsigned char v1state, thislen;
static unsigned char infDisp[] = "\xaa\xd8\xea\x31\x09";        // put in Flash?
static void dostate(unsigned char val)
{
    // FIXME - hardcoded packet length
    // on the fly comparison of the first 5 bytes of the infDisplay packet
    if (v1state < 5) {
        v1state++;
        if (v1state == 3) {     // Checksum or not?
            if (val == 0xea)
                infDisp[4] = 9, nochecksum = 0;
            else if (val == 0xe9)
                infDisp[4] = 8, nochecksum = 1;
            else
                v1state = 0;
            return;
        }
        if (val == infDisp[v1state - 1])
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
        OCR4B = OCR4A + BITTIME(10) + BITTIME(1) / 2;
        TIFR4 |= _BV(OCF4B);    /* clear compare match interrupt */
        TIMSK4 |= _BV(OCIE4B);  /* enable compare match interrupt */
        PORTB ^= _BV(PB7);
        return;
    }
    if (thislen > 6 + infDisp[4])       // too long
        v1state = 0;
}

static unsigned char outchar;   // bitbang UART receive register
static unsigned char polarity;  // which edge are we looking for
volatile unsigned legacy;
//Stopbit for software UART
ISR(TIMER4_COMPA_vect)
{
    TIMSK4 &= ~_BV(OCIE4A);     // disable
    if (!polarity) {            // not break condition
        while (bitcnt < 10) {   // fill in one bits up to stop bit
            bitcnt++;
            outchar >>= 1;
            outchar |= 0x80;
        }
        UDR2 = outchar;
        // UDR0=64|63&outchar;
        if (transp)
            UDR0 = outchar;
        else
            v1buf[v1head++] = outchar;
        dostate(outchar);
        if(legacy)
            legacy--;
    }
    else {                      // break, reset things for next start bit
        TCCR4B &= ~_BV(ICES1);
        polarity = 0;
    }
    bitcnt = 0;
}

// Software UART via edges
char legbits[36] = "+medcbap87654321gfKALFFFSSSRRRIXMNOP";
unsigned long legimg;
ISR(TIMER4_CAPT_vect)
{
    static unsigned lastedge;
    unsigned thisedge = ICR4;
    TCCR4B ^= _BV(ICES1);
    unsigned width = thisedge - lastedge;
    lastedge = thisedge;
    polarity ^= 1;

// Legacy Mode
#define LEGABIT (504)
#define USTICS(us) ((us) * (F_CPU/PRESCALE1)/1000000)
    if( legacy > 100 ) {
        if( width < USTICS(LEGABIT)/5 ) { // normal bits for ESP
            bitcnt = 0;
            legacy--;
            return;
        }
        if( width > USTICS(LEGABIT)*2 ) { // 9.58 mS nominal
            PORTB ^= _BV(PB7);
            bitcnt = 0;
            ;//UDR2 = UDR0 = '\n';
            return;
        }
        if( polarity )
            return;
        ++bitcnt;
        legimg <<= 1;
        if( width <  USTICS(LEGABIT)/2 ) {
            if( bitcnt < 37 ) {
                ;//UDR2 = UDR0 = legbits[bitcnt-1];
                legimg |= 1;
            }
            else  
                ;//UDR2 = UDR0 = '#';
        }
        else
            ;//UDR2 = UDR0 = '_';  
            
        if( bitcnt == 33 ) { // Simulate an infDisplay packet
            unsigned char outb, cks = 0;
            cks += v1buf[v1head++] = 0xaa; // SOF
            cks += v1buf[v1head++] = 0xd8; // Dest - broadcast
            cks += v1buf[v1head++] = 0xea; // Source - V1
            cks += v1buf[v1head++] = 0x31; // infDisplay
            cks += v1buf[v1head++] = 9; // Len

            outb = (legimg >> 26) & 0x1f;
            outb |= (legimg >> 10) & 0x40;
            outb |= (legimg >> 10) & 0x20;
            outb |= (legimg >> 18) & 0x80;
            
            cks += v1buf[v1head++] = outb;
            cks += v1buf[v1head++] = outb;

            outb = legimg >> 17; // signal strength
            cks += v1buf[v1head++] = outb;

            outb = (legimg >> 12) & 7;
            outb |= (legimg << 2) & 8;
            outb |= (legimg >> 6) & 0x20;
            outb |= (legimg >> 1) & 0x40;
            outb |= (legimg << 3) & 0x80;

            cks += v1buf[v1head++] = outb;
            cks += v1buf[v1head++] = outb;

            cks += v1buf[v1head++] = 0xe | ((legimg>>31) & 1);
            v1buf[v1head++] = 0;
            v1buf[v1head++] = 0;

            v1buf[v1head++] = cks; // checksum 
            v1buf[v1head++] = 0xab; // EOF

        }
        return;
    }

    if(width > USTICS(LEGABIT) - 25 && width < USTICS(LEGABIT) + 25 ) {
        if( (++legacy & 1023) == 0 ) {
            PORTB ^= _BV(PB7);
            UDR0 = 'L';
        }
        legacy &= 4095;
    }

    /* toggle interrupt on rising/falling edge */
    if (polarity && !bitcnt) { // start bit
        OCR4A = lastedge + BITTIME(9) + BITTIME(1) / 2;
        TIFR4 |= _BV(OCF4A);    /* clear compare match interrupt */
        TIMSK4 |= _BV(OCIE4A);  /* enable compare match interrupt */
        bitcnt = 1;
        return;
    }
    width += BITTIME(1) / 2;    // round up
    while (width >= BITTIME(1)) {       // Shift in bits based on width
        width -= BITTIME(1);
        bitcnt++;
        outchar >>= 1;
        if (polarity)
            outchar |= 0x80;
    }

}

void hwsetup()
{
    cli();
    v1state = inmsgstate = inmsglen = polarity = bitcnt = 0;

    DDRB = _BV(PB7); // PB7/LED
    // UART init
#include <util/setbaud.h>
    UBRR2H = UBRR1H = UBRR0H = UBRRH_VALUE;
    UBRR2L = UBRR1L = UBRR0L = UBRRL_VALUE;
#if USE_2X
    UCSR2A = UCSR1A = UCSR0A = _BV(U2X1);
#endif
    UCSR2C = UCSR1C = UCSR0C = _BV(UCSZ10) | _BV(UCSZ11);       // 8N1
    UCSR2B = UCSR0B = _BV(TXEN0) | _BV(RXEN0) | _BV(RXCIE0);    // Enable TX and RX
    // for UART1, only TX is enabled, and only when sending in the right timeslice

    // Timer init
    GTCCR = 0;                  //_BV(PSR10);         /* reset prescaler */

    // trigger on falling edge (default), noise cancel
    // lower 3 bits is div, off,1,8,64,256,1024,extfall,extris ; CS12,11,10
    TCCR4B = _BV(ICNC1) | _BV(CS11);

    // clear and enable Input Capture interrupt 
    TIFR4 |= _BV(ICF4) | _BV(TOV4) | _BV(OCF4A) | _BV(OCF4B);
    TIMSK4 = _BV(ICIE4);        // enable input capture only

    sei();                      // enable interrupts
    // and sleep between events
    set_sleep_mode(SLEEP_MODE_IDLE);
}


