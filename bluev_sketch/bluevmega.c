#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <string.h>
#include <stdio.h>

/*========================================================================*/
// V1 Command, Response, and Info packet Processing

/*========================================================================*/
static unsigned char cmdslice = 4;
extern volatile unsigned char v1head, v1tail, inhead, intail;
extern unsigned char v1buf[], inbuf[];
extern unsigned char inmsgbuf[];
extern volatile unsigned char inmsglen, inmsgstate;
extern volatile unsigned char transp;
// LOW LEVEL ROUTINES FOR ARDUINO

// Read one character from the V1 data stream
static int readv1rx(void)
{
    while (v1head == v1tail)
        sleep_mode();
    return v1buf[v1tail++];
}

static char serbuf[256];
// print out strings to the main (USB) serial port
static void printser(char *str)
{
    while (*str) {
        while (!(UCSR0A & _BV(UDRE0)));
        UDR0 = *str++;
    }
}

static char localstr[128];
static char *pullp(const char *p)
{
    strcpy_P(localstr, p);
    return localstr;
}

static int readpkt(unsigned char *);
static unsigned char respget[];
static int getkey() {
    while (inhead == intail)
        readpkt(respget);
    return inbuf[intail++];
}

static int keypress() {
    return( inhead != intail);
}

#define REQVERSION (1)
#define RESPVERSION (2)
#define REQSERIALNUMBER (3)
#define RESPSERIALNUMBER (4)

#define REQUSERBYTES (0x11)
#define RESPUSERBYTES (0x12)
#define REQWRITEUSERBYTES (0x13)
#define REQFACTORYDEFAULT (0x14)

#define REQWRITESWEEPDEFINITIONS (0x15)
#define REQALLSWEEPDEFINITIONS (0x16)
#define RESPSWEEPDEFINITION (0x17)
#define REQDEFAULTSWEEPS (0x18)

#define REQMAXSWEEPINDEX (0x19)
#define RESPMAXSWEEPINDEX (0x20)
#define RESPSWEEPWRITERESULT (0x21)
#define REQSWEEPSECTIONS (0x22)
#define RESPSWEEPSECTIONS (0x23)

#define INFDISPLAYDATA (0x31)
#define REQTURNOFFMAINDISPLAY (0x32)
#define REQTURNONMAINDISPLAY (0x33)
#define REQMUTEON (0x34)
#define REQMUTEOFF (0x35)
#define REQCHANGEMODE (0x36)

#define REQSTARTALERTDATA (0x41)
#define REQSTOPALERTDATA (0x42)
#define RESPALERTDATA (0x43)

#define RESPDATARECEIVED (0x61)
#define REQBATTERYVOLTAGE (0x62)
#define RESPBATTERYVOLTAGE (0x63)
#define RESPUNSUPPORTEDPACKET (0x64)
#define RESPREQUESTNOTPROCESSED (0x65)
#define INFV1BUSY (0x66)
#define RESPDATAERROR (0x67)

#define REQSAVVYSTATUS (0x71)
#define RESPSAVVYSTATUS (0x72)
#define REQVEHICLESPEED (0x73)
#define RESPVEHICLESPEED (0x74)
#define REQOVERRIDETHUMBWHEEL (0x75)
#define REQSETSAVVYUNMUTEENABLE (0x76)

#define NORESPONSE (0xfe)

static unsigned char tstnocks = 0;      // is this a checksummed V1?

// Create a valid V1 command in BUF, return length.  Returns 0 if something is invalid
// Note it permits source and dest 0-15, not just a source of 3,4,5.
// pkt is the ID of the command.
// len is size of param, param points to the bytes to go after the length
// (before and not including the optional checksum which will be handled
// according to what the infDisplay is doing.
static int makecmd(unsigned char *buf, unsigned char src, unsigned char dst, unsigned char pkt,
  unsigned char len, unsigned char *param)
{
    if (len > 16)
        return 0;
    if (src > 15 || dst > 15)
        return 0;
    if (len && !param)
        return 0;
    unsigned char *b = buf, l = len;
    *b++ = 0xaa;
    if (tstnocks && dst == 0x0a)
        dst = 9;
    if (dst == 9 && !tstnocks)
        dst = 0x0a;
    *b++ = 0xd0 + dst;
    *b++ = 0xe0 + src;
    *b++ = pkt;
    *b++ = len + !tstnocks;
    while (l--)
        *b++ = *param++;
    if (!tstnocks) {
        unsigned char cks = 0, ix = 0;
        for (ix = 0; ix < len + 5; ix++)
            cks += buf[ix];
        *b++ = cks;
    }
    *b++ = 0xab;
    return b - buf;
};

static unsigned char v1idd = 0, v1infdisplaydata[8];
static unsigned char v1alerts = 0, v1alerttemp[16][7], v1alertout[16][7];
static unsigned char cddr = 0;

// Get a full, valid packet from the V1 - SOF, EOF, source, destination, and length bytes checked.
// For unsolicited packets (infdisp, respalert, respdatarx), process internally
static int readpkt(unsigned char *buf)
{
    unsigned char len, ix;
    buf[0] = 0;
    for (;;) {
        while (buf[0] != 0xaa)  // SOF
        {
            buf[0] = readv1rx();
            //sprintf( serbuf, pullp(PSTR("%02x")), buf[0] );
            //printser( serbuf );
        }
        buf[1] = readv1rx();    // destination
        if ((buf[1] & 0xf0) != 0xd0)
            continue;
        buf[2] = readv1rx();    // source
        if ((buf[2] & 0xf0) != 0xe0)
            continue;
        if ((buf[2] & 15) == 0xa)       // check if checksum or nonchecksum
            tstnocks = 0;
        else if ((buf[2] & 15) == 9)
            tstnocks = 1;
        buf[3] = readv1rx();    // packet ID
        buf[4] = readv1rx();    // length
        len = 5;
        for (ix = 0; ix < buf[4]; ix++) {
            buf[len] = readv1rx();
            len++;
        }
        if (!tstnocks) {        // checksum if present
            unsigned char cks = 0;
            for (ix = 0; ix < len - 1; ix++)
                cks += buf[ix];
            if (buf[len - 1] != (cks & 0xff))
                return -2;      // continue; ???
        }
        buf[len] = readv1rx();  // EOF
        if (buf[len++] != 0xab)
            continue;

        // save off current alert or inf packet separately
        if (buf[3] == INFDISPLAYDATA) {
            // maybe verify rest of packet
            v1idd++;
            memcpy(v1infdisplaydata, buf + 5, 8);
        }
        // alerts
        else if (buf[3] == RESPALERTDATA) {
            //sprintf( serbuf, pullp(PSTR("a: %d\n")), buf[1] & 0xf);
            //printser(serbuf);
            // copy to temp until index == total, then move to out and inc
            if (!buf[5]) {      // no alerts
                if (!v1alertout[0][0]) {        // already zero?
                    memset(v1alertout, 0, 7);
                    v1alerts++;
                }
            }
            // v1 alertout is updated when a full set of alerts is received and accumulated.
            else {              // accumulate and copy block
                memcpy(v1alerttemp[(buf[5] >> 4) - 1], buf + 5, 7);
                if (buf[5] >> 4 == (buf[5] & 15)) {
                    memcpy(v1alertout, v1alerttemp, 7 * (buf[5] & 15));
                    v1alerts++;
                }
            }
        }
        // concealed display
        else if (buf[3] == RESPDATARECEIVED) {
            cddr++;
        }
        //else 
        {
#if 0
            // show nonstream traffic
            sprintf(serbuf, pullp(PSTR("r: %d:")), len);
            printser(serbuf);
            for (ix = 1; ix < len - 1; ix++) {
                sprintf(serbuf, pullp(PSTR(" %02x")), buf[ix]);
                printser(serbuf);
            }
            printser(pullp(PSTR("\r\n")));
#endif
        }
        return len;
    }
}

// send a command, wait until it goes out, then look for a response packet (by ID), placing it in buf.
static int sendcmd(unsigned char *thiscmd, unsigned char resp, unsigned char *buf)
{
    unsigned char ix, iy;
    int ret;

    memcpy(inmsgbuf, thiscmd, thiscmd[4] + 6);  // This queues the message for transmission
    inmsglen = thiscmd[4] + 6;
    inmsgstate = 4;

    // wait this many packets max for the command to go out on the bus
#define ECHOTIME 8
    for (ix = 0; ix < ECHOTIME; ix++) { // look for command on bus
        ret = readpkt(buf);
        //      sprintf( serbuf, pullp(PSTR("%d: %02x %02x %02x %02x %02x %02x %02x\r\n")), ret, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6] );
        if (ret < thiscmd[4] + 6)
            continue;
        if (!memcmp(thiscmd, buf, ret))
            break;
    }
    if (ix == ECHOTIME)         // can't transmit for some reason - TS holdoff?
        return -1;
    if (resp == NORESPONSE)     // no response required for this
        return 0;
    // look for response, max packets to wait - busy will reset.
#define RESPTIME 20
    for (ix = 0; ix < RESPTIME; ix++) {
        ret = readpkt(buf);
        if (ret < 6)
            continue;
        // FIXME - we don't check the source or destination ID.
        switch (buf[3]) {
        case RESPUNSUPPORTEDPACKET:
            // print, but don't exit because it may not be our request.
            printser(pullp(PSTR("Unsupported Packet\r\n")));
            break;
        case RESPREQUESTNOTPROCESSED:
            // if our packet ID, return error
            sprintf(serbuf, pullp(PSTR("Request Not Processed %02x\r\n")), buf[5]);
            printser(serbuf);
            if (buf[5] == thiscmd[3])
                return -3;
            break;
        case INFV1BUSY:
#if 0
            printser(pullp(PSTR("V1Busy:")));
            for (iy = 0; iy < buf[4] - 1; iy++) {
                sprintf(serbuf, pullp(PSTR(" %02x")), buf[5 + iy]);
                printser(serbuf);
            }
            printser(serbuf, "\r\n")));
#endif
            for (iy = 0; iy < buf[4] - 1; iy++)
                if (buf[5 + iy] == thiscmd[3])
                    ix = 0;     // reset timer since it is still processing our packet
            break;
        case RESPDATAERROR:
            sprintf(serbuf, pullp(PSTR("Data Error %02x\r\n")), buf[5]);
            printser(serbuf);
            if (buf[5] == thiscmd[3])
                return -4;
            break;
        }
        if (buf[3] == resp)
            break;
    }
    if (ix == RESPTIME)
        return -2;
    return 0;
}

// command and response buffers
static unsigned char respget[22], cmdsend[22];
static unsigned int maxswp = 5;

// make sure the buffer is clear by getting an echo.
// FIXME - should have long timeout and return status.
static void syncresp() {
    int iy;
    // send one request version to clear out the incoming packet respgetfer
    makecmd(cmdsend, cmdslice, 0xa, REQVERSION, 0, NULL);
    sendcmd(cmdsend, NORESPONSE, respget);
    for (;;) {
        iy = readpkt(respget);
        if (iy < 6)
            continue;
        if (respget[3] == RESPVERSION)
            break;
    }
}

// This is the first part of the sweep probe getting the sections and maximum index
static void sweep1() {
    int ix, ret;
    // Sweep Sections and Definitions
    makecmd(cmdsend, cmdslice, 0xa, REQSWEEPSECTIONS, 0, NULL);
    sendcmd(cmdsend, RESPSWEEPSECTIONS, respget);
    printser(pullp(PSTR("SweepSections:\r\n")));
    sprintf(serbuf, pullp(PSTR("+%d/%d %5u - %5u\r\n")), respget[5] >> 4, respget[5] & 15, respget[8] << 8 | respget[9],
      respget[6] << 8 | respget[7]);
    printser(serbuf);
    if (respget[4] > 6) {
        sprintf(serbuf, pullp(PSTR("+%d/%d %5u -:%5u\r\n")), respget[10] >> 4, respget[10] & 15, respget[13] << 8 | respget[14],
          respget[11] << 8 | respget[12]);
        printser(serbuf);
    }
    if (respget[4] > 11) {
        sprintf(serbuf, pullp(PSTR("+%d/%d %5u - %5u\r\n")), respget[15] >> 4, respget[15] & 15, respget[18] << 8 | respget[19],
          respget[16] << 8 | respget[17]);
        printser(serbuf);
    }
    unsigned int nswppkt = (respget[5] & 15);
    for (ix = 1; ix < nswppkt / 3;) {
        // read additional 0x23 packet, print it out
        ret = readpkt(respget);
        if (ret < 5)
            continue;
        if (respget[3] != 0x23)
            continue;
        ix++;
        sprintf(serbuf, pullp(PSTR("+%d/%d %5u - %5u\r\n")), respget[5] >> 4, respget[5] & 15, respget[8] << 8 | respget[9],
          respget[6] << 8 | respget[7]);
        printser(serbuf);

        if (respget[4] > 6) {
            sprintf(serbuf, pullp(PSTR("+%d/%d %5u -:%5u\r\n")), respget[10] >> 4, respget[10] & 15, respget[13] << 8 | respget[14],
              respget[11] << 8 | respget[12]);
            printser(serbuf);
        }
        if (respget[4] > 11) {
            sprintf(serbuf, pullp(PSTR("+%d/%d %5u - %5u\r\n")), respget[15] >> 4, respget[15] & 15, respget[18] << 8 | respget[19],
              respget[16] << 8 | respget[17]);
            printser(serbuf);
        }
    }
    // sweep definitions must stay within the sections above
    makecmd(cmdsend, cmdslice, 0xa, REQMAXSWEEPINDEX, 0, NULL);
    sendcmd(cmdsend, RESPMAXSWEEPINDEX, respget);
    sprintf(serbuf, pullp(PSTR("MaxSweepIndex: %d (+1 for number of definitions)\r\n")), respget[5]);
    printser(serbuf);
    maxswp = respget[5];
}

// This is the second part of the sweep probe which gets the actual sweep definitions
static void sweep2() {
    int ix, ret;
    // read sweep sections
    makecmd(cmdsend, cmdslice, 0xa, REQALLSWEEPDEFINITIONS, 0, NULL);
    sendcmd(cmdsend, RESPSWEEPDEFINITION, respget);
    printser(pullp(PSTR("SweepDefinitions:\r\n")));
    sprintf(serbuf, pullp(PSTR("+%d/%d Low:%5u High:%5u\r\n")), 1 + (respget[5] & 63), 1 + maxswp, respget[8] << 8 | respget[9],
      respget[6] << 8 | respget[7]);
    printser(serbuf);
    for (ix = 0; ix < maxswp;) {
        ret = readpkt(respget);
        if (ret < 5)
            continue;
        if (respget[3] != 0x17)
            continue;
        ix++;
        sprintf(serbuf, pullp(PSTR("+%d/%d Low:%5u High:%5u\r\n")), 1 + (respget[5] & 63), 1 + maxswp, respget[8] << 8 | respget[9],
          respget[6] << 8 | respget[7]);
        printser(serbuf);
    }
}

// user interactive program to get a 16 bit unsigned word (frequency to write sweep)
static unsigned getword() {
    unsigned ret = 0;
    for (;;) {
        unsigned char c = getkey();
        if (c >= ' ' && c < 127) {
          sprintf( serbuf, "%c", c );
          printser(serbuf);
        }
        switch (c) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            ret *= 10;
            ret += c - '0';
            break;
        case 127:
        case 8:
            ret /= 10;
            printser(pullp(PSTR("\b \b")));
            break;
        case '\r':
            return ret;
        case 0x15:
            ret = 0;
            printser(pullp(PSTR("\r            \r")));
            break;
        case 0x1b:
            return 0;
        default:
            break;
        }
    }
}

// Set a new set of sweep definitions
static void sweepset() {
    int ix, iy;
    unsigned low[20], high[20];

    syncresp();

    sweep1();
    sweep2();
    printser(pullp(PSTR("Enter Definition Ranges, low and high, 0 to end\r\n")));
    for (ix = 0; ix <= maxswp; ix++) {
        sprintf(serbuf, pullp(PSTR("Def %d Low:\r\n")), ix + 1);
        printser(serbuf);
        low[ix] = getword();
        printser(pullp(PSTR("\r\n")));
        if (low[ix] == 0)
            break;
        sprintf(serbuf, pullp(PSTR("Def %d High:\r\n")), ix + 1);
        printser(serbuf);
        high[ix] = getword();
        printser(pullp(PSTR("\r\n")));
        if (high[ix] == 0)
            break;
    }
    // print and ask to commit
    if (!ix)
        return;

    for (iy = 0; iy < ix; iy++) {
        sprintf(serbuf, pullp(PSTR("%2d: %5u - %5u\r\n")), iy + 1, low[iy], high[iy]);
        printser(serbuf);
    }
    printser(pullp(PSTR("Write to V1? (Y/N):")));
    char c = getkey();
    if (c != 'Y' && c != 'y')
        return;
    for (iy = 0; iy < ix; iy++) {
        unsigned char wrsbuf[5];
        // write, commit on the last.
        wrsbuf[0] = 0x80 | iy;
        if (iy == ix - 1)
            wrsbuf[0] |= 0x40;
        wrsbuf[1] = high[iy] >> 8;
        wrsbuf[2] = high[iy];
        wrsbuf[3] = low[iy] >> 8;
        wrsbuf[4] = low[iy];
        makecmd(cmdsend, cmdslice, 0xa, REQWRITESWEEPDEFINITIONS, 5, wrsbuf);
        sendcmd(cmdsend, NORESPONSE, respget);
        sprintf(serbuf, pullp(PSTR("Wrote %2d: %5u - %5u\r\n")), iy + 1, low[iy], high[iy]);
        printser(serbuf);
    }
    // get write response and decode errors.
    for (;;) {
        iy = readpkt(respget);
        if (iy < 6)
            continue;
        if (respget[3] == RESPSWEEPWRITERESULT)
            break;
    }
    if (respget[5]) {
        sprintf(serbuf, pullp(PSTR("Write Error in index %d\r\n")), respget[5] - 1);
        printser(serbuf);
    }
    else
        printser(pullp(PSTR("Write Successfup\r\n")));
    sweep2();
}

// set default sweeps
static void defaultsweeps() {
    syncresp();
    makecmd(cmdsend, cmdslice, 0xa, REQDEFAULTSWEEPS, 0, NULL);
    sendcmd(cmdsend, NORESPONSE, respget);
    printser(pullp(PSTR("Default Sweeps Done, Please use infoscan to confirm\r\n")));
}

static char userset[] = "12345678AbCdEFGHJuUtL   ";
// show user bytes
static void userprint() {
    int ix;
    printser(pullp(PSTR("UserSet: (default) ")));
    for (ix = 0; ix < 8; ix++) {
        sprintf(serbuf, pullp(PSTR("%c")), (respget[5] >> ix) & 1 ? userset[ix] : '_');
        printser(serbuf);
    }
    for (ix = 0; ix < 8; ix++) {
        sprintf(serbuf, pullp(PSTR("%c")), (respget[6] >> ix) & 1 ? userset[ix + 8] : '_');
        printser(serbuf);
    }
    for (ix = 0; ix < 8; ix++) {
        sprintf(serbuf, pullp(PSTR("%c")), (respget[7] >> ix) & 1 ? userset[ix + 16] : '_');
        printser(serbuf);
    }
    printser(pullp(PSTR("\r\nUserSet: (changed) ")));
    for (ix = 0; ix < 8; ix++) {
        sprintf(serbuf, pullp(PSTR("%c")), (respget[5] >> ix) & 1 ? '_' : userset[ix]);
        printser(serbuf);
    }
    for (ix = 0; ix < 8; ix++) {
        sprintf(serbuf, pullp(PSTR("%c")), (respget[6] >> ix) & 1 ? '_' : userset[ix + 8]);
        printser(serbuf);
    }
    for (ix = 0; ix < 8; ix++) {
        sprintf(serbuf, pullp(PSTR("%c")), (respget[7] >> ix) & 1 ? '_' : userset[ix + 16]);
        printser(serbuf);
    }
    printser(pullp(PSTR("\r\n")));
}

static void usershow() {
    // User settings
    makecmd(cmdsend, cmdslice, 0xa, REQUSERBYTES, 0, NULL);
    sendcmd(cmdsend, RESPUSERBYTES, respget);
    userprint();
}

// Scan for everything I could find in the spec for all devices
static void infoscan() {
    int ix;

    //    sprintf( serbuf, pullp(PSTR("VN: %d: %02x %02x %02x %02x %02x %02x %02x\r\n")), ix, cmdsend[0], cmdsend[1], cmdsend[2], cmdsend[3], cmdsend[4], cmdsend[5], cmdsend[6] );
    printser(pullp(PSTR("=====INFOSCAN=====\r\n")));
    syncresp();

    // do commands
    makecmd(cmdsend, cmdslice, 0xa, REQVERSION, 0, NULL);
    sendcmd(cmdsend, RESPVERSION, respget);
    printser(pullp(PSTR("V1 Version: ")));

    for (ix = 0; ix < respget[4] - 1; ix++) {
        sprintf(serbuf, pullp(PSTR("%c")), respget[5 + ix] < 127 && respget[5 + ix] > 31 ? respget[5 + ix] : '.');
        printser(serbuf);
    }
    printser(pullp(PSTR("\r\n")));

    makecmd(cmdsend, cmdslice, 0xa, REQSERIALNUMBER, 0, NULL);
    sendcmd(cmdsend, RESPSERIALNUMBER, respget);
    printser(pullp(PSTR("V1 SerialNo: ")));
    for (ix = 0; ix < respget[4] - 1; ix++) {
        sprintf(serbuf, pullp(PSTR("%c")), respget[5 + ix] < 127 && respget[5 + ix] > 31 ? respget[5 + ix] : '.');
        printser(serbuf);
    }
    printser(pullp(PSTR("\r\n")));

    makecmd(cmdsend, cmdslice, 0xa, REQBATTERYVOLTAGE, 0, NULL);
    sendcmd(cmdsend, RESPBATTERYVOLTAGE, respget);
    sprintf(serbuf, pullp(PSTR("BattVolt: %d.%02d\r\n")), respget[5], respget[6]);
    printser(serbuf);

    usershow();

    sweep1();
    sweep2();

    // Concealed Display
    makecmd(cmdsend, cmdslice, 0, REQVERSION, 0, NULL);
    sendcmd(cmdsend, RESPVERSION, respget);
    printser(pullp(PSTR("CD Version: ")));
    for (ix = 0; ix < respget[4] - 1; ix++) {
        sprintf(serbuf, pullp(PSTR("%c")), respget[5 + ix] < 127 && respget[5 + ix] > 31 ? respget[5 + ix] : '.');
        printser(serbuf);
    }
    printser(pullp(PSTR("\r\n")));

    // Remote Audio
    makecmd(cmdsend, cmdslice, 1, REQVERSION, 0, NULL);
    sendcmd(cmdsend, RESPVERSION, respget);
    printser(pullp(PSTR("RA Version: ")));
    for (ix = 0; ix < respget[4] - 1; ix++) {
        sprintf(serbuf, pullp(PSTR("%c")), respget[5 + ix] < 127 && respget[5 + ix] > 31 ? respget[5 + ix] : '.');
        printser(serbuf);
    }
    printser(pullp(PSTR("\r\n")));

    // Savvy
    makecmd(cmdsend, cmdslice, 2, REQVERSION, 0, NULL);
    sendcmd(cmdsend, RESPVERSION, respget);
    printser(pullp(PSTR("SV Version: ")));
    for (ix = 0; ix < respget[4] - 1; ix++) {
        sprintf(serbuf, pullp(PSTR("%c")), respget[5 + ix] < 127 && respget[5 + ix] > 31 ? respget[5 + ix] : '.');
        printser(serbuf);
    }
    printser(pullp(PSTR("\r\n")));

    makecmd(cmdsend, cmdslice, 2, REQSERIALNUMBER, 0, NULL);
    sendcmd(cmdsend, RESPSERIALNUMBER, respget);
    printser(pullp(PSTR("SV SerialNo: ")));
    for (ix = 0; ix < respget[4] - 1; ix++) {
        sprintf(serbuf, pullp(PSTR("%c")), respget[5 + ix] < 127 && respget[5 + ix] > 31 ? respget[5 + ix] : '.');
        printser(serbuf);
    }
    printser(pullp(PSTR("\r\n")));

    makecmd(cmdsend, cmdslice, 2, REQSAVVYSTATUS, 0, NULL);
    sendcmd(cmdsend, RESPSAVVYSTATUS, respget);
    sprintf(serbuf, pullp(PSTR("SavvyStat: ThreshKPH:%d (unmu ena: throvrd):%d\r\n")), respget[5], respget[6]);
    printser(serbuf);
    makecmd(cmdsend, cmdslice, 2, REQVEHICLESPEED, 0, NULL);
    sendcmd(cmdsend, RESPVEHICLESPEED, respget);
    sprintf(serbuf, pullp(PSTR("SavvyVehSpd: %d kph\r\n")), respget[5]);
    printser(serbuf);
    printser(pullp(PSTR("=====END INFOSCAN=====\r\n")));
}

// Ask for and Display (decoded) alerts
static const unsigned char typ[] = "LAKXU^-v", t;
static void alerts() {
    int ix;
    syncresp();
    printser(pullp(PSTR("=====ALERTS=====\r\n")));
    makecmd(cmdsend, cmdslice, 0xa, REQSTARTALERTDATA, 0, NULL);
    sendcmd(cmdsend, NORESPONSE, respget);
    for (;;) {
        if( keypress() ) {
            getkey();
            break;
        }
        printser(pullp(PSTR("===\r\n")));
        ix = v1alerts;
        while (ix == v1alerts)
            readpkt(respget);
        for (ix = 0; ix < (v1alertout[0][0] & 15); ix++) {
            unsigned char *b = v1alertout[ix], t;
            sprintf(serbuf, pullp(PSTR("%2d/%2d %5u %3d ^v %3d ")), b[0] >> 4, b[0] & 15, b[1] << 8 | b[2], b[3], b[4]);
            printser(serbuf);
            for (t = 0; t < 8; t++)
                if ((b[5] >> t) & 1) {
                    sprintf(serbuf, pullp(PSTR("%c")), typ[t]);
                    printser(serbuf);
                }
            if (b[6] & 0x80)
                printser(pullp(PSTR("!")));
            printser(pullp(PSTR("\r\n")));
        }
    }
    makecmd(cmdsend, cmdslice, 0xa, REQSTOPALERTDATA, 0, NULL);
    sendcmd(cmdsend, NORESPONSE, respget);
    printser(pullp(PSTR("=====END ALERTS=====\r\n")));
}

// convert 7 segment to ASCII or something close
static const prog_char sevs2ascii[] PROGMEM = {
" ~'...17_...j..]........l...uvJ.`\".^............|.......LC...GU0-.......=#.....3r./.....c..2o.d.....\\.4......5y9.F.Ph.HAtE..b6.8"
};
//bit 0-7: Mute, TSHold, SysUp, DispOn, Euro, Custom, -, -
static const unsigned char inf2[] = "MHUDEC-=";
static void showinfdisp() {
    int ix;
    sprintf(serbuf, pullp(PSTR("%c%c %02x %02x ")), pgm_read_byte(sevs2ascii + (respget[5] & 0x7f)), respget[5] & 0x80 ? 'o' : ' ',
      respget[5], respget[6] ^ respget[5]);
    printser(serbuf);
    for (ix = 0; ix < 8; ix++) {
        sprintf(serbuf, pullp(PSTR("%c")), (respget[7] >> ix) & 1 ? '*' : '.');
        printser(serbuf);
    }
    printser(" ");

    for (ix = 0; ix < 8; ix++)
        if ((respget[8] >> ix) & 1) {
            sprintf(serbuf, pullp(PSTR("%c")), typ[ix]);
            printser(serbuf);
        }
        else
            printser("_");
    printser(" ");

    for (ix = 0; ix < 8; ix++)
        if (((respget[8] ^ respget[9]) >> ix) & 1) {
            sprintf(serbuf, pullp(PSTR("%c")), typ[ix]);
            printser(serbuf);
        }
        else
            printser("_");
    printser(" ");

    for (ix = 0; ix < 8; ix++)
        if ((respget[10] >> ix) & 1) {
            sprintf(serbuf, pullp(PSTR("%c")), inf2[ix]);
            printser(serbuf);
        }
        else
            printser("_");
    printser(pullp(PSTR("\r\n")));
}

// This setup can be used for Savvy override and unmute setting, by changing the command
// set scan mode
// 1=AllBogeys, 2=Logic, 3=AdvancedLogic
static void setmode(unsigned char mode) {
    syncresp();
    makecmd(cmdsend, cmdslice, 0xa, REQCHANGEMODE, 1, &mode);
    sendcmd(cmdsend, NORESPONSE, respget);
}

static void savvyunmute(unsigned char enable) {
    syncresp();
    enable = ! !enable;         // make 0 or 1
    makecmd(cmdsend, cmdslice, 0xa, REQSETSAVVYUNMUTEENABLE, 1, &enable);
    sendcmd(cmdsend, NORESPONSE, respget);
}

// 0 - never, 0xff - always, else kph.
static void savvyoverride(unsigned char speed) {
    syncresp();
    makecmd(cmdsend, cmdslice, 0xa, REQOVERRIDETHUMBWHEEL, 1, &speed);
    sendcmd(cmdsend, NORESPONSE, respget);
}

// no param command without response
static void quickcommand(unsigned char cmd) {
    syncresp();
    makecmd(cmdsend, cmdslice, 0xa, cmd, 0, NULL);
    sendcmd(cmdsend, NORESPONSE, respget);
}

// prototype - need to add set/reset dialog and edit the buffer
static void userbytes() {
    syncresp();
    printser(pullp(PSTR("Current:\r\n")));
    usershow();
    printser(pullp(PSTR("Use one of the above letters/numbers to change,\r\n" " or Q to quit, W to write changes to the V1\r\n")));
    unsigned char ub[6];
    memcpy(ub, respget + 5, 6);
    for (;;) {
        char c = getkey();
        if (c <= ' ')
            continue;
        if (c == 'Q' || c == 'q')
            return;
        if (c == 'w' || c == 'w')
            break;
        int ix;
        for (ix = 0; ix < 24; ix++)
            if (userset[ix] == c)
                break;
        if (ix < 24)
            ub[ix >> 3] ^= 1 << (ix & 7);
        memcpy(respget + 5, ub, 6);
        userprint();
    }
    memcpy(respget + 5, ub, 6);
    userprint();

    printser(pullp(PSTR("Updating\r\n")));
    makecmd(cmdsend, cmdslice, 0xa, REQWRITEUSERBYTES, 6, ub);
    sendcmd(cmdsend, NORESPONSE, respget);
    printser(pullp(PSTR("New:\r\n")));
    usershow();
}

extern void hwsetup(void);
extern volatile unsigned legacy;
/*========================================================================*/
#ifdef STANDALONE
int main()
#else
void init()
#endif
{
    hwsetup();

    int ret;
    unsigned char lastdisp[12];

    printser(pullp(PSTR("V1MegaTool\r\n")));
  
    for (;;) {
        for (;;) {              // get at least one inf packet
            ret = readpkt(respget);
            if (ret < 5)
                continue;
            if (respget[3] == INFDISPLAYDATA)
                break;
        }
        // should give Not Ready message if timecmdslice holdoff.
        if(legacy > 32)
            printser(pullp(PSTR("LEGACY! only V works\r\n")));  
        printser(pullp(PSTR("A-alerts, I-infoscan, D-DefaultSweep, S-SetSweeps. T-transparent, U-userbytes V-ViewDisplay\r\n")));
        printser(pullp(PSTR("#-FactDefault 1-DispOff 2-DispOn 3-MuteOn 4-Muteoff 5-AllBogeys 6-Logic 7-Advanced-Logic\r\n")));
        char c = getkey();
        switch (c) {
        case '#':
            quickcommand(REQFACTORYDEFAULT);
            break;
        case '1':
            quickcommand(REQTURNOFFMAINDISPLAY);
            break;
        case '2':
            quickcommand(REQTURNONMAINDISPLAY);
            break;
        case '3':
            quickcommand(REQMUTEON);
            break;
        case '4':
            quickcommand(REQMUTEOFF);
            break;
        case '5':
            setmode(1);         // all bogeys
            break;
        case '6':
            setmode(2);         // logic
            break;
        case '7':
            setmode(3);         // advanced logic
            break;
        case 'A':
        case 'a':
            alerts();
            break;
        case 'U':
        case 'u':
            userbytes();
            break;
        case 'I':
        case 'i':
            infoscan();
            break;
        case 'S':
        case 's':
            sweepset();
            break;
        case 'D':
        case 'd':
            defaultsweeps();
            break;
        case 'T':
        case 't':
            transp = 1;         // act like bluetooth port 
            while (transp)
                sleep_mode();
            break;
        case 'V':
        case 'v':
            printser(pullp(PSTR("Mute (ESP)Hold systemUp mainDisp Euro Custom\r\n")));
            lastdisp[0] = 0;
            while (!keypress()) {
                ret = readpkt(respget);
                if (ret < 5)
                    continue;
                if (respget[3] == INFDISPLAYDATA && memcmp(lastdisp, respget, 12)) {
                    showinfdisp();
                    memcpy(lastdisp, respget, 12);
                }
            }
            c = getkey();
            break;
        default:
            break;
        }
    }
}



