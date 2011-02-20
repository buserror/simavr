// Ansteuerung eines HD44780 kompatiblen LCD im 4-Bit-Interfacemodus
// http://www.mikrocontroller.net/articles/AVR-GCC-Tutorial/LCD-Ansteuerung
//
// Die Pinbelegung ist über defines in lcd-routines.h einstellbar
 
#include <avr/io.h>
#include "atmega48_lcd.h"
#include <util/delay.h>
 
// sendet ein Datenbyte an das LCD
 
void lcd_data(unsigned char temp1)
{
   unsigned char temp2 = temp1;
 
   LCD_PORT |= (1<<LCD_RS);        // RS auf 1 setzen
 
   temp1 = temp1 >> 4;
   temp1 = temp1 & 0x0F;
   LCD_PORT &= 0xF0;
   LCD_PORT |= temp1;               // setzen
   lcd_enable();
 
   temp2 = temp2 & 0x0F;
   LCD_PORT &= 0xF0;
   LCD_PORT |= temp2;               // setzen
   lcd_enable();
   
   _delay_us(42);
}
 
// sendet einen Befehl an das LCD
 
void lcd_command(unsigned char temp1)
{
   unsigned char temp2 = temp1;
 
   LCD_PORT &= ~(1<<LCD_RS);        // RS auf 0 setzen
 
   temp1 = temp1 >> 4;              // oberes Nibble holen
   temp1 = temp1 & 0x0F;            // maskieren
   LCD_PORT &= 0xF0;
   LCD_PORT |= temp1;               // setzen
   lcd_enable();
 
   temp2 = temp2 & 0x0F;            // unteres Nibble holen und maskieren
   LCD_PORT &= 0xF0;
   LCD_PORT |= temp2;               // setzen
   lcd_enable();
   
   _delay_us(42);
}
 
// erzeugt den Enable-Puls
void lcd_enable(void)
{
   // Bei Problemen ggf. Pause gemäß Datenblatt des LCD Controllers einfügen
   // http://www.mikrocontroller.net/topic/81974#685882
   LCD_PORT |= (1<<LCD_EN);
    _delay_us(1);                   // kurze Pause
   // Bei Problemen ggf. Pause gemäß Datenblatt des LCD Controllers verlängern
   // http://www.mikrocontroller.net/topic/80900
   LCD_PORT &= ~(1<<LCD_EN);
}
 
// Initialisierung: 
// Muss ganz am Anfang des Programms aufgerufen werden.
 
void lcd_init(void)
{
   LCD_DDR = LCD_DDR | 0x0F | (1<<LCD_RS) | (1<<LCD_EN);   // Port auf Ausgang schalten
 
   // muss 3mal hintereinander gesendet werden zur Initialisierung
 
   _delay_ms(15);
   LCD_PORT &= 0xF0;
   LCD_PORT |= 0x03;            
   LCD_PORT &= ~(1<<LCD_RS);      // RS auf 0
   lcd_enable();
 
   _delay_ms(5);
   lcd_enable();
 
   _delay_ms(1);
   lcd_enable();
   _delay_ms(1);
 
   // 4 Bit Modus aktivieren 
   LCD_PORT &= 0xF0;
   LCD_PORT |= 0x02;
   lcd_enable();
   _delay_ms(1);
 
   // 4Bit / 2 Zeilen / 5x7
   lcd_command(0x28);
    
   // Display ein / Cursor aus / kein Blinken
   lcd_command(0x0C); 
 
   // inkrement / kein Scrollen
   lcd_command(0x06);
 
   lcd_clear();
}
 
// Sendet den Befehl zur Löschung des Displays
 
void lcd_clear(void)
{
   lcd_command(CLEAR_DISPLAY);
   _delay_ms(5);
}
 
// Sendet den Befehl: Cursor Home
 
void lcd_home(void)
{
   lcd_command(CURSOR_HOME);
   _delay_ms(5);
}
 
// setzt den Cursor in Zeile y (1..4) Spalte x (0..15)
 
void set_cursor(uint8_t x, uint8_t y)
{
  uint8_t tmp;
 
  switch (y) {
    case 1: tmp=0x80+0x00+x; break;    // 1. Zeile
    case 2: tmp=0x80+0x40+x; break;    // 2. Zeile
    case 3: tmp=0x80+0x10+x; break;    // 3. Zeile
    case 4: tmp=0x80+0x50+x; break;    // 4. Zeile
    default: return;                   // für den Fall einer falschen Zeile
  }
  lcd_command(tmp);
}
 
// Schreibt einen String auf das LCD
 
void lcd_string(char *data)
{
    while(*data) {
        lcd_data(*data);
        data++;
    }
}
