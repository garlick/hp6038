#include <htc.h>
#include <string.h>
#include <stdio.h>

#if defined(_16F873A)
__CONFIG (HS & WDTDIS & PWRTEN & BORDIS & LVPDIS & DUNPROT
             & WRTEN & DEBUGDIS & UNPROTECT);
#else
#error Config bits and other stuff may need attention for non-16F873A chip.
#endif

/**
 ** PIC parallel port/SPI definitions
 **/

#define SW_LOCAL  	RA0
#define LCD_EN		RA1
#define LCD_RW		RA2
#define LCD_RS		RA3
#define SW_DISP_SET	RA4
#define SW_FOLDBACK	RA5
#define PORTA_INPUTS	0b00110001

#define LCD_DATA	PORTB
#define SW_RPG		PORTB
#define LCD_DATA4	RB0
#define LCD_DATA5	RB1
#define LCD_DATA6	RB2
#define LCD_DATA7	RB3
#define __UNUSED1  	RB4	/* RBI */
#define __UNUSED2  	RB5	/* RBI */
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
#define DEBUG_LED       RC7	/* used for debugging during development */
#define PORTC_INPUTS	0b01111111 /* N.B. HP_DATAUP is initially tri-state */


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

/* If your RPG has detents or if you want to reduce its resolution by 4,
 * define the <B><A> greycode that is emitted when you land on a detent.
 * Undef this if you don't want it.
 */
#define RPG_DETENT	0

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


#define	LCD_STROBE()	((LCD_EN = 1),(LCD_EN=0))

void
lcd_write(unsigned char c)
{
	__delay_us(40);
	LCD_DATA = (c >> 4) & 0x0f;
	LCD_STROBE();
	LCD_DATA = c & 0x0f;
	LCD_STROBE();
}

void
lcd_clear(void)
{
	LCD_RS = 0;
	lcd_write(0x1);
	__delay_ms(2);
}

void
lcd_puts(const char * s)
{
	LCD_RS = 1;
	while(*s)
		lcd_write(*s++);
}

void
lcd_goto(unsigned char pos)
{
	LCD_RS = 0;
	lcd_write(0x80+pos);
}
	
void
lcd_init()
{
	LCD_RS = 0;
	LCD_EN = 0;
	LCD_RW = 0;
	
	__delay_ms(15);
	LCD_DATA = 0x3;
	LCD_STROBE();
	__delay_ms(5);
	LCD_STROBE();
	__delay_us(200);
	LCD_STROBE();
	__delay_us(200);
	LCD_DATA = 2;		/* select 4 bit mode */
	LCD_STROBE();

	lcd_write(0x28); 	/* set interface length */
	lcd_write(0xc); 	/* display on, cursor off, cursor no blink */
	lcd_clear();		/* clear screen */
	lcd_write(0x6);		/* set entry mode */
}

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
	/* FIXME: do we need "E" "r" for self-test error display? */
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
        case (0xef & 0xf9): return "+1";
        case (0xe6 & 0xf9): return "-1";
	case 0xff: return " ";
	default:   return "*";
    }
}

static void
update_display (void)
{
    char s[21];
    char tmp1[10], tmp2[10];;
    unsigned char d = ~data[0];
    unsigned char e = ~data[1];

    /* Line 1: volts and amps
     */
    sprintf (tmp1, "%s%s%s%s",
                 seg5str (data[0xf]), seg7str (data[0xe]),
                 seg7str (data[0xd]), seg7str (data[0xc]));
    sprintf (tmp2, "%s%s%s%s",
                 seg5str (data[0xb]), seg7str (data[0xa]),
                 seg7str (data[0x9]), seg7str (data[0x8]));
    sprintf (s, "%-10s%10s", tmp1, tmp2);
    lcd_goto (0);
    lcd_puts (s);

    /* Line 2: [CC|CV] ADJ-[c|v] [fold] [gpib|error]
     * FIXME: only one error can be displayed at a time.
     */
    sprintf (tmp1, "%c%c%c%c",
        (d & MODE_RMT) ? '*' : ' ',
        (d & MODE_LSN) ? '*' : ' ',
        (d & MODE_TLK) ? '*' : ' ',
        (d & MODE_SRQ) ? '*' : ' ');
    sprintf (s, "%-2s %-4s %-4s %7s",
                (d & MODE_CV) ? "cV" : (d & MODE_CC) ? "cC" : "",
                (e & MODE_VOLTAGE) ? "adjV" : (e & MODE_CURRENT) ? "adjC" : "",
	        (e & MODE_FOLDBACK_EN) ? "fold" : "",
	                (d & MODE_OV)        ? "OVERVLT" 
	              : (d & MODE_OT)        ? "OVERTMP"
	              : (d & MODE_OVERRANGE) ? "OVERRNG"
	              : (d & MODE_DISABLED)  ? "DISABL"
                      : (d & MODE_FOLDBACK)  ? "FOLDBCK"
                      : (d & MODE_ERROR)     ? "ERROR" : tmp1);
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

    /* Handle interrupt on RB7:6 (RPG) state change.
     * Goal is to set CONT_RPG_ROTAT flag if turned since last read.
     * Quadrature trick: set CONT_RPG_CLOCKW if (old RPG_B xor new RPG_A) == 1.
     */
    if (RBIF) {
        char nrpg = (SW_RPG >> 6) & 0x3;

#ifdef RPG_DETENT
	if (nrpg != RPG_DETENT)
            goto skip_notdetent;
#endif
        data[2] &= ~CONT_RPG_ROTAT;
        if (((rpg >> 1) ^ nrpg) & 1 == 1)
            data[2] &= ~CONT_RPG_CLOCKW;
        else
            data[2] |= CONT_RPG_CLOCKW;
skip_notdetent:
        rpg = nrpg;
        RBIF = 0;
    }
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
    rpg = (PORTB >> 6) & 0x3; /* initialize RPG state */

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
    RBIE = 1;           /* enable interrupt on PORTB state change */
    GIE = 1;            /* enable global interrupts */

    for (;;) {
	update_controls ();
        update_display ();
	__delay_ms(10);
    }
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

