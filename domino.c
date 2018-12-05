#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include <softPwm.h>

#include "common.h"
#include "pi_lirc.h"

#define PIN_MOTOR1  13


#define PIN_PWM_2   2
#define PIN_DIR_2   3


//LED
#define ledR    21
#define ledG    22
#define ledB    23

static bool done = false;
static bool paused = true;

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

void* keyboard_raw(void *arg) {
    int failed;
    char *line;
    if (!pi_lirc_init())
        return NULL;

    line = pi_lirc_next_key();
    failed = 0;
    while (strlen(line) > 0) {
        printf("==>%s\n", line);
        if(strcmp (line,"00FF30CF") == 0) { //key_1
            paused = false; failed = 0;
        }
        else if (strcmp (line, "00FF18E7") == 0) {//key_2
            paused = true; failed = 0;
            
        }
        else if (strcmp (line,"00FF7A85") == 0) {//key_3
            done=true; failed = 0;
        }
        else if (++failed > 2) {//any other keys
            done=true;
        }
        if (done)
            break;
        line = pi_lirc_next_key();
    }
    pi_lirc_close();
    done = true;
    printf("Keyboard done\n");
    
    return NULL;
}

void run_motor() {
    bool last_status;
    

    //set direction 
    pinMode(PIN_MOTOR1, OUTPUT);
    pinMode(PIN_DIR_2, OUTPUT);

    
    digitalWrite(PIN_MOTOR1, LOW);
    digitalWrite(PIN_DIR_2, LOW);
    softPwmCreate (PIN_PWM_2, 0, 100);
    
    last_status = paused;
    while (!done) {
        if (last_status != paused) {
            last_status = paused;
            digitalWrite(ledR, paused ? HIGH: LOW);
            digitalWrite(ledG, paused ? LOW: HIGH);
        }
        if (!paused) {
            //move forward
            digitalWrite(PIN_MOTOR1, HIGH);
            delay(160);
            //stop
            digitalWrite(PIN_MOTOR1, LOW);
            
            //start the second motor
            digitalWrite(PIN_DIR_2, HIGH);
            softPwmWrite (PIN_PWM_2, 56); //400ms @ duty 60
            delay(400);
            digitalWrite(PIN_DIR_2, LOW);
            delay(400);
            softPwmWrite (PIN_PWM_2, 0);
        }
        delay(200);
    }
    digitalWrite(PIN_MOTOR1, LOW);
    
    softPwmWrite (PIN_PWM_2, 0);
    delay(200);
}

//gcc -o domino.out -Wall domino.c pi_lirc.c -lwiringPi -lwiringPiDev -lpthread
int main (void)
{
    pthread_t thread_keyboard;
    pthread_create(&thread_keyboard, NULL, keyboard_raw, NULL);
    
    srand(time(NULL));   // should only be called once
    hook_signal();
    wiringPiSetup();
    
    pinMode(ledR, OUTPUT);
    digitalWrite(ledR, HIGH);
    pinMode(ledG, OUTPUT);
    digitalWrite(ledG, LOW);

    run_motor();
    digitalWrite(ledR, LOW);
    digitalWrite(ledG, LOW);
    
    pthread_join(thread_keyboard, NULL);
}
