
#include <wiringPi.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include "pi_lcd.h"

#define CLOCKWISE           1
#define COUNTER_CLOCKWISE   2

#define pinA    1
#define pinB    0
#define pinC    2
#define pinD    3

#define PIN_LED		10
#define PIN_BTN		29

#define ONE_WAY_STEPS	260
#define DURATION_MS		10

#define TIME_HOUR_FEED         18
#define TIME_SCREEN_SAVER      120 //in seconds

void delayMS(int x);
void rotate(int* pins, int direction);
void turn_off_motor(int *pins);
bool should_feed_now();

bool feeded_today;
bool demo_mode;
bool ready, done, paused,feeding;
int display_timer, counter_on_lcd; 

bool g_led_is_on;

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

void turn_on_led() {
    if (g_led_is_on)
        return;
    digitalWrite(PIN_LED, HIGH);
    g_led_is_on = true;
}

void turn_off_led() {
    if (!g_led_is_on)
        return;
    digitalWrite(PIN_LED, LOW);
    g_led_is_on = false;
}

//production mode: pressing the button to turn the lcd on
//demo mode: pause or resume the application
void* btn_monitor(void *arg) {
	while (!done) {
		if (ready) {
            if (digitalRead(PIN_BTN) == HIGH) {
                bool pressed = false;
                delay(200);
                if (digitalRead(PIN_BTN) == HIGH) {
                    pressed = true;
                }
                if (pressed) {
                    if (demo_mode) {
                        paused = paused ? false : true;
                        if (paused)
                            turn_on_led();
                        else
                            turn_off_led();
                    }
                    else {
                        display_timer = TIME_SCREEN_SAVER;
                    }
                    while (digitalRead(PIN_BTN) == HIGH) {
                        delay(200);
                    }
                }
            }
		}
		delay(500);
	}
	return NULL;
}

void* timer_raw(void *arg) {
	char szBuf1[255], szBuf2[255];
	
	strcpy (szBuf1, " ");
	strcpy (szBuf2, " ");
	
	init_lcd();
	while (!done) {
		if (ready) {
			//need to keep tracking even the lcd is off
			if (feeding) {
				strcpy (szBuf1, "Feeding...");
			}
			else {//display current time
				struct tm *timeinfo ;
				time_t rawtime ;
				rawtime = time (NULL) ;
				timeinfo = localtime(&rawtime);
				sprintf (szBuf1, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
				if (counter_on_lcd > 0) {
					sprintf(szBuf2, "FOOD %02d %02d:%02d:%02d", counter_on_lcd, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
					counter_on_lcd = 0;
				}
                else if (strlen(szBuf2) <= 1) {
			        sprintf(szBuf2, "Everyday at %02d", TIME_HOUR_FEED);
                }
			}
		}
        if (!demo_mode && display_timer > 0)
            --display_timer;
        if (display_timer <= 0) {
            turn_off_lcd();
        }
        else {
            turn_on_lcd();
            show_lcd_center(0, szBuf1);
            show_lcd_center(1, szBuf2);
        }
        if (ready && !demo_mode) {
            //flashing the led
            if (g_led_is_on)
                turn_off_led();
            else
                turn_on_led();
        }
		delayMS(1000);
	}
	deinit_lcd();
	digitalWrite(PIN_LED, LOW);

	return NULL;
}

//gcc -o feeder feeder.c pi_lcd.c -lpthread -lwiringPi -lwiringPiDev -lm
int main(int argc, char *argv[]) 
{
	int counter;
    bool on_boot;
	
	int pins[4] = {pinA, pinB, pinC, pinD};
	if (-1 == wiringPiSetup()) {
		printf("Setup wiringPi failed!");
		return 1;
	}
	
	hook_signal();

	feeded_today = done = demo_mode = on_boot = false;
	for (int i = 1; i < argc; ++i) {
		if (strcmp (argv[i], "-demo") == 0)
			demo_mode = true;
		else if (strcmp (argv[i], "-boot") == 0)
			on_boot = true;
	}

	/* set mode to output */
	pinMode(pinA, OUTPUT);
	pinMode(pinB, OUTPUT);
	pinMode(pinC, OUTPUT);
	pinMode(pinD, OUTPUT);
	
	pinMode(PIN_BTN, INPUT);
	pinMode(PIN_LED, OUTPUT);
    
    g_led_is_on = false;

	delayMS(50);    // wait for a stable status

    ready = paused = feeding = false;
    display_timer = TIME_SCREEN_SAVER;
    counter_on_lcd = 0;
    
    if (on_boot) {
        turn_on_led();
        delay(60 * 1000);
    }
    turn_off_led();
    
    pthread_t thread_timer;
    pthread_create(&thread_timer, NULL, timer_raw, NULL);

    pthread_t thread_btn;
    pthread_create(&thread_btn, NULL, btn_monitor, NULL);

    ready = true;
    
    while (!done) {
        if ((demo_mode && !paused) || should_feed_now()) {
            feeding = true;
            for (int i = 0; i < ONE_WAY_STEPS; i++) {
                rotate(pins, CLOCKWISE);
            }
            delayMS(DURATION_MS);
            for (int i = 0; i < ONE_WAY_STEPS; i++) {
                rotate(pins, COUNTER_CLOCKWISE);
            }
            turn_off_motor (pins);
            feeding = false;
            feeded_today = true;
            counter_on_lcd = ++counter;
        }
        for (int i = 0; i < (demo_mode ? 8 : 60); ++i) {
            if (done)
                break;
            delayMS(1000);
        }
    }

    pthread_join(thread_timer, NULL);
    pthread_join(thread_btn, NULL);

	return 0;
}

/* Suspend execution for x milliseconds intervals.
*  @param ms Milliseconds to sleep.
*/
void delayMS(int x) {
	usleep(x * 1000);
}

/* Rotate the motor.
*  @param pins     A pointer which points to the pins number array.
*  @param direction  CLOCKWISE for clockwise rotation, COUNTER_CLOCKWISE for counter clockwise rotation.
*/
void rotate(int* pins, int direction) {
	for (int i = 0; i < 4; i++) {
		if (CLOCKWISE == direction) {
			for (int j = 0; j < 4; j++) {
				if (j == i) {
				  digitalWrite(pins[3 - j], 1); // output a high level
				} 
				else {
				  digitalWrite(pins[3 - j], 0); // output a low level
				}
			}
		} else if (COUNTER_CLOCKWISE == direction) {
		   for (int j = 0; j < 4; j++) {
				 if (j == i) {
				   digitalWrite(pins[j], 1); // output a high level
				 } 
				 else {
				   digitalWrite(pins[j], 0); // output a low level
				 }
		   }
		}
		delayMS(2); //was 4
	}
}

void turn_off_motor(int *pins) {
	for (int i = 0; i < 4; i++) {
		digitalWrite(pins[i], 0);	
	}
}


bool should_feed_now() {
    struct tm *timeinfo ;
    time_t rawtime ;
    
	rawtime = time (NULL) ;
	timeinfo = localtime(&rawtime);
	
	bool its_time = (timeinfo->tm_hour == TIME_HOUR_FEED) && (timeinfo->tm_min < 10);
	if (!its_time) {
		feeded_today = false;
	}
	return its_time && !feeded_today;
}


