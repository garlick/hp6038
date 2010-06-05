#include <htc.h>
#include <string.h>
#include <stdio.h>
#include "lcd.h"
#include "delay.h"

#if defined(_16F873A)
__CONFIG (HS & WDTDIS & PWRTEN & BORDIS & LVPDIS & DUNPROT
             & WRTEN & DEBUGDIS & UNPROTECT);
#else
#error Please configure chip-specific __CONFIG mask
#endif

/* Define to display 16 register values in hex on the LCD instead
 * of decoded values.
 */
#undef RAW_DISPLAY


/**
 ** PIC parallel port/SPI definitions
 **/

#define __UNUSED1	RA0
#define LCD_EN		RA1
#define LCD_RW		RA2
#define LCD_RS		RA3
#define SW_DISP_SET	RA4
#define SW_FOLDBACK	RA5
#define PORTA_INPUTS	0b00110000

#define LCD_DATA4	RB0
#define LCD_DATA5	RB1
#define LCD_DATA6	RB2
#define LCD_DATA7	RB3
#define __UNUSED2  	RB4	/* RBI */
#define __UNUSED3  	RB5	/* RBI */
#define SW_RPG_A	RB6	/* RBI */
#define SW_RPG_B	RB7	/* RBI */
#define PORTB_INPUTS	0b11000000
#define PORTB_PULLUP	0	/* pullups enabled for RPG */

#define SW_VOLT_CUR 	RC0
#define SW_DISP_OVP	RC1
#define HP_DA	    	RC2
#define HP_IOCLOCK	RC3	/* SPI SCK */
#define HP_DATADOWN	RC4	/* SPI SDI */
#define HP_DATAUP	RC5	/* SPI SDO */
#define HP_PCLR		RC6
#define SW_LOCAL        RC7
#define PORTC_INPUTS	0b11111111 /* N.B. HP_DATAUP is initially tri-state */

/**
 ** HP 6038A shift register bit definitions
 **/

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

#define CONT_RPG_CLOCKW	0x01
#define CONT_RPG_ROTAT	0x02
#define CONT_LOCAL	0x04
#define CONT_VOLT_CUR   0x08
#define CONT_DISP_OVP	0x10
#define CONT_DISP_SET	0x20
#define CONT_FOLDBACK	0x40

/* Software versions of HP shift registers
 * 0x00   mode0   0x08   ds1 (current ones)     0x0c  ds5 (volts ones)
 * 0x01   mode1   0x09   ds2 (current tens)     0x0d  ds6 (volts tens)
 * 0x02   cont    0x0a   ds3 (current hundreds) 0x0e  ds7 (volts hundreds)
 * 0x03-7 unused  0x0b   ds4 (current sign)     0x0f  ds8 (volts sign)
 */
static unsigned char data[16];

/* Address used to index data[].
 */
static unsigned char addr;

/* Initial/previous state of RPG control.
 */
static unsigned char rpg;

#define RPG_INTERRUPT 0
#define POLL_CONTROLS 1

static const char *
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
	/* FIXME: do we need "E" "r" for error display? */
	default:   return "*";
    }
}

static const char *
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

static void
update_display (void)
{
    char s[21];
    unsigned char d = ~data[0];
    unsigned char e = ~data[1];
    char dostatus = 0;

    /* Line 1: +1.000v +1.000a
     *     or: HP 6038 front panel
     *     or OVP +1.000v
     *     or HP-IB channel 05
     */
    if (data[0xe] == 0xff && data[0xd] == 0xff && data[0xc] == 0xff) {
        sprintf (s, "HP 6038A front panel");

    } else if (data[0xe] == 0xc3 && data[0xd] == 0x55 && data[0xc] == 0xd3) {
        sprintf (s, "HP-IB channel %s%s",
                 seg7str (data[0x9]), seg7str (data[0x8]));

    } else if (data[0x8] == 0xff && data[0x9] == 0xff && data[0xa] == 0xff) {
        sprintf (s, "OVP %s%s%s%sV",
                 seg5str (data[0xf]), seg7str (data[0xe]),
                 seg7str (data[0xd]), seg7str (data[0xc]));

        dostatus = 1;
    } else {
        sprintf (s, "%s%s%s%s%c %s%s%s%s%c",
                 seg5str (data[0xf]), seg7str (data[0xe]),
                 seg7str (data[0xd]), seg7str (data[0xc]),
                 (e & MODE_VOLTAGE) ? 'V' : 'v',
                 seg5str (data[0xb]), seg7str (data[0xa]),
                 seg7str (data[0x9]), seg7str (data[0x8]),
                 (e & MODE_CURRENT) ? 'A' : 'a');
        dostatus = 1;
    }
    if (!strncmp (s, " .", 2))
        s[0] = '0';
    while (strlen (s) < 20)
        strcat (s, " ");

    lcd_goto (0);
    lcd_puts (s);

    /* Line 2: cv cc fb  ERROR!
     *     or: (c) 2010 Jim Garlick
     * FIXME: we are only displaying one error at a time.
     */
    if (dostatus) {
        sprintf (s, "%s %s %s  %s",
                 (d & MODE_CV)                ? "cv" : "  ",
                 (d & MODE_CC)                ? "cc" : "  ",
                 (e & MODE_FOLDBACK_EN)       ? "fb" : "  ",
                 (d & MODE_OV)                ? " OVERVOLT!" 
                       : (d & MODE_OT)        ? " OVERTEMP!"
                       : (d & MODE_OVERRANGE) ? "OVERRANGE!"
                       : (d & MODE_DISABLED)  ? " DISABLED!"
                       : (d & MODE_FOLDBACK)  ? " FOLDBACK!"
                       : (d & MODE_ERROR)     ? "    ERROR!"
                       :                        "          ");
    } else {
        sprintf (s, "(c) 2010 Jim Garlick");
    }
    lcd_goto (0x40);
    lcd_puts (s);

}

/* According to service manual, HP polls RPG every 1ms, controls every 10ms.
 * FIXME: call this from a ~5ms  timer interrupt?
 */
static void
update_controls (void)
{
    if (SW_LOCAL == 0)
        data[2] &= ~CONT_LOCAL;
    else
        data[2] |= CONT_LOCAL;

    if (SW_VOLT_CUR == 0)
        data[2] &= ~CONT_VOLT_CUR;
    else
        data[2] |= CONT_VOLT_CUR;

    if (SW_DISP_OVP == 0)
        data[2] &= ~CONT_DISP_OVP;
    else
        data[2] |= CONT_DISP_OVP;

    if (SW_DISP_SET == 0)
        data[2] &= ~CONT_DISP_SET;
    else
        data[2] |= CONT_DISP_SET;

    if (SW_FOLDBACK == 0)
        data[2] &= ~CONT_FOLDBACK;
    else
        data[2] |= CONT_FOLDBACK;
}

static void interrupt
isr (void)
{
    unsigned char hp_da = HP_DA; /* cache this - timing is tight! */

    /* Handle SPI interrupt.
     * First comes address with HP_DA=L, the data with HP_DA=H.
     * With SPI each cycle is both read and write, however the HP wants
     * DATA_UP tri-state when it is not explicitly reading.  Addr=0x12
     * signifies a read.  When we see that, load SSPBUF and un-tri-state
     * DATA_UP.  After read is over, re-tri-state DATA_UP.
     * FIXME: addr=0x02 appears on the wire - why?  Ignore for now.
     */
    if (SSPIF) {
        char b = ~SSPBUF;    /* read SSPBUF no matter what (else overflow) */

        if (!hp_da) {        /* address */
            addr = b;
            if (addr == 0x12) {
                SSPBUF = data[2];
	        TRISC5 = 0;
                data[2] |= CONT_RPG_ROTAT;
            }
	} else {             /* data */
            if (addr == 0x12)
                TRISC5 = 1;
            else if ((addr & 0x10))
                data[addr & 0x0f] = b;
        }
        SSPIF = 0;
    }

#if RPG_INTERRUPT
    /* Handle interrupt on RB7:6 (RPG) state change.
     * Goal is to set CONT_RPG_ROTAT flag if turned since last read.
     * Quadrature trick: set CONT_RPG_CLOCKW if (old RPG_B xor new RPG_A) == 1.
     */
    if (RBIF) {
        char nrpg = PORTB | 0x3f;

        data[2] &= ~CONT_RPG_ROTAT;
        if (((rpg >> 7) & 1)  ^ ((nrpg >> 6) & 1) == 1)
            data[2] &= ~CONT_RPG_CLOCKW;
        else
            data[2] |= CONT_RPG_CLOCKW;
        rpg = nrpg;
        RBIF = 0;
    }
#endif
}

void
main(void)
{
    ADCON1 = 0x06;

    TRISA = PORTA_INPUTS;
    TRISB = PORTB_INPUTS;
    TRISC = PORTC_INPUTS;
    RBPU  = PORTB_PULLUP;

    lcd_init ();

    memset (data, 0xff, sizeof (data));     
    addr = 0;
    rpg = PORTB | 0x3f;	/* initialize RPG state (SW_RPG_B | SW_RPG_A) */

    while (!HP_PCLR)    /* wait for HP to come out of reset */
        ;

    /* SSPCON */
    SSPEN = 0;          /* disable SSP (clears shift reg) */
    SSPM3=0; SSPM2=1; SSPM1=0; SSPM0 = 1; /* select SPI slave mode w/o SS */
    CKP = 1;            /* clock polarity: idle state is H */
    /* SSPSTAT */
    CKE = 0;            /* data transmitted on the rising edge of SCK */
    SSPEN = 1;          /* enable SSP */

    /* Interrupts */
    SSPIF = 0;          /* clear SSP interrupt flag */
    RBIF = 0;           /* clear PORTB state change interrupt flag */
    SSPIE = 1;          /* enable SSP interrupt */
    PEIE = 1;           /* enable peripheral interupts */
#if RPG_INTERRUPT
    RBIE = 1;           /* enable interrupt on PORTB state change */
#endif
    GIE = 1;            /* enable global interrupts */

    for (;;) {
#if POLL_CONTROLS
	update_controls ();
#endif
        update_display ();
        DelayMs (100);
    }
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

