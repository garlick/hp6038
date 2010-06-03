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

#define HP_DA	    	RC2	/* pin 8 (0=address, 1=data) */
#define HP_IOCLOCK	RC3	/* pin 5 (0=HP_DATADOWN valid) */
#define HP_DATADOWN	RC4	/* pin 2 (data read by pic) */
#define HP_DATAUP	RC5	/* pin 11 (data written by pic) */
#define HP_PCLR		RC6	/* pin 16 (active low) */

#define MODE_CV		0x01
#define MODE_CC		0x02
#define MODE_OVERRANGE	0x04
#define MODE_DISABLED  	0x08
#define MODE_OV         0x10
#define MODE_OT         0x20
#define MODE_FOLDBACK   0x40
#define MODE_ERROR      0x80

#define MODE_FOLDBACK_EN 0x01
#define MODE_CURRENT    0x02
#define MODE_VOLTAGE    0x04
#define MODE_RMT        0x08
#define MODE_LSN        0x10
#define MODE_TLK        0x20
#define MODE_SRQ        0x40

static unsigned char data[16];

#ifndef RAW_DISPLAY
const char *
seg7str (unsigned char c)
{
    switch (c) {
        case 0x41: return "0";
	case 0x7d: return "1";
	case 0x62: return "2";
	case 0x68: return "3";
	case 0x5c: return "4";
	case 0xc8: return "5";
	case 0xc0: return "6";
	case 0x6d: return "7";
	case 0x40: return "8";
	case 0x48: return "9";
	case 0x01: return ".0";
	case 0x3d: return ".1";
	case 0x22: return ".2";
	case 0x28: return ".3";
	case 0x1c: return ".4";
	case 0x88: return ".5";
	case 0x80: return ".6";
	case 0x2d: return ".7";
	case 0x00: return ".8";
	case 0x08: return ".9";
	case 0xc3: return "C";
	case 0x55: return "H";
	case 0xd3: return "L";
	case 0xff: return " ";
	default:   return "*";
    }
}

const char *
seg5str (unsigned char c)
{
    switch (c) {
	case 0xe6: return "+";
	case 0xef: return "-";
	case 0xf9: return "1";
	case 0xff: return " ";
	default:   return "*";
    }
}

/* Update the LCD display
 */
void
update_display (void)
{
    char s[21];
    unsigned char d = ~data[0];
    unsigned char e = ~data[1];

    if (data[0xe] == 0xff && data[0xd] == 0xff && data[0xc] == 0xff) {
        sprintf (s, "===== HP 6038A =====");

    } else if (data[0xe] == 0xc3 && data[0xd] == 0x55 && data[0xc] == 0xd3) {
        sprintf (s, "GP-IB channel %s%s",
                 seg7str (data[0x9]), seg7str (data[0x8]));

    } else if (data[0x8] == 0xff && data[0x9] == 0xff && data[0xa] == 0xff) {
        sprintf (s, "OVP %s%s%s%sV",
                 seg5str (data[0xf]), seg7str (data[0xe]),
                 seg7str (data[0xd]), seg7str (data[0xc]));

    } else {
        sprintf (s, "%s%s%s%s%c %s%s%s%s%c",
                 seg5str (data[0xf]), seg7str (data[0xe]),
                 seg7str (data[0xd]), seg7str (data[0xc]),
                 (e & MODE_VOLTAGE) ? 'V' : 'v',
                 seg5str (data[0xb]), seg7str (data[0xa]),
                 seg7str (data[0x9]), seg7str (data[0x8]),
                 (e & MODE_CURRENT) ? 'A' : 'a');
    }
    if (!strncmp (s, " .", 2))
        s[0] = '0';
    while (strlen (s) < 20)
        strcat (s, " ");

    lcd_goto (0);
    lcd_puts (s);

    sprintf (s, "%s %s %s  %s",
             (d & MODE_CV) ? "cv" : "  ",
             (d & MODE_CC) ? "cc" : "  ",
             (e & MODE_FOLDBACK_EN) ? "fb" : "  ",
             (d & MODE_OV)                                    ? " OVERVOLT!" 
                                       : (d & MODE_OT)        ? " OVERTEMP!"
                                       : (d & MODE_OVERRANGE) ? "OVERRANGE!"
                                       : (d & MODE_DISABLED)  ? " DISABLED!"
                                       : (d & MODE_FOLDBACK)  ? " FOLDBACK!"
                                       : (d & MODE_ERROR)     ? "    ERROR!"
                                       :                        "          ");
    lcd_goto (0x40);
    lcd_puts (s);

}
#else
void
update_display (void)
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
#endif

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

    memset (data, 0xff, sizeof (data));     

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
        update_display ();
        __delay_ms (100);
        __delay_ms (100);
        __delay_ms (100);
        __delay_ms (100);
    }
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

