#ifndef _XTAL_FREQ
 // Unless specified elsewhere, 4MHz system frequency is assumed
 #define _XTAL_FREQ 4000000
#endif

#include <htc.h>
#include <string.h>
#include <stdio.h>
#include "lcd.h"
#include "delay.h"

#if defined(_16F873A)
// must turn off LVP to make RB3 an I/O port
__CONFIG (XT & WDTDIS & PWRTEN & BORDIS & LVPDIS & DUNPROT
             & WRTEN & DEBUGDIS & UNPROTECT);
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

#define HP_IOCLOCK	RB0	/* pin 5 (0=data_down valid) */
#define HP_DA	    RB1	/* pin 8 (0=address, 1=data) */
#define HP_DATADOWN	RB2	/* pin 2 (data read by pic) */
#define HP_PCLR		RB3	/* pin 16 (active low) */

#define HP_DATAUP	RB4	/* pin 11 (data written by pic) */

static unsigned char data[16];
static unsigned char addr;

static unsigned char indata;
static unsigned char indata_bits;

static unsigned char inaddr;

/* Append character to string. 
 */
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

/* Update the LCD display
 */
void
update_display ()
{
    char s[21];

    *s = '\0';
    fseg2char (data[0xf], s);
    sseg2char (data[0xe], s);
    sseg2char (data[0xd], s);
    sseg2char (data[0xc], s);
    strcat (s, "V   ");
    fseg2char (data[0xb], s);
    sseg2char (data[0xa], s);
    sseg2char (data[0x9], s);
    sseg2char (data[0x8], s);
    strcat (s, "A");

    lcd_goto (0);	// select first line
    lcd_puts (s);
}

void
update_display_raw ()
{
    char s[21];

    sprintf (s, "%2x%2x%2x%2x %2x%2x%2x%2x",
        data[0xf],
        data[0xe],
        data[0xd],
        data[0xc],
        data[0xb],
        data[0xa],
        data[0x9],
        data[0x8]);
    lcd_goto (0x40);
    lcd_puts (s);
}

pclr (void)
{
    memset (&data, 0, sizeof (data));
    addr = inaddr = indata = 0;
    indata_bits = 0;
}

void
shift_in (unsigned char *cp, unsigned char val)
{
    if (val)
        *cp = (*cp << 1) | 0x01;
    else
        *cp = (*cp << 1) & 0xfe;
}


static void interrupt
isr (void)
{
    if (INTF) {
        RC7 = 1;
        if (HP_DA) {    /* data */
            if (addr == 2) {
                /* outdata */
            } else {
                indata = (indata << 1) & 0xfe; 
                indata |= (~HP_DATADOWN) & 1;
                if (++indata_bits == 8) {
                    data[addr] = indata;
                    indata_bits = 0;
                }
            }
        } else {        /* addr */
            inaddr = (inaddr << 1) & 0xfe;
            inaddr |= (~HP_DATADOWN) & 1;
            if (inaddr & 0x10) {
                addr = inaddr & 0xf;
                inaddr = 0;
            }
        }
        INTF = 0;
    }
}

void
main(void)
{
    ADCON1 = 0x06;

    TRISA = 0;
    TRISB = 0x0f; /* RB3:0 inputs */
    TRISC = 0;

    lcd_init ();

    pclr ();
    INTF = 0;
    INTEDG = 0;
    INTE = 1;
    GIE = 1;

    for (;;) {
        RC6 = 1;
        update_display ();
        update_display_raw ();
        __delay_ms (197);
        __delay_ms (197);
        RC5 = 0;
        RC6 = 0;
        RC7 = 0;
    }
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

