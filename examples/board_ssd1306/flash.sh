avr-objcopy -R .mmcu --change-section-lma .mmcu=0 -R .fuse -R .lock -R .signature -R .mmcu  --change-section-vma .data=0  --change-section-vma .text=0 -O ihex atmega32_ssd1306.axf "atmega32_ssd1306.hex";
avrdude -pm32 -cavrisp2 -Pusb -y -Uflash:w:atmega32_ssd1306.hex:a;


