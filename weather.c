#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include "pi_lcd.h"


//censor
#define MAXTIMINGS  85
#define DHTPIN      15  

#define STATE_TIME              0
#define STATE_INDOOR            1 //displaying in door temperature
#define STATE_OUTDOOR           2

extern int frederick();

int dht11_dat[5] = { 0, 0, 0, 0, 0 };
static float temperature, humidity;//fahrenheit
static bool done;

void handle_signal(int signal) {
    // Find out which signal we're handling
    switch (signal) {
		case SIGINT://key 3
			done=true;
			break;
	}
}

void hook_signal() {
	struct sigaction sa;
	
	// Setup the sighub handler
    sa.sa_handler = &handle_signal;
    // Restart the system call, if at all possible
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
		printf("Failed to capture sigint 1\n");
	}
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

//gcc -o weather ../weather.c ../pi_lcd.c ../yahoo.c -lwiringPi -lwiringPiDev -lssl -lcrypto
int main( void )
{
    done = false;
    hook_signal();
    
    wiringPiSetup();
    init_lcd();
    init_censor(); //temperature
    
    struct tm *timeinfo ;
    time_t rawtime ;
    char buffer[128] ;

    int counter = 0;
    int states[] = {STATE_TIME, STATE_TIME, STATE_TIME, STATE_TIME, STATE_TIME, STATE_INDOOR, STATE_OUTDOOR};
    int pos = 0;
    while ( !done ) {
        switch (states[pos]) {
            case STATE_TIME: {
                //get current time
                rawtime = time (NULL) ;
                timeinfo = localtime(&rawtime) ;
                strftime(buffer,sizeof (buffer),"%H:%M:%S %a",timeinfo);
                show_lcd_center(0, "NOW");
                show_lcd_center(1, buffer);
                counter = 5;
                }
                break;
            case STATE_INDOOR:
                if (read_dht11_dat()) {
                    sprintf(buffer, "%.1fF / %.1f%%", temperature, humidity);
                    show_lcd_center(0, "HOME");
                    show_lcd_center(1, buffer);
                    
                    delay(4000);
                }
                break;
            case STATE_OUTDOOR: {
                    int temperature = frederick();
                    if (temperature >= 0) {
                        sprintf(buffer, "%dF", temperature);
                        show_lcd_center(0, "FREDERICK");
                        show_lcd_center(1, buffer);
                        delay(4000);
                    }
                }
                break;
        }
        pos = (pos + 1) % (sizeof (states) / sizeof (int));
        delay(1000); /* wait 1sec to refresh */
    }
    
	deinit_lcd();

    return(0);
}
