#include <wiringPi.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include "pi_lcd.h"

using namespace std; 
/*
 * 
 * 
 * Display Rapberry Pi CPU temperature
 * 
 * 
 */

#define PIN_BUZZER      16

static bool g_done;

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

float cpu_temperature() {
    ifstream input("/sys/class/thermal/thermal_zone0/temp", ifstream::in); //The input stream
    string line;
    getline(input, line, '\n');
    input.close();
    
    return stoi(line) / 1000.0f;
}

//make a long or shot buzzle
void buzzle (bool long_time) {
    int duration = long_time ? 2000 : 300;
    for(int i=0;i<(duration/2);i++) { 
        digitalWrite(PIN_BUZZER,HIGH);// sound 
        delay(1);//delay1ms 
        digitalWrite(PIN_BUZZER,LOW);//not sound 
        delay(1);//ms delay
    }
    digitalWrite(PIN_BUZZER, LOW);
}            

//gcc -o cpu ../cpu.cpp ../pi_lcd.c -lwiringPi -lwiringPiDev
int main( void )
{
    g_done = false;
    hook_signal();
    
    wiringPiSetup();
    init_lcd();
    pinMode(PIN_BUZZER, OUTPUT);
    
    struct tm *timeinfo ;
    time_t rawtime ;
    char buffer[128];
    
    float temperature;

    while ( !g_done ) {
        //current time
        rawtime = time (NULL) ;
        timeinfo = localtime(&rawtime) ;
        strftime(buffer,sizeof (buffer),"%H:%M:%S %a",timeinfo);
        show_lcd_center(0, buffer);
        
        temperature = cpu_temperature();
        sprintf(buffer, "CPU %0.1f", temperature);
        show_lcd_center(1, buffer);
        
        if (temperature > 80)
        buzzle(true);
        
        delay(1000);
    }
    
    deinit_lcd();

    return(0);
}
