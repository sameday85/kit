
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
#define TIMER_TV_TIMEOUT_       2
#define TIMER_LEARNING          3
#define TIMER_LEARNING_TIMEOUT_ 4
#define TIMER_BREAK             5
#define TIMER_BREAK_TIMEOUT_    6

#define DURATION_TV             15 //minutes
#define DURATION_LEARNING       15 //minutes
#define DURATION_BREAK          5  //minutes
#define DURATION_BEEP           20 //seconds

#ifndef bool
#define bool    int
#define true    1
#define false   0
#endif

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

void log_event(int event, long long elapsed_ms) {
    int elapsed_minutes = elapsed_ms / 1000 / 60;

    char description[255];
    switch (event) {
        case TIMER_TV:
        strcpy (description, "Started watching TV");
        break;
        case TIMER_TV_TIMEOUT_:
        sprintf(description, "Finished watching TV. %d minutes", elapsed_minutes);
        break;
        case TIMER_LEARNING:
        strcpy (description, "Started learning");
        break;
        case TIMER_LEARNING_TIMEOUT_:
        sprintf(description, "Finished learning. %d minutes", elapsed_minutes);
        break;
        default:
        sprintf(description, "Unknown event: %d", event);
        break;
    }

    char buffer[1024];
    struct tm *timeinfo ;
    time_t rawtime ;
    rawtime = time (NULL) ;
    timeinfo = localtime(&rawtime);
    sprintf (buffer, "[%02d/%02d %02d:%02d:%02d]%s", timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, description);

    FILE *pFile = fopen("/var/www/html/album/daily.html", "a");
    if (pFile) {
        fprintf(pFile, "%s<br>", buffer);
        fclose(pFile);
    }
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
    bool on_boot = false;
	for (int i = 1; i < argc; ++i) {
		if (strcmp (argv[i], "-boot") == 0)
			on_boot = true;
	}
    
	hook_signal();
    wiringPiSetup();
    pinMode(PIN_BTN1, INPUT);
    pinMode(PIN_BTN2, INPUT);
    pinMode(PIN_BTN3, INPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_Y, OUTPUT);
    
    turn_off_buzzer();
    if (on_boot) {
        digitalWrite(PIN_LED_R, HIGH);
        digitalWrite(PIN_LED_G, HIGH);
        digitalWrite(PIN_LED_Y, HIGH);

        delay(30 * 1000);//30 seconds
    }   
    turn_off_all_leds();
    
    int timer_state = TIMER_IDLE, timer_sub_state = 0;
    header = tail = 0;

    pthread_t thread_btn;
    pthread_create(&thread_btn, NULL, btn_monitor, NULL);
    
    unsigned long long start_at, timeout_at = 0, now = 0;
    int counter = 0;
    while (!done) {
        int key_event = 0;
        if (header != tail) {
            key_event = keys[header];
            header = (header + 1) % MAX_KEY_BUFF;
        }
        now = get_current_time();
        //hard reset
        if (key_event == KEY_BTN1_LONG_PRESSED || 
                key_event == KEY_BTN2_LONG_PRESSED ||
                    key_event == KEY_BTN3_LONG_PRESSED) {
            if (timer_state == TIMER_TV) {
                log_event(TIMER_TV_TIMEOUT_, now - start_at);
            }
            else if (timer_state == TIMER_LEARNING) {
                log_event(TIMER_LEARNING_TIMEOUT_, now - start_at);
            }
            turn_off_all_leds();
            turn_off_buzzer();
            timer_state = TIMER_IDLE;
            timer_sub_state = 0;
            key_event = 0;
        }
        switch (timer_state) {
            case TIMER_IDLE:
                if (timer_sub_state) {
                    if ((key_event == KEY_BTN1_PRESSED && timer_sub_state == TIMER_TV_TIMEOUT_) ||
                            (key_event == KEY_BTN2_PRESSED && timer_sub_state == TIMER_LEARNING_TIMEOUT_) ||
                                (key_event == KEY_BTN3_PRESSED && timer_sub_state == TIMER_BREAK_TIMEOUT_)) {
                        timer_sub_state = 0;
                        turn_off_all_leds();
                        turn_off_buzzer();

                        key_event = 0;
                    }
                    if (timer_sub_state && (now >= timeout_at)) {
                        timer_sub_state = 0;
                        turn_off_all_leds();
                        turn_off_buzzer();
                    }
                }
                if (key_event == KEY_BTN1_PRESSED) {//tv time
                    timer_state = TIMER_TV;
                    timer_sub_state = 0;
                    start_at = now;
                    timeout_at = now + DURATION_TV * 60 * 1000;
                    counter = 0;
                    turn_off_all_leds();
                    turn_off_buzzer();
                    digitalWrite(PIN_LED_R, HIGH);
                    log_event(TIMER_TV, 0);
                }
                else if (key_event == KEY_BTN2_PRESSED) {//learning time
                    timer_state = TIMER_LEARNING;
                    timer_sub_state = 0;
                    start_at = now;
                    timeout_at = now + DURATION_LEARNING * 60 * 1000;
                    counter = 0;
                    turn_off_all_leds();
                    turn_off_buzzer();
                    digitalWrite(PIN_LED_G, HIGH);
                    log_event(TIMER_LEARNING, 0);
                }
                else if (key_event == KEY_BTN3_PRESSED) {//break time
                    timer_state = TIMER_BREAK;
                    timer_sub_state = 0;
                    start_at = now;
                    timeout_at = now + DURATION_BREAK * 60 * 1000;
                    counter= 0;
                    turn_off_all_leds();
                    turn_off_buzzer();
                    digitalWrite(PIN_LED_Y, HIGH);
                }
                break;
            case TIMER_TV:
                if (now >= timeout_at) {
                    timer_state = TIMER_IDLE;
                    timer_sub_state = TIMER_TV_TIMEOUT_;
                    timeout_at = get_current_time() + DURATION_BEEP * 1000;
                    digitalWrite(PIN_LED_R, HIGH); //R always on
                    turn_on_buzzer(); //buzzer on
                    counter = 0;
                    log_event(TIMER_TV_TIMEOUT_, now - start_at);
                }
                else {
                    digitalWrite(PIN_LED_R, (counter & 1) ? LOW: HIGH);
                }
                break;
            case TIMER_LEARNING:
                if (now >= timeout_at) {
                    timer_state = TIMER_IDLE;
                    timer_sub_state = TIMER_LEARNING_TIMEOUT_;
                    timeout_at = get_current_time() + DURATION_BEEP * 1000;
                    digitalWrite(PIN_LED_G, HIGH); //G always on
                    turn_on_buzzer(); //buzzer on
                    counter = 0;
                    log_event(TIMER_LEARNING_TIMEOUT_, now - start_at);
                }
                else {
                    digitalWrite(PIN_LED_G, (counter & 1) ? LOW: HIGH);
                }
                break;
            case TIMER_BREAK:
                now = get_current_time();
                if (now >= timeout_at) {
                    timer_state = TIMER_IDLE;
                    timer_sub_state = TIMER_BREAK_TIMEOUT_;
                    timeout_at = get_current_time() + DURATION_BEEP * 1000;
                    digitalWrite(PIN_LED_Y, HIGH); //Y always on
                    turn_on_buzzer(); //buzzer on
                    counter = 0;
                }
                else {
                    digitalWrite(PIN_LED_Y, (counter & 1) ? LOW: HIGH);
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


