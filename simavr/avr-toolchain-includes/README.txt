This contains the AVR include directory taken from the Atmel Studio toolchain.

It is necessary to copy it heere, instead of directly reference the toolchain, because Visual Studio is too 'silly' to ignore the avr versions of stdio.h and stdint.h, and pulls them in for the windows build!

By only copying the avr directory, we avoid this problem.

