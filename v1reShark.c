#include <stdio.h>
#include <stdlib.h>

char sevs2ascii[] = {
    ' ', '~', '.', '.', '.', '.', '1', '7',
    '_', '.', '.', '.', 'j', '.', '.', ']',
    '.', '.', '.', '.', '.', '.', '.', '.',
    'l', '.', '.', '.', 'u', 'v', 'J', '.',

    '.', '.', '.', '^', '.', '.', '.', '.',
    '.', '.', '.', '.', '.', '.', '.', '.',
    '|', '.', '.', '.', '.', '.', '.', '.',
    'L', 'C', '.', '.', '.', 'G', 'U', '0',

    '-', '.', '.', '.', '.', '.', '.', '.',
    '=', '#', '.', '.', '.', '.', '.', '3',
    'r', '.', '/', '.', '.', '.', '.', '.',
    'c', '.', '.', '2', 'o', '.', 'd', '.',

    '.', '.', '.', '.', '\\', '.', '4', '.',
    '.', '.', '.', '.', '.', '5', 'y', '9',
    '.', 'F', '.', 'P', 'h', '.', 'H', 'A',
    't', 'E', '.', '.', 'b', '6', '.', '8'
};

char userset[] = "12345678AbCdEFGHJuUtL   ";

// 12345678AbCdEFGHIJuXtL-- ------------------------
// byte 0, bit 0 - 7, byte 1, etc.

FILE *fp = NULL;

unsigned char nochecksum = 0;

int readpkt(unsigned char *buf)
{
    unsigned char len, ix;
    int ret;
    buf[0] = 0;
    for (;;) {
        while (buf[0] != 0xaa)  // SOF
            while (1 != (ret = fread(&buf[0], 1, 1, fp)))
                if (ret < 0)
                    return ret;
        while (1 != fread(&buf[1], 1, 1, fp));  // target
        if ((buf[1] & 0xf0) != 0xd0)
            continue;
        while (1 != fread(&buf[2], 1, 1, fp));  // source
        if ((buf[2] & 0xf0) != 0xe0)
            continue;
        if ((buf[2] & 15) == 0xa)
            nochecksum = 0;
        else if ((buf[2] & 15) == 9)
            nochecksum = 1;
        while (1 != fread(&buf[3], 1, 1, fp));  // packet id
        while (1 != fread(&buf[4], 1, 1, fp));  // length
        if (buf[4] > 16)
            continue;

        len = 5;
        for (ix = 0; ix < buf[4]; ix++) {
            while (1 != fread(&buf[len], 1, 1, fp));
            len++;
        }
        if (!nochecksum) {
            unsigned char cks = 0;
            for (ix = 0; ix < len - 1; ix++)
                cks += buf[ix];
            if (buf[len - 1] != (cks & 0xff))
                return -2;      // continue; ???
        }
        len++;
        while (1 != fread(&buf[len], 1, 1, fp));
        return len;
    }
}

int main(int argc, char *argv[])
{
    unsigned char buf[24], ix;
    int ret;
    fp = fopen("/dev/ttyUSB0", "rb");
    if (!fp)
        fp = fopen("/dev/rfcomm0", "rb");
    if (!fp)
        return -1;
    for (;;) {

        ret = readpkt(buf);
        if (ret < 5)
            continue;

        printf("%x %x ", buf[1] & 15, buf[2] & 15);

        switch (buf[3]) {
        case 2:
            printf("Version: ");
            for (ix = 0; ix < buf[4] - 1; ix++)
                printf("%c", buf[5 + ix] < 127 && buf[5 + ix] > 31 ? buf[5 + ix] : '.');
            printf("\n");
            break;
        case 4:
            printf("SerialNo: ");
            for (ix = 0; ix < buf[4] - 1; ix++)
                printf("%c", buf[5 + ix] < 127 && buf[5 + ix] > 31 ? buf[5 + ix] : '.');
            printf("\n");
            break;

        case 0x12:
            printf("\nUserSet: (default)");
            for (ix = 0; ix < 8; ix++)
                printf("%c", (buf[5] >> ix) & 1 ? userset[ix] : '_');
            for (ix = 0; ix < 8; ix++)
                printf("%c", (buf[6] >> ix) & 1 ? userset[ix + 8] : '_');
            for (ix = 0; ix < 8; ix++)
                printf("%c", (buf[7] >> ix) & 1 ? userset[ix + 16] : '_');
            printf("\nUserSet: (changed)");
            for (ix = 0; ix < 8; ix++)
                printf("%c", (buf[5] >> ix) & 1 ? '_' : userset[ix]);
            for (ix = 0; ix < 8; ix++)
                printf("%c", (buf[6] >> ix) & 1 ? '_' : userset[ix + 8]);
            for (ix = 0; ix < 8; ix++)
                printf("%c", (buf[7] >> ix) & 1 ? '_' : userset[ix + 16]);
            printf("\n");
            break;

        case 0x17:
            printf("SweepDef: %d Top:%5d Bot:%5d\n", buf[5], buf[6] << 8 | buf[7], buf[8] << 8 | buf[9]);
            break;
        case 0x20:
            printf("SweepMax: %d\n", buf[5]);
            break;
        case 0x21:
            printf("SweepWriteResult: %d\n", buf[5]);
            break;
        case 0x23:
            printf("SweepSct:\n");
            printf("+%d/%d %5d - %5d\n", buf[5] >> 4, buf[5] & 15, buf[8] << 8 | buf[9], buf[6] << 8 | buf[7]);
            if (buf[4] > 6)
                printf("+%d/%d %5d -:%5d\n", buf[10] >> 4, buf[10] & 15, buf[13] << 8 | buf[14], buf[11] << 8 | buf[12]);
            if (buf[4] > 11)
                printf("+%d/%d %5d - %5d\n", buf[15] >> 4, buf[15] & 15, buf[18] << 8 | buf[19], buf[16] << 8 | buf[17]);
            break;
        case 0x31:
            printf("Disp: %c%c %02x %02x ", sevs2ascii[buf[5] & 0x7f], buf[5] & 0x80 ? 'o' : ' ', buf[5], buf[6] ^ buf[5]);
            for (ix = 0; ix < 8; ix++)
                printf("%c", (buf[7] >> ix) & 1 ? '*' : '.');

            //bit 0-7: Laser, Ka, K, X, -, Front, Side, Rear
            printf(" %02x %02x", buf[8], buf[9] ^ buf[8]);
            //bit 0-7: Mute, TSHold, SysUp, DispOn, Euro, Custom, -, -
            printf(" %02x\n", buf[10]);
            break;
        case 0x43:
            printf("Alert: %d/%d %5d %3d^ %3dv %02x %02x\n", buf[5] >> 4, buf[5] & 15, buf[6] << 8 | buf[7], buf[8], buf[9],
              buf[10], buf[11]);
            break;
        case 0x61:
            printf("Data Rcvd\n");
            break;
        case 0x63:
            printf("BattVolt: %d.%02d\n", buf[5], buf[6]);
            break;
        case 0x64:
            printf("Unsupported Packet\n");
            break;
        case 0x65:
            printf("Request Not Processed %02x\n", buf[5]);
            break;
        case 0x66:
            printf("V1Busy:");
            for (ix = 0; ix < buf[4] - 1; ix++)
                printf(" %02x", buf[5 + ix]);
            printf("\n");
            break;
        case 0x67:
            printf("Data Error %02x\n", buf[5]);
            break;
        case 0x72:
            printf("SavvyStat: ThreshKPH:%d (unmu ena: throvrd):%d\n", buf[5], buf[6]);
            break;
        case 0x74:
            printf("SavvyVehSpd: %d kph\n", buf[5]);
            break;
        case 1:                // vers
        case 3:                // serno
        case 0x11:             //user
        case 0x14:             //fac default
        case 0x16:             //allsweeps
        case 0x18:             //defsweeps
        case 0x19:             //maxsweepindex
        case 0x22:             //sweepsects
        case 0x32:             //offMainDisp
        case 0x33:             //onMainDisp
        case 0x34:             //muteOn
        case 0x35:             //muteOff
        case 0x41:             //AlertOn
        case 0x42:             //AlertOff
        case 0x62:             //BattVolt
        case 0x71:             //SavvyStat
        case 0x73:             //SavvyVehSpd
            //      char *pktid[] = { "Version", "SerialNo", "UserSet", "FactoryDefault", "AllSweeps", "DefaultSweeps", "MaxSweepIndex", "SweepSections", "MainDispOff", "MainDispOn", "MuteOn", "MuteOff", "AlertOn", "AlertOff", "BatteryVolt", "SavvyStatus", "SavvyVehicleSpeed" };
            printf("REQ %02x\n", buf[3]);
            break;
        case 0x13:             //WriteUserBytes p6
        case 0x15:             //WriteSweepDef p5
        case 0x36:             //changeMode p1
        default:
            printf("OTHER: %02x:(%d) ", buf[3], buf[4]);
            for (ix = 0; ix < buf[4] - 1; ix++)
                printf(" %02x", buf[5 + ix]);
            printf(" ");
            for (ix = 0; ix < buf[4] - 1; ix++)
                printf("%c", buf[5 + ix] < 127 && buf[5 + ix] > 31 ? buf[5 + ix] : '.');
            printf("\n");
        }

    }

}
