#include <wiringPi.h>
#include <pcf8574.h>
#include <lcd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#define bool 	int
#define true 	1
#define false 	0

//censor
#define MAXTIMINGS  85
#define DHTPIN      15  
int dht11_dat[5] = { 0, 0, 0, 0, 0 };
static float temperature, humidity;//fahrenheit

//lcd
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
// Global lcd handle:
static int lcdHandle;

static bool init_lcd() {
    int i;
    pcf8574Setup(AF_BASE,0x3F);
    lcdHandle = lcdInit (2, 16, 4, AF_RS, AF_E, AF_DB4,AF_DB5,AF_DB6,AF_DB7, 0,0,0,0) ;
    if (lcdHandle < 0)
        return false;
    
    for(i=0;i<8;i++)
        pinMode(AF_BASE+i,OUTPUT); 	//Will expand the IO port as the output mode
    digitalWrite(AF_LED,1); 	//Open back light
    digitalWrite(AF_RW,0); 		//Set the R/Wall to a low level, LCD for the write state
    lcdClear(lcdHandle); 		//Clear display

    return true;
}

static void show_lcd (int line, const char *msg) {
    if (lcdHandle < 0)
        return;
    
    lcdPosition(lcdHandle,0,line);
    lcdPuts(lcdHandle, msg);
}

static bool init_censor() {
    temperature = 0;
    humidity    = 0;
    
    return true;
}

static bool read_dht11_dat()
{
    uint8_t laststate   = HIGH;
    uint8_t counter     = 0;
    uint8_t j       = 0, i;
    
    temperature = 0;
    humidity    = 0;

    dht11_dat[0] = dht11_dat[1] = dht11_dat[2] = dht11_dat[3] = dht11_dat[4] = 0;

    /* pull pin down for 18 milliseconds */
    pinMode( DHTPIN, OUTPUT );
    digitalWrite( DHTPIN, LOW );
    delay( 18 );
    /* then pull it up for 40 microseconds */
    digitalWrite( DHTPIN, HIGH );
    delayMicroseconds( 40 );
    /* prepare to read the pin */
    pinMode( DHTPIN, INPUT );

    /* detect change and read data */
    for ( i = 0; i < MAXTIMINGS; i++ )
    {
        counter = 0;
        while ( digitalRead( DHTPIN ) == laststate )
        {
            counter++;
            delayMicroseconds( 1 );
            if ( counter == 255 )
            {
                break;
            }
        }
        laststate = digitalRead( DHTPIN );
    	delayMicroseconds( 12 );

        if ( counter == 255 )
            break;

        /* ignore first 3 transitions */
        if ( (i >= 4) && (i % 2 == 0) )
        {
            /* shove each bit into the storage bytes */
            dht11_dat[j / 8] <<= 1;
            if ( counter > 16 )
                dht11_dat[j / 8] |= 1;
            j++;
        }
    }

    /*
     * check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
     * print it out if data is good
     */
    if ( (j >= 40) && (dht11_dat[4] == ( (dht11_dat[0] + dht11_dat[1] + dht11_dat[2] + dht11_dat[3]) & 0xFF) ) ) {
        temperature = (dht11_dat[2] + dht11_dat[3] / 10.)  * 9. / 5. + 32;
        humidity = dht11_dat[0] + dht11_dat[1] / 10.;
        
        return true;
    }
    
    return false;
    
}

//gcc -o weather weather.c -lwiringPi -lwiringPiDev
int main( void )
{
    wiringPiSetup();
    init_lcd();
    init_censor(); //temperature
    
    struct tm *timeinfo ;
    time_t rawtime ;
    char buffer[128] ;

    while ( 1 ) {
        //get current time
        rawtime = time (NULL) ;
        timeinfo = localtime(&rawtime) ;
        strftime(buffer,sizeof (buffer),"%H:%M:%S %a",timeinfo);
        show_lcd(0, buffer);
        
        //temperature & humidity
        if (read_dht11_dat()) {
            sprintf(buffer, "%.1fF / %.1f%%", temperature, humidity);
            show_lcd(1, buffer);
        }
        delay( 1000 ); /* wait 1sec to refresh */
    }
    return(0);
}
