
#include <wiringPi.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>

#define PIN_BTN1    29
#define PIN_BTN2    26
#define PIN_BTN3    10

#define PIN_BUZZER	16

#define PIN_LED_R   28
#define PIN_LED_G   21
#define PIN_LED_Y   6

#define MAX_KEY_BUFF       10
#define KEY_BTN1_PRESSED    1
#define KEY_BTN2_PRESSED    2
#define KEY_BTN3_PRESSED    3
#define KEY_BTN1_LONG_PRESSED    11
#define KEY_BTN2_LONG_PRESSED    12
#define KEY_BTN3_LONG_PRESSED    13

#define BTN_LOW         10
#define BTN_HIGH        11
#define BTN_IGNORE      12

#define TIMER_IDLE              0
#define TIMER_TV                1
#define TIMER_TV_TIMEOUT        2
#define TIMER_LEARNING          3
#define TIMER_LEARNING_TIMEOUT  4
#define TIMER_BREAK             5
#define TIMER_BREAK_TIMEOUT     6

#define DURATION_TV             15 //minutes
#define DURATION_LEARNING       15 //minutes
#define DURATION_BREAK          5  //minutes
#define DURATION_BEEP           20  //seconds

#ifndef bool
#define bool    int
#define true    1
#define false   0
#endif


void delay_ms(int x);

static bool done = false, buzzer_on = false;
static int keys[MAX_KEY_BUFF], tail, header;

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

void delay_ms(int x) {
	usleep(x * 1000);
}

unsigned long long get_current_time() {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (unsigned long long)(tv.tv_sec) * 1000 + (unsigned long long)(tv.tv_usec) / 1000;
}

void turn_off_all_leds() {
    digitalWrite(PIN_LED_R, LOW);
    digitalWrite(PIN_LED_G, LOW);
    digitalWrite(PIN_LED_Y, LOW);
}

void turn_on_buzzer() {
    digitalWrite(PIN_BUZZER, HIGH);
    buzzer_on = true;
}

void turn_off_buzzer() {
    digitalWrite(PIN_BUZZER, LOW);
    buzzer_on = false;
}

void beep() {
    if (buzzer_on)
        return;

    turn_on_buzzer();
    delay_ms(100);
    turn_off_buzzer();
}

void generate_one_key(int event) {
    keys[tail]=event;
    tail = (tail + 1) % MAX_KEY_BUFF;
    beep();
}

void* btn_monitor(void *arg) {
    tail = header = 0; //the buffer is empty
    int btn_states[]={BTN_LOW, BTN_LOW, BTN_LOW};
    int btn_pins[]={PIN_BTN1,PIN_BTN2,PIN_BTN3};
    
    int counter = 0, counter_as_long = 10;
	while (!done) {
        for (int i = 0; i < sizeof (btn_states) / sizeof (int); ++i) {
            int state = digitalRead(btn_pins[i]);
            if (state == HIGH) {//button pressed
                if (btn_states[i] == BTN_LOW) {
                    btn_states[i] = BTN_HIGH;
                    counter = 0;
                }
                else if (btn_states[i] == BTN_HIGH) {
                    if (++counter >= counter_as_long) {
                        generate_one_key(KEY_BTN1_LONG_PRESSED + i);
                        btn_states[i] = BTN_IGNORE;
                    }
                }
            }
            else if (state == LOW) {
                if (btn_states[i] == BTN_HIGH) {
                    generate_one_key(KEY_BTN1_PRESSED+i);
                }
                btn_states[i] = BTN_LOW;
            }
        }
        delay_ms(200);
	}
	return NULL;
}

//gcc -o daily daily.c -lpthread -lwiringPi -lwiringPiDev -lm
int main(int argc, char *argv[]) 
{
	hook_signal();
    
    wiringPiSetup();
    pinMode(PIN_BTN1, INPUT);
    pinMode(PIN_BTN2, INPUT);
    pinMode(PIN_BTN3, INPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_Y, OUTPUT);
    
    turn_off_all_leds();
    turn_off_buzzer();
    
    int timer_state = TIMER_IDLE;
    
    pthread_t thread_btn;
    pthread_create(&thread_btn, NULL, btn_monitor, NULL);
    
    unsigned long long timeout_at = 0, now = 0;
    unsigned long long counter = 0;
    bool beep_on = false;
    while (!done) {
        int key_event = 0;
        if (header != tail) {
            key_event = keys[header];
            header = (header + 1) % MAX_KEY_BUFF;
        }
        //hard reset
        if (key_event == KEY_BTN1_LONG_PRESSED || 
                key_event == KEY_BTN2_LONG_PRESSED ||
                    key_event == KEY_BTN3_LONG_PRESSED) {
            turn_off_all_leds();
            turn_off_buzzer();
            timer_state = TIMER_IDLE;
            key_event = 0;
        }
        now = get_current_time();
        switch (timer_state) {
            case TIMER_IDLE:
                if (key_event == KEY_BTN1_PRESSED) {//tv time
                    timer_state = TIMER_TV;
                    timeout_at = get_current_time() + DURATION_TV * 60 * 1000;
                    counter = 0;
                    turn_off_all_leds();
                    digitalWrite(PIN_LED_R, HIGH);
                }
                else if (key_event == KEY_BTN2_PRESSED) {//learning time
                    timer_state = TIMER_LEARNING;
                    timeout_at = get_current_time() + DURATION_LEARNING * 60 * 1000;
                    counter = 0;
                    turn_off_all_leds();
                    digitalWrite(PIN_LED_G, HIGH);
                }
                else if (key_event == KEY_BTN3_PRESSED) {//break time
                    timer_state = TIMER_BREAK;
                    timeout_at = get_current_time() + DURATION_BREAK * 60 * 1000;
                    counter = 0;
                    turn_off_all_leds();
                    digitalWrite(PIN_LED_Y, HIGH);
                }
                break;
            case TIMER_TV:
                if (now >= timeout_at) {
                    timer_state = TIMER_TV_TIMEOUT;
                    timeout_at = get_current_time() + DURATION_BEEP * 1000;
                    digitalWrite(PIN_LED_R, HIGH); //R always on
                    turn_on_buzzer(); //buzzer on
                    counter = 0;
                    beep_on = true;
                }
                else {
                    digitalWrite(PIN_LED_R, (counter & 1) ? LOW: HIGH);
                }
                break;
            case TIMER_TV_TIMEOUT:
                if (now >= timeout_at) {
                    turn_off_buzzer(); //stop beeping, R led is still on
                    timer_state = TIMER_IDLE;
                }
                break;
            case TIMER_LEARNING:
                if (now >= timeout_at) {
                    timer_state = TIMER_LEARNING_TIMEOUT;
                    timeout_at = get_current_time() + DURATION_BEEP * 1000;
                    digitalWrite(PIN_LED_G, HIGH); //G always on
                    turn_on_buzzer(); //buzzer on
                    counter = 0;
                    beep_on = true;
                }
                else {
                    digitalWrite(PIN_LED_G, (counter & 1) ? LOW: HIGH);
                }
                break;
            case TIMER_LEARNING_TIMEOUT:
                now = get_current_time();
                if (now >= timeout_at) {
                    turn_off_buzzer(); //stop beeping, G led is still on
                    timer_state = TIMER_IDLE;
                }
                break;
            case TIMER_BREAK:
                now = get_current_time();
                if (now >= timeout_at) {
                    timer_state = TIMER_BREAK_TIMEOUT;
                    timeout_at = get_current_time() + DURATION_BEEP * 1000;
                    digitalWrite(PIN_LED_Y, HIGH); //Y always on
                    turn_on_buzzer(); //buzzer on
                    counter = 0;
                    beep_on = true;
                }
                else {
                    digitalWrite(PIN_LED_Y, (counter & 1) ? LOW: HIGH);
                }
                break;
            case TIMER_BREAK_TIMEOUT:
                now = get_current_time();
                if (now >= timeout_at) {
                    turn_off_buzzer(); //stop beeping, Y led is still on
                    timer_state = TIMER_IDLE;
                }
                break;
        }
        delay_ms(1000);
        counter = (counter + 1) & 1;
    }
    turn_off_all_leds();
    turn_off_buzzer();
    
    pthread_join(thread_btn, NULL);
	return 0;
}


