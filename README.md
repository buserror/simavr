simavr - a lean and mean Atmel AVR simulator for linux
======

# Notes for compiling on MinGW

Get MSys2 mingw from https://www.msys2.org/
Start the MSYS MinGW shortcut (e.g. MSYS2 MinGW 32-bit or 64-bit)

Update packages 

```
pacman -Syu
```

Install toolchains and dependencies. For 64-bit 

```
pacman -S mingw-w64-x86_64-toolchain
pacman -S mingw-w64-x86_64-libelf
pacman -S mingw-w64-x86_64-avr-toolchain
pacman -S mingw-w64-x86_64-freeglut
```

Replace `x86_64` with `i686` to get the 32-bit version instead. E.g, 

```
pacman -S mingw-w64-i686-toolchain
```

etc. 

Note: If you are using the 32-bit version, you **may** have to adapt `Makefile.common` regarding `AVR_ROOT := /mingw64/avr` to `AVR_ROOT := /mingw32/avr`. I have not tested this and the paths may be the same, but if you run into errors regarding AVR, check there.

In the normal user directory (`cd ~`), clone this repo 

```
git clone https://github.com/maxgerhardt/simavr.git
cd simavr
```

Start building with

```
make build-simavr V=1
```

That should be successfull. 

Now create a install directory and install it. Note that `/home/Max/` is in this case my home directory.

Additionally it copies needed runtime DLLs to the output directory. Ignore the compile terror in the examples, it only counts that simavr is copied to the install directory.

```
mkdir simavr_installed
make install DESTDIR=/home/$USER/simavr/simavr_installed/
mv simavr_installed/bin/simavr simavr_installed/bin/simavr.exe
```

Copy the compiled output back to the normal Windows environment, e.g.

```
cp -r /home/$USER/simavr/simavr_installed/ /c/Users/$USER/Desktop
```

You should now have the simavr executable. 

```
C:\Users\Max\Desktop\simavr_installed\bin>simavr --help
Usage: simavr [...] <firmware>
       [--freq|-f <freq>]  Sets the frequency for an .hex firmware
       [--mcu|-m <device>] Sets the MCU type for an .hex firmware
       [--list-cores]      List all supported AVR cores and exit
       [--help|-h]         Display this usage message and exit
       [--trace, -t]       Run full scale decoder trace
       [-ti <vector>]      Add traces for IRQ vector <vector>
       [--gdb|-g [<port>]] Listen for gdb connection on <port> (default 1234)
       [-ff <.hex file>]   Load next .hex file as flash
       [-ee <.hex file>]   Load next .hex file as eeprom
       [--input|-i <file>] A vcd file to use as input signals
       [--output|-o <file>] A vcd file to save the traced signals
       [--add-trace|-at <name=kind@addr/mask>] Add signal to be traced
       [-v]                Raise verbosity level
                           (can be passed more than once)
       <firmware>          A .hex or an ELF file. ELF files are
                           prefered, and can include debugging syms
```

-----------------------

_simavr_ is a new AVR simulator for linux, or any platform that uses avr-gcc. It uses 
avr-gcc's own register definition to simplify creating new targets for supported AVR
devices. The core was made to be small and compact, and hackable so allow quick 
prototyping of an AVR project. The AVR core is now stable for use with parts 
with <= 128KB flash, and with preliminary support for the bigger parts. The 
simulator loads ELF files directly, and there is even a way to specify simulation 
parameters directly in the emulated code using an .elf section. You can also 
load multipart HEX files.

Installation
------------
On OSX, we recommend using [homebrew](https://brew.sh):

    brew tap osx-cross/avr
    brew install --HEAD simavr

On Ubuntu, SimAVR is available in the Bionic package source:

    apt-get install simavr

(Note that the command is made available under the name `simavr` not `run_avr`.)

Otherwise, `make` is enough to just start using __bin/simavr__. To install the __simavr__ command system-wide, `make install RELEASE=1`.

Supported IOs
--------------
* _eeprom_
* _watchdog_
* _IO ports_ (including pin interrupts)
* _Timers_, 8 &16 (Normal, CTC and Fast PWM, the overflow interrupt too)
* The _UART_, including tx & rx interrupts (there is a loopback/local echo test mode too)
* _SPI_, master/slave including the interrupt
* _i2c_ Master & Slave
* External _Interrupts_, INT0 and so on.
* _ADC_
* Self-programming (ie bootloaders!)

Emulated Cores (very easy to add new ones!)
--------------
+ ATMega2560
+ AT90USB162 (with USB!)
+ ATMega1281
+ ATMega1280
+ ATMega128
+ ATMega128rf1
+ ATMega16M1
+ ATMega169
+ ATMega162
+ ATMega164/324/644
+ ATMega48/88/168/328
+ ATMega8/16/32
+ ATTiny25/45/85
+ ATTIny44/84
+ ATTiny2313/2313v
+ ATTiny13/13a

Extras:
-------
* fully working _gdb_ support including some pretty cool “passive modes”.
* There is also very easy support for “VCD” (Value Change Dump) that can be visualized 
graphically as “waveforms” with tools like _gtkwave_ (see below).
* There are a few examples of real life firmwares running on simavr, including OpenGL rendering of the display…
* There is support for _Arduino_, but no IDE integration

Documentation And Further Information
-------------------------------------

* Manual / Developer Guide: https://github.com/buserror-uk/simavr/blob/master/doc/manual/manual.pdf?raw=true
* Examples: https://github.com/buserror-uk/simavr/tree/master/examples
* Mailing List: http://groups.google.com/group/simavr
* IRC: _#simavr_ on Freenode

Contributing
------------

Patches are always welcome! Please submit your changes via Github pull requests.

VCD Support -- built in logic analyzer 
-----------
_simavr_ can output most of its pins, firmware variables, interrupts and a few other
things as signals to be dumped into a file that can be plotted using gtkwave for
further, precise analysis.
A firmware can contain instructions for _simavr_ to know what to trace, and the file is
automatically generated.
Example:

	const struct avr_mmcu_vcd_trace_t _mytrace[]  _MMCU_ = {
		{ AVR_MCU_VCD_SYMBOL("UDR0"), .what = (void*)&UDR0, },
		{ AVR_MCU_VCD_SYMBOL("UDRE0"), .mask = (1 << UDRE0), .what = (void*)&UCSR0A, },
	};

Will tell _simavr_ to generate a trace everytime the UDR0 register changes and everytime
the interrupt is raised (in UCSR0A). The *_MMCU_* tag tells gcc that it needs compiling,
but it won't be linked in your program, so it takes literally zero bytes, this is a code
section that is private to _simavr_, it's free!
A program running with these instructions and writing to the serial port will generate
a file that will display:

	$ ./simavr/run_avr tests/atmega88_example.axf
	AVR_MMCU_TAG_VCD_TRACE 00c6:00 - UDR0
	AVR_MMCU_TAG_VCD_TRACE 00c0:20 - UDRE0
	Loaded 1780 .text
	Loaded 114 .data
	Loaded 4 .eeprom
	Starting atmega88 - flashend 1fff ramend 04ff e2end 01ff
	atmega88 init
	avr_eeprom_ioctl: AVR_IOCTL_EEPROM_SET Loaded 4 at offset 0
	Creating VCD trace file 'gtkwave_trace.vcd'
	Read from eeprom 0xdeadbeef -- should be 0xdeadbeef..
	Read from eeprom 0xcafef00d -- should be 0xcafef00d..
	simavr: sleeping with interrupts off, quitting gracefully

And when the file is loaded in gtkwave, you see:
![gtkwave](https://github.com/buserror-uk/simavr/raw/master/doc/img/gtkwave1.png)

You get a very precise timing breakdown of any change that you add to the trace, down
to the AVR cycle. 

Example:
--------
_simavr_ is really made to be the center for emulating your own AVR projects, not just
a debugger, but also the emulating the peripherals you will use in your firmware, so 
you can test and develop offline, and now and then try it on the hardware.

You can also use _simavr_ to do test units on your shipping firmware to validate it
before you ship a new version, to prevent regressions or mistakes.

_simavr_ has a few 'complete projects/ that demonstrate this, most of them were made
using real hardware at some point, and the firmware binary is _exactly_ the one that
ran on the hardware. The key here is to emulate the _parts_ or peripherals that
are hooked to the AVR. Of course, you don't have to emulate the full hardware, you just
need to generate the proper stimulus so that the AVR is fooled.

HD44780 LCD Board Demo
----------------------

![lcd](https://github.com/buserror-uk/simavr/raw/master/doc/img/hd44780.png)

This example board hooks up an Atmega48 to an emulated HD44780 LCD and display a running
counter in the 'lcd'. Everything is emulated, the firmware runs exactly like this
on a real hardware.

![lcd-gtkwave](https://github.com/buserror-uk/simavr/raw/master/doc/img/hd44780-wave.png)

And this is a gtkwave trace of what the firmware is doing. You can zoom in, measure, etc
in gtkwave, select trades to see etc.

Quite a few other examples are available!
