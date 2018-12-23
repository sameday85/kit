#include <wiringPi.h>
#include <string.h>
#include <pcf8574.h>
#include "pi_lcd.h"

//PCF8574 Start I/O address
//PCF8754 64+8
#define AF_BASE 64
#define AF_RS (AF_BASE + 0)
#define AF_RW (AF_BASE + 1)
#define AF_E (AF_BASE + 2)
#define AF_LED (AF_BASE + 3)
#define AF_DB4 (AF_BASE + 4)
#define AF_DB5 (AF_BASE + 5)
#define AF_DB6 (AF_BASE + 6)
#define AF_DB7 (AF_BASE + 7)

#define LCD_ROWS        2
#define LCD_COLS        16

// Global lcd handle:
static int lcdHandle;
static bool lcd_is_on;

bool init_lcd() {
    int i;
    pcf8574Setup(AF_BASE,0x3F);
    lcdHandle = lcdInit (LCD_ROWS, LCD_COLS, 4, AF_RS, AF_E, AF_DB4,AF_DB5,AF_DB6,AF_DB7, 0,0,0,0) ;
    if (lcdHandle < 0)
        return false;
    
    for(i=0;i<8;i++)
        pinMode(AF_BASE+i,OUTPUT);  //Will expand the IO port as the output mode
    digitalWrite(AF_LED,1);     //Open back light
    digitalWrite(AF_RW,0);      //Set the R/Wall to a low level, LCD for the write state
    lcdClear(lcdHandle);        //Clear display

    lcd_is_on = true;
    return true;
}

void deinit_lcd() {
    if (lcdHandle < 0)
        return;

    digitalWrite(AF_LED,0);
    lcdClear(lcdHandle);
    lcdHandle=-1;
    lcd_is_on = false;
}

void turn_on_lcd() {
    if (lcd_is_on)
        return;
    digitalWrite(AF_LED,1);
    lcd_is_on = true;
}

void turn_off_lcd() {
    if (!lcd_is_on)
        return;
    digitalWrite(AF_LED,0);
    lcdClear(lcdHandle);
    lcd_is_on = false;
}

void show_lcd (int line, const char *msg) {
    if (lcdHandle < 0 || !lcd_is_on)
        return;
    
    lcdPosition(lcdHandle,0,line);
    lcdPuts(lcdHandle, msg);
}

void show_lcd_center (int line, const char *msg) {
    char szBuf[LCD_COLS+4];
    int len = strlen (msg);
    
    if (lcdHandle < 0 || !lcd_is_on)
        return;
    
    if (len == LCD_COLS) {
        strcpy (szBuf, msg);
    }
    else if (len > LCD_COLS) {
        strncpy (szBuf, msg, LCD_COLS);
    }
    else {
        int margin = (LCD_COLS - len) / 2;
        for (int i = 0; i < sizeof (szBuf); ++i) {
            szBuf[i]=' ';
        }
        strcpy (szBuf + margin, msg);
        szBuf[margin+len]=' ';
    }
    szBuf[LCD_COLS]='\0';
    lcdPosition(lcdHandle,0,line);
    lcdPuts(lcdHandle, szBuf);
}

