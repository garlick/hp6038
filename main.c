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
// HS or XT (<= 4MHZ) 
__CONFIG (HS & WDTDIS & PWRTEN & BORDIS & LVPDIS & DUNPROT
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

#define HP_DA	    	RC2	/* pin 8 (0=address, 1=data) */
#define HP_IOCLOCK	RC3	/* pin 5 (0=HP_DATADOWN valid) */
#define HP_DATADOWN	RC4	/* pin 2 (data read by pic) */
#define HP_DATAUP	RC5	/* pin 11 (data written by pic) */
#define HP_PCLR		RC6	/* pin 16 (active low) */


static unsigned char data[16];


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
#if 0
        case 0x7f: c = 'C'; break;
        case 0x7f: c = 'H'; break;
        case 0x7f: c = 'L'; break;
#endif
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

    sprintf (s, "%.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x",
        data[0x0], data[0x1], data[0x2], data[0x3],
        data[0x4], data[0x5], data[0x6], data[0x7]);
    lcd_goto (0);
    lcd_puts (s);

    sprintf (s, "%.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x",
        data[0x8], data[0x9], data[0xa], data[0xb],
        data[0xc], data[0xd], data[0xe], data[0xf]);
    lcd_goto (0x40);
    lcd_puts (s);
}

static void interrupt
isr (void)
{
    int is_address = !HP_DA; /* read it fast - timing is tight! */
    static unsigned char addr = 0;
    char b;

    if (SSPIF) {
        b = ~SSPBUF;	/* read SSPBUF no matter what (else overflow) */
        if (is_address) {
            addr = b;
	} else {
            if (addr & 0x10)
                data[addr & 0x0f] = b;
        }
        SSPIF = 0;
    }
}

void
main(void)
{
    ADCON1 = 0x06;

    TRISA = 0;
    TRISB = 0;
    TRISC = 0b01011100; /* RC2, RC3, RC4, RC6 are inputs */

    lcd_init ();

    memset (data, 0, sizeof (data));     

    while (!HP_PCLR)    /* wait for p/s to come out of reset */
        ;

    /* SSPCON */
    SSPEN = 0;          /* disable SSP (clears shift reg) */
    SSPM3=0; SSPM2=1; SSPM1=0; SSPM0 = 1; /* select slave mode w/o SS */
    CKP = 1;            /* clock polarity: idle state is H */

    /* SSPSTAT */
    CKE = 0;            /* data transmitted on the rising edge of SCK */

    SSPEN = 1;          /* enable SSP */

    /* Interrupts */
    SSPIF = 0;          /* clear SSP interrupt flag */
    SSPIE = 1;          /* enable SSP interrupt */
    PEIE = 1;           /* enable peripheral interupts */
    GIE = 1;            /* enable global interrupts */

    for (;;) {
        //update_display ();
        update_display_raw ();
        __delay_ms (100);
        __delay_ms (100);
        __delay_ms (100);
        __delay_ms (100);
    }
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

