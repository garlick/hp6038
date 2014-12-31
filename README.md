## HP 6038A Front Panel Replacement Hardware

A schematic of the original TTL front panel is available in the
[HP6033A/38A service manual](http://www.home.agilent.com/agilent/redirector.jspx?action=ref&cname=AGILENT_EDITORIAL&ckey=1000000382-1%3Aepsg%3Aman&lc=eng&cc=US&nfr=-35691.384539), page 99.

The heart of the replacement front panel is the
[PIC 16F873A](http://www.microchip.com/wwwproducts/Devices.aspx?dDocName=en010236). In a nutshell, the PIC interfaces with the various input/output devices
on the front panel, and communicates with the GPIB board using a variant
of SPI.  The PIC emulates a set of addressable shift registers that
correspond to the state of the input/output devices.

![](https://github.com/garlick/hp6038/blob/master/doc/schematic.png)

### GPIB Board Interface

The front panel attaches to the GPIB board via a 16-pin ribbon cable.
The front panel obtains power from the GPIB board.

| pin            | function       |description               |
|----------------|----------------|--------------------------|
| 1,4,7,10,15    | +5V            |                          |
| 3,6,9,12,13,14 | GND            |                          |
| 2              | HP_DATADOWN    | serial I/O to front panel|
| 5              | HP_IOCLOCK     | L=data valid             |
| 8              | HP_DA          | L=address, H=data        |
| 11             | HP_DATAUP      | serial I/O from front panel|
| 16             | HP_PCLR        | L=clear shift registers  |

![](https://github.com/garlick/hp6038/blob/master/doc/ribbon.png)

The handshake employed by the HP
to move serial data into shift registers is similar to SPI except for the
additional `HP_DA` line to designate whether the byte being transferred is
address or data, and the fact that the `HP_DATAUP` line must be in a high
impedance state when the GPIB board has not addressed it for a read.

The PIC is programmed to use its MSSP module in a three-pin slave SPI mode
(`SCK`, `SDI`, and `SDO`), and to generate an interrupt when a character is received.
`SDO` is initially programmed as a parallel input to get it into the high
impedance state.

Upon interrupt, the state of `HP_DA` is sampled to determine whether
an address or data byte is being read.  If address, the value is stored,
and if it addresses a read register (0x12 - there is only one), the
register contents are placed in `SSPBUF` and `SDO` is configured as an output.

When a data byte is read on `HP_DATADOWN` (address != 0x12), the value is stored
in the addressed register.

When a data byte is written on `HP_DATAUP` (address == 0x12), `SDO` is configured
in high-impedance mode afterwards.

As shown in the scope traces on the [protocol Protocol] wiki page,
the HP drives `SCK` at a half period of about 1 microsecond.
Call this T<sub>sck</sub> / 2.

The PIC specification requires that:

> T<sub>sck</sub>/2 >= T<sub>cy</sub> + 20 ns

Since T<sub>cy</sub> = 4/F<sub>osc</sub>, an equivalent statement is:

> T<sub>sck</sub>/2 >= 4/F<sub>osc</sub> + 20 ns

Substituting 1 microsecond (1000 ns) for Tsck/2:

> 980 ns >= 4/F<sub>osc</sub>

> F<sub>osc</sub> >= 4 / (9.80 x 10<sup>-7</sup> s)

> F<sub>osc</sub> >= 4.08 x 10<sup>6</sup> Hz

Thus to avoid going below the minimum T<sub>sck</sub>/2, the minimum 
F<sub>osc</sub> is 4.08 MHz.

In addition the `HP_DA` line must be sampled within about 5 microseconds
after the last bit of a byte is shifted in by `SCK`.  This is perhaps an even
more demanding requirement since it requires the PIC to take an interrupt
from the MSSP, save context, etc., and execute the interrupt
service routine in time to sample that line.  I did not study this requirement
in detail but found a 20 MHz oscillator to be adequate.

### LCD

The LCD employed in this project uses a 
[Hitachi HD44780 compatible](http://ouwehand.net/~peter/lcd/lcd.shtml)
controller chip.  It is configured in 4 byte data mode so only 7 PIC I/O
pins are consumed by the LCD.

A 1K resistor is attached between pin 3 `contrast` and pin 1 `GND` to set
the display contrast.  The value was determined experimentally by attaching
a potentiometer, adjusting the display, measuring the value and substituting
a fixed resistance.

The main loop of the firmware updates the LCD display content based on the
state of the various registers every 10 ms.  Since the registers for numeric
displays in the original front panel directly drive LED segments, a lookup
table is used to map register values to ASCII characters required by the LCD.
The service manual schematic had the LED segments
assigned to the wrong shift register bits.
The correct wiring was deduced through trial and error:

![](https://github.com/garlick/hp6038/blob/master/doc/led.png)

| bit | 7 segment | 5 segment |
|-----|-----------|-----------|
|0    |g          |e |
|1    |c          |a |
|2    |d          |b |
|3    |e          |d |
|4    |a          |c |
|5    |f          |  |
|6    |d.p.       |  |
|7    |b          |  |

### RPG

The rotary pulse generator (RPG) or
[rotary encoder](http://en.wikipedia.org/wiki/Rotary_encoder)
is a digital rotary input device with quadrature output that is used
in the HP 6038A to adjust voltage and current.  It is connected to PIC
inputs `RB6` and `RB7` which are configured to interrupt
whenever their state changes.  Two bits are set in the interrupt service
routine, one indicating that the RPG changed, and another indicating its
direction of rotation (based on the previous 2 bit code).  Each time these
bits are sampled by the HP, they are cleared.  By sampling frequently,
the HP can tell how fast and in what direction the knob is turning.

I tried three encoders:

* Encoder Products Co. [FV00233](http://www.encoder.com/model15th.html):
2048 cycles per revolution (CPR), no detents (clicks)
* Grayhill [62P](http://www.grayhill.com/catalog/Opt_Encoder_62P.pdf):
4 CPR, 16 detents
* Avago [HRPG-A032](http://www.avagotech.com/docs/5988-5851EN):
32 CPR, 32 detents

I settled on the Avago although I think a bit more resolution might be better.
Note: with 32 CPR, 32 detents there are actually 128 interrupts per revolution,
but one can only report one change per detent, or that perfect setting
between detents remains forever out of reach!

### Switches

Five normally open pushbuttons are connected to five inputs on the PIC.
10K resistors (in a `761-1-R10K` resistor pack) pull the inputs high when
the switches are open.  The inputs are grounded when the switches are closed.

### Chassis Mounting

The HP6038A has an inner and an outer front panel, both alumininum.
The inner panel is drilled for the front panel option but the outer panel
on an option 001 model is not.  Standoffs are in place for mounting a PC
board inside the inner panel.  I mounted a perfboard across the top half
of the inner panel with LCD mounted to it, and used existing holes in
the inner panel for LCD and controls.
I then used the inner panel as a template for drilling holes in the front
panel.  A friend of mine handled the rectangular hole for the LCD;
however if you have no friends, this article may be a good starting point:
[Make Square Holes in Aluminum Sheet Metal](http://makezine.com/extras/15.html)
by Nick Carter for _Make Magazine_.
