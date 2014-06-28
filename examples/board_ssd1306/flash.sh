avr-objcopy -R .mmcu --change-section-lma .mmcu=0 -R .fuse -R .lock -R .signature -R .mmcu  --change-section-vma .data=0  --change-section-vma .text=0 -O ihex atmega48_charlcd.axf "atmega48_charlcd.hex";
avrdude -pm32 -cavrisp2 -Pusb -y -Uflash:w:atmega48_charlcd.hex:a;


