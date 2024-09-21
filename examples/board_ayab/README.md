# AYAB Shield example

This contains a sample program to emulate the AYAB shield running on uno or mega CPU

Once simavr compiled, you should have a ayab.elf executable in the obj-x86_64-linux-gnu directory (for a linux/x86 host)

Available command line parameters:
``` bash
# ./obj-x86_64-linux-gnu/ayab.elf --help 
Usage: ayab.elf [...] <firmware>
       [--help|-h|-?]      Display this usage message and exit
       [--list-cores]      List all supported AVR cores and exit
       [-v]                Raise verbosity level
                           (can be passed more than once)
       [--freq|-f <freq>]  Sets the frequency for an .hex firmware
       [--mcu|-m <device>] Sets the MCU type for an .hex firmware
       [--gdb|-g [<port>]] Listen for gdb connection on <port> (default 1234)
       [--machine|-m <machine>]   Select KH910/KH930 machine (default=KH910)
       [--carriage|-c <carriage>] Select K/L/G carriage (default=K)
       [--beltphase|-b <phase>]   Select Regular/Shifted (default=Regular)
       [--startside|-s <side>]    Select Left/Right side to start (default=Left)
       <firmware>          An ELF file (can include debugging syms)
```

Once started, the program create a pseudo TTY serial interface you can connect to (/dev/pts/4 here below).
``` bash
#./obj-x86_64-linux-gnu/ayab.elf --mcu atmega328p --freq 16000000 firmware.elf
ELF: Loaded 9634 bytes at 0
ELF: Loaded 384 bytes at 800100
firmware.elf loaded (f=16000000 mmcu=atmega328p)
uart_pty_init bridge on port *** /dev/pts/4 ***
uart_pty_connect: /tmp/simavr-uart0 now points to /dev/pts/4
note: export SIMAVR_UART_XTERM=1 and install picocom to get a terminal

simavr launching:
```

A basic UI will show LEDs, carriage and lines for needle bed limits.
Carriage can be moved with keyboard's left and right arrows.
Terminal interface will show carriage position, selected needle and needle bed (2x100needles)

Machine and carriage type, start side and beltphase can be selected using command line options (see above)
