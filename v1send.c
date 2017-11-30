#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SLOT (4)

unsigned char cmdbuf[24] = "\xaa\xda\xe0\x00\x01\xcc\xab";
unsigned char chmbuf[24] = "\xaa\xda\xe0\x36\x02\x00\xcc\xab";
unsigned char sumbuf[24] = "\xaa\xda\xe0\x76\x02\x00\xcc\xab";

unsigned char optable[] = { 0x00,
			    0x01, 0x03, 0x11, 0x16, 0x18, 0x19, 0x22, 0x32, 0x33, 0x34, 0x35, 0x41, 0x42, 0x62, 0x71, 0x73, 0x36, 0x36, 0x36,1,1,1,3 
};

// Requests not included in the above

// + Request Version for display, audio, savvy - when they arrive, dest = 0,1,2.
// + Request SerialNum for Savvy, dest = 2 - when I get my Savvy 
// Factory Reset 0x17 - no bytes (I don't want to reset my V1 right now)
// Savvy Unmute Enable 0x76 - 0-disable unmuting 1-enable unmuting (When my Savvy arrives)

// Write User Bytes - 0x13 - 6 bytes
// Write Sweep Definition - 5 bytes, commit/index, HighMHz MSB, LSB, LowMHz M, LSB
// Savvy Override Thumbwheel 0x75 - 1 byte, 0-never mute, 0xff-mute at all, else KPH to mute at (MPH*1.6093)

int main(int argc, char *argv[])
{
    int op = -1;
    if (argc > 1)
        op = atoi(argv[1]);
    if (op < 1 || op > sizeof(optable)) {
        printf(
	       "1 - Version\n"
	       "2 - SerNo\n"
	       "3 - UserBytes\n"
	       "4 - SweepDefs\n"
	       "5 - DefSweeps\n"
	       "6 - MaxSwpIdx\n"
	       "7 - SweepSects\n"
	       "8 - MainDispOFF\n"
	       "9 - MainDispON\n"
	       "10 - MuteON\n"
	       "11 - MuteOFF\n"
	       "12 - AlertDatON\n"
	       "13 - AlertDatOFF\n"
	       "14 - BattVolt\n" 
	       "15 - SavvyStat\n" 
	       "16 - SavvyVehSPd\n" 
	       "17 - AllBogeys\n" 
	       "18 - Logic\n" 
	       "19 - AdvancedLogic\n"
	       "20 - CDVersion\n"
	       "21 - RAVersion\n"
	       "22 - SaVersion\n"
	       "23 - SaSerNo\n"
	       );
        return -1;
    }

    if (optable[op] != 0x36)
        cmdbuf[3] = optable[op];
    else {
        memcpy(cmdbuf, chmbuf, 10);
        cmdbuf[5] = op - 16;
    }
    cmdbuf[2] += SLOT;

    if( op >= 20 )
	cmdbuf[1] = 0xd0 + op - 20;

    if( op > 22 || cmdbuf[3] > 0x6f)
	cmdbuf[1] = 0xd2;

    unsigned char ix, cks = 0;
    for (ix = 0; ix < cmdbuf[4] + 4; ix++)
        cks += cmdbuf[ix];
    cmdbuf[ix] = cks;

    FILE *fp = fopen("/dev/ttyUSB0", "wb");
    if( !fp )
	fp = fopen("/dev/rfcomm0", "wb");
    if( fp ) {
	fwrite(cmdbuf, cmdbuf[4] + 6, 1, fp);
	fclose(fp);
    }

    printf( "{" );
    for (ix = 0; ix < cmdbuf[4] + 6; ix++)
        printf( " 0x%02x,", cmdbuf[ix] );
    printf( "}\n");


    return 0;
}
