#include <wiringPi.h>
#include <stdint.h>
#include "pi_dht11.h"

#define MAXTIMINGS  85
#define DHTPIN      7  
int dht11_dat[5] = { 0, 0, 0, 0, 0 };

bool read_dht11_dat(float *temperature, float *humidity)
{
    uint8_t laststate   = HIGH, thisstate;
    uint8_t counter     = 0;
    uint8_t j       = 0, i;

    dht11_dat[0] = dht11_dat[1] = dht11_dat[2] = dht11_dat[3] = dht11_dat[4] = 0;
    *temperature = *humidity = 0;

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
    for ( i = 0; i < MAXTIMINGS; i++ ) {
        counter = 0;
		thisstate=digitalRead( DHTPIN );
        while ( thisstate == laststate ) {
            counter++;
            delayMicroseconds( 1 );
            if ( counter == 255 ) {
                break;
            }
			thisstate=digitalRead( DHTPIN );
        }
        laststate = thisstate;
        delayMicroseconds( 12 );
  
        if ( counter == 255 )
            break;

        /* ignore first 3 transitions */
        if ( (i >= 4) && (i % 2 == 0) ) {
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
     bool data_available = false;
    if ( (j >= 40) &&
         (dht11_dat[4] == ( (dht11_dat[0] + dht11_dat[1] + dht11_dat[2] + dht11_dat[3]) & 0xFF) ) )
    {
        *temperature = (dht11_dat[2] + dht11_dat[3] / 10.)  * 9. / 5. + 32;
        *humidity = dht11_dat[0] + dht11_dat[1] / 10.;
        data_available = true;
    }
    
    return data_available;
}
