#include <htc.h>
#include <string.h>
#include "lcd.h"

#if defined(_16F873A)
// must turn off LVP to make RB3 an I/O port
__CONFIG (LVPDIS);
#endif

/* Mode indicators:
 * Address    Bit:Indicator
 * 0x10 CS0   0:CV 1:CC 2:OVERRANGE 3:DISABLE, 4:OV, 5:OT, 6:FOLDBACK, 7:ERROR
 * 0x11 CS1   0:FOLD_EN 1:CURRENT 2:VOLTAGE 3: RMT: 4:LSN 5:TLK 6:SRQ
 */

/* RPG/buttons:
 * CS2
 */

/* Current display (0 = lighted segment)
 * Address    Bit:Segment
 * 0x18 CS8   DS1 0:a 1:b 2:c 3:d 4:e 5:f 6:g 7:DP
 * 0x19 CS9   DS2 0:a 1:b 2:c 3:d 4:e 5:f 6:g 7:DP
 * 0x1A CS10  DS3 0:a 1:b 2:c 3:d 4:e 5:f 6:g 7:DP
 * 0x1B CS11  DS4 0:e 1:d 2:c 3:b 4:a
 */

/* Voltage display (0 = lighted segment)
 * Address    Bit:Segment
 * 0x1C CS12  DS5 0:a 1:b 2:c 3:d 4:e 5:f 6:g 7:DP
 * 0x1D CS13  DS6 0:a 1:b 2:c 3:d 4:e 5:f 6:g 7:DP
 * 0x1E CS14  DS7 0:a 1:b 2:c 3:d 4:e 5:f 6:g 7:DP
 * 0x1F CS15  DS8 0:e 1:d 2:c 3:b 4:a
 */

#define IO_CLOCK	RC0	/* pin 5 (0=data_down valid) */
#define DATA_ADDR	RC1	/* pin 8 (0=address, 1=data) */
#define DATA_DOWN	RC2	/* pin 2 (data read by pic) */
#define PCLR		RC3	/* pin 16 (active low) */

#define DATA_UP		RC4	/* pin 11 (data written by pic) */


void
catchar (char *s, char c)
{
    int i = 0;

    while (s[i] != '\0')
        i++;
    s[i++] = c;
    s[i] = '\0';
}

/* Decode seven segment LED: decimal pt., digit
 */
void
sseg2char (unsigned char seg, char *s)
{
    char c;

    if (((~seg) >> 7) == 1)
	    catchar (s, '.');
    switch ((~seg) & 0x7f) {
        case 0x3f: c = '0'; break;
        case 0x06: c = '1'; break;
        case 0x5b: c = '2'; break;
        case 0x4f: c = '3'; break;
        case 0x66: c = '4'; break;
        case 0x6d: c = '5'; break;
        case 0x7c: c = '6'; break;
        case 0x07: c = '7'; break;
        case 0x7f: c = '8'; break;
        default:   c = 'x'; break;
    }
    catchar (s, c);
}

/* Decode five segment LED: sign, digit
 */
void
fseg2char (unsigned char seg, char *s)
{
    char c;

    switch ((~(seg) >> 2) & 7) {
        case 1:    c = '-'; break;
        case 6:    c = '1'; break;
        case 7:    c = '+'; break;
        default:   c = 'x'; break;
    }
    catchar (s, c);
    switch (((~seg) & 3)) {
        case 3:    c = '1'; break;
        default:   c = 'x'; break;
    }
    catchar (s, c);
}

void
update_display (unsigned char *data)
{
    char s[21];

    *s = '\0';
    fseg2char (data[0xf], s);
    sseg2char (data[0xe], s);
    sseg2char (data[0xd], s);
    sseg2char (data[0xc], s);
    strcat (s, "V ");
    fseg2char (data[0xb], s);
    sseg2char (data[0xa], s);
    sseg2char (data[0x9], s);
    sseg2char (data[0x8], s);
    strcat (s, "A ");

    lcd_goto (0);	// select first line
    lcd_puts (s);
}

void
shiftin (unsigned char *cp, unsigned char val)
{
    if (val)
        *cp = (*cp << 1) | 0x01;
    else
        *cp = (*cp << 1) & 0xfe;
}


void
main(void)
{
    unsigned char c;
    unsigned char clock;
    unsigned char data[16];
    unsigned char address;

    TRISA = 0;
    TRISB = 0;
    TRISC = 0x0f;               /* RC3:0 are inputs */

    lcd_init ();

    memset (&data, 0, sizeof (data));
    clock = 0;
    address = 0;

    lcd_goto (0);
    lcd_puts ("I am");
    lcd_goto (0x40);
    lcd_puts ("alive...");

    for (;;) {
        c = PORTC;
#if 0
        if (!((c >> 3) & 1)) {  /* PCLR active */
            memset (&data, 0, sizeof (data));
            clock = 0;
            address = 0;
        }
#endif
        if ((c & 1)) {          /* IO_CLOCK inactive */
            if (clock != 0)
                update_display (data); /* gack!  this is too slow */
            clock = 0;
        } else if (clock == 0) {/* IO_CLOCK transition to active */
            clock = 1;
            if ((c >> 1) & 1) { /* DATA_ADDR == 1 (data) */
                shiftin (&data[address & 0xf], (c >> 2) & 1);
            } else {            /* DATA_ADDR == 0 (address) */
                shiftin (&address, (c >> 2) & 1);
            }
        }
    }
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

