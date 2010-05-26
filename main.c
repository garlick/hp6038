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

static char ds[8];
enum { DS1=0, DS2=1, DS3=2, DS4=3, DS5=4, DS6=5, DS7=6, DS8=7 };

void
catchar (char *s, char c)
{
    int l = strlen (s);

    s[l - 1] = c;
    s[l] = '\0';
}

/* Decode seven segment LED: decimal, digit
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
        default:   c = ' '; break;
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
        default:   c = ' '; break;
    }
    catchar (s, c);
    switch (((~seg) & 3)) {
        case 3:    c = '1'; break;
        default:   c = ' '; break;
    }
    catchar (s, c);
}

void
update_lcd (void)
{
    char s[20];

    *s = '\0';
    fseg2char (ds[DS8], s);
    sseg2char (ds[DS7], s);
    sseg2char (ds[DS6], s);
    sseg2char (ds[DS5], s);
    strcat (s, "volts ");
    fseg2char (ds[DS4], s);
    sseg2char (ds[DS3], s);
    sseg2char (ds[DS2], s);
    sseg2char (ds[DS1], s);
    strcat (s, "amps");

    lcd_goto (0);	// select first line
    lcd_puts (s);
}

int
pollit (void)
{
    unsigned char addr = 0;
    char addr_bits = 0;
    unsigned char data = 0;
    char data_bits = 0;
    int res = 0;

    if (!IO_CLOCK && !DATA_ADDR) { /* addr */
        lcd_goto (0x40); lcd_puts ("*");
        if (addr_bits == 8)
            addr_bits = 0;
        addr_bits = 0;
        addr & (DATA_DOWN << addr_bits++);
    }
    if (!IO_CLOCK && DATA_ADDR) { /* data */
        lcd_goto (0x41); lcd_puts ("*");
        if (data_bits == 8)
            data_bits = 0;
        data & (DATA_DOWN << data_bits++);
    }
    while (!IO_CLOCK)
        ;
    if (addr_bits == 8 && data_bits == 8) {
        switch (addr) {
            case 0x18: /* DS1 */
            case 0x19: /* DS2 */
            case 0x1a: /* DS3 */
            case 0x1b: /* DS4 */
            case 0x1c: /* DS5 */
            case 0x1d: /* DS6 */
            case 0x1e: /* DS7 */
            case 0x1f: /* DS8 */
                ds[addr - 0x18 - 1] = data;
                res = 1;
                break;
        }
    }
    return res;
}


void
main(void)
{
    TRISA = 0;
    TRISB = 0;
    TRISC = 0x0f; /* RC3:0 are inputs */
    lcd_init ();
    lcd_goto (0);	// select first line
    lcd_puts ("hello world");
    for (;;) {
        if (pollit())
            update_lcd();
    }
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

