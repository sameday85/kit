#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <wiringPi.h>

#define PIN_LED             15
#define PIN_AIR_PUMP        16

#define STATE_ON            1
#define STATE_OFF           0
#define STATE_ALWAYS_OFF    99
#define STATE_ALWAYS_ON     100

using namespace std;

//pair of start time and end time
int g_led_schedule[]={1800,2100};//1700 means 17:00
int g_air_pump_schedule[]={730,800,1700,1800};
bool g_done = false;

void handle_signal(int signal) {
    // Find out which signal we're handling
    switch (signal) {
		case SIGINT://key 3
			g_done=true;
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

//suspend execution for millisecond intervals
void delay_ms(int x) {
    usleep(x * 1000);
}

int get_current_time_flat(int *day_of_week) {
    struct tm *timeinfo ;
    time_t rawtime ;
    rawtime = time (NULL) ;
    timeinfo = localtime(&rawtime);
    *day_of_week=timeinfo->tm_wday;//0-saturday
    return timeinfo->tm_hour * 100 + timeinfo->tm_min;
}

void turn_led_on() {
    digitalWrite(PIN_LED, LOW);    
}

void turn_led_off() {
    digitalWrite(PIN_LED, HIGH);
}

void turn_air_pump_on() {
    digitalWrite(PIN_AIR_PUMP, LOW);
}

void turn_air_pump_off() {
    digitalWrite(PIN_AIR_PUMP, HIGH);
}

bool should_turn_led_on() {
    int day_of_week;
    int ts = get_current_time_flat(&day_of_week);
    bool be_on = false;
    for (int i = 0; i < sizeof(g_led_schedule)/sizeof(int); i+=2) {
        if (ts >= g_led_schedule[i] && ts < g_led_schedule[i+1]) {
            be_on = true;
            break;
        }
    }
    return be_on;
}

bool should_turn_led_off() {
    return !should_turn_led_on();
}

bool should_turn_air_pump_on(){
    int day_of_week;
    int ts = get_current_time_flat(&day_of_week);
    bool be_on = false;
    for (int i = 0; i < sizeof(g_air_pump_schedule)/sizeof(int); i+=2) {
        if (ts >= g_air_pump_schedule[i] && ts < g_air_pump_schedule[i+1]) {
            be_on = true;
            break;
        }
    }
    //No air pump in saturday evening and sunday morning because the filter is on
    if (be_on && (((day_of_week == 6) && (ts > 1200)) || ((day_of_week == 0) && (ts < 1200)))) {
        be_on = false;
    }
    return be_on;
}

bool should_turn_air_pump_off(){
    return !should_turn_air_pump_on();
}

//gcc timer.cpp -lstdc++ -lwiringPi -lwiringPiDev
int main(int argc, char *argv[])
{
    g_done = false;
    hook_signal();//ctr+c to stop the program
    
    wiringPiSetup() ;
    pinMode (PIN_LED, OUTPUT) ; 
    pinMode (PIN_AIR_PUMP, OUTPUT) ; 

    //turn everything off
    turn_led_off();
    turn_air_pump_off();
    
    //set initial states
    int m_led_state = STATE_OFF;
    int m_air_pump_state = STATE_OFF;
    int interval=1000;
    
    //command line to turn the led or the pump on. Only one be on
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-boot") == 0) { //autorun at boot up
            //wait for the pi zero to be fully boot up
            delay_ms(2 * 60 * 1000);
            interval=2000;
        }
        else if (strcmp(argv[i], "-led") == 0) {
            turn_led_on();
            m_led_state = STATE_ALWAYS_ON;
            m_air_pump_state = STATE_ALWAYS_OFF;
        }
        else if (strcmp(argv[i], "-pump") == 0) {
            turn_air_pump_on();
            m_air_pump_state = STATE_ALWAYS_ON;
            m_led_state = STATE_ALWAYS_OFF;
        }
    }
    
    while (!g_done) {
        //led operation
        if (m_led_state == STATE_OFF) {
            if (should_turn_led_on()) {
                turn_led_on();
                m_led_state = STATE_ON;
            }
        }
        else if (m_led_state == STATE_ON) {
            if (should_turn_led_off()) {
                turn_led_off();
                m_led_state = STATE_OFF;
            }
        }
        //air pump operation
        if (m_air_pump_state == STATE_OFF) {
            if (should_turn_air_pump_on()) {
                turn_air_pump_on();
                m_air_pump_state = STATE_ON;
            }
        }
        else if (m_air_pump_state == STATE_ON) {
            if (should_turn_air_pump_off()) {
                turn_air_pump_off();
                m_air_pump_state = STATE_OFF;
            }
        }
        delay_ms(interval);
    }
    turn_led_off();
    turn_air_pump_off();
}

