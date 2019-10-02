
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
#define PIN_BTN4    0

#define PIN_BUZZER	16

#define PIN_LED_R   28
#define PIN_LED_G   21
#define PIN_LED_Y   6

#define MAX_KEY_BUFF       10
#define KEY_BTN1_PRESSED    1
#define KEY_BTN2_PRESSED    2
#define KEY_BTN3_PRESSED    3
#define KEY_BTN4_PRESSED    4
#define KEY_BTN1_LONG_PRESSED    11
#define KEY_BTN2_LONG_PRESSED    12
#define KEY_BTN3_LONG_PRESSED    13
#define KEY_BTN4_LONG_PRESSED    14

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
#define TIMER_STOPWATCH         7
#define TIMER_STOPWATCH_STOP    8
#define TIMER_HARD_RESET        99

#define DURATION_TV             15 //minutes
#define DURATION_LEARNING       15 //minutes
#define DURATION_BREAK          5  //minutes
#define DURATION_BEEP           20 //seconds
#define REMINDER_INTERVAL       300 //seconds, 5 minutes

#ifndef bool
#define bool    int
#define true    1
#define false   0
#endif

static bool done = false, buzzer_on = false;
static int cadence;
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
    cadence = 0;
    buzzer_on = true;
}

void turn_off_buzzer() {
    buzzer_on = false;
    digitalWrite(PIN_BUZZER, LOW);
}

void beep() {
    if (buzzer_on)
        return;

    digitalWrite(PIN_BUZZER, HIGH);
    delay_ms(200);
    digitalWrite(PIN_BUZZER,LOW);
}

void log_event(int event, long long elapsed_ms) {
    int elapsed_minutes = elapsed_ms / 1000 / 60;
    int elapsed_seconds = (elapsed_ms / 1000) % 60;

    char description[255];
    switch (event) {
        case TIMER_TV:
        strcpy (description, "Started watching TV");
        break;
        case TIMER_TV_TIMEOUT_:
        sprintf(description, "Finished watching TV. %02d:%02d", elapsed_minutes, elapsed_seconds);
        break;
        case TIMER_LEARNING:
        strcpy (description, "Started spelling bee");
        break;
        case TIMER_LEARNING_TIMEOUT_:
        sprintf(description, "Finished spelling bee. %02d:%02d", elapsed_minutes, elapsed_seconds);
        break;
        case TIMER_BREAK:
        strcpy (description, "Break time");
        break;
        case TIMER_BREAK_TIMEOUT_:
        sprintf(description, "Break timed out. %02d:%02d", elapsed_minutes, elapsed_seconds);
        break;
        case TIMER_STOPWATCH:
        strcpy (description, "Start reading");
        break;
        case TIMER_STOPWATCH_STOP:
        sprintf(description, "Finished reading. %02d:%02d", elapsed_minutes, elapsed_seconds);
        break;
        case TIMER_HARD_RESET:
        strcpy(description, "Hard reset");
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
        fprintf(pFile, "%s<br>\r\n", buffer);
        fclose(pFile);
    }
}

void generate_one_key(int event) {
    keys[tail]=event;
    tail = (tail + 1) % MAX_KEY_BUFF;
    beep();
}

void* timer_daemon(void *arg) {
    tail = header = 0; //the buffer is empty
    int delay = 200, steps = 2000 / delay; //every two seconds
    int btn_states[]={BTN_LOW, BTN_LOW, BTN_LOW, BTN_LOW};
    int btn_pins[]={PIN_BTN1,PIN_BTN2,PIN_BTN3, PIN_BTN4};
    
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
        if (buzzer_on) {
            if (cadence == steps/2) {
                digitalWrite (PIN_BUZZER, LOW);
            }
            else if (cadence == 0) {
                digitalWrite (PIN_BUZZER, HIGH);
            }
            cadence = (cadence + 1) % steps;
        }
        delay_ms(delay);
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
    pinMode(PIN_BTN4, INPUT);
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
    

    pthread_t thread_daemon;
    pthread_create(&thread_daemon, NULL, timer_daemon, NULL);

    unsigned long long start_at, timeout_at = 0, now = 0;
    int counter = 0, reminder = 0, alt = 0, beep_counter = 0;
    while (!done) {
        int key_event = 0;
        if (header != tail) {
            key_event = keys[header];
            header = (header + 1) % MAX_KEY_BUFF;
        }
        now = get_current_time();
        //hard reset
        if (key_event == KEY_BTN1_LONG_PRESSED || key_event == KEY_BTN2_LONG_PRESSED ||
                    key_event == KEY_BTN3_LONG_PRESSED || key_event == KEY_BTN4_LONG_PRESSED) {
            log_event(TIMER_HARD_RESET, 0);
            if (timer_state == TIMER_TV) {
                log_event(TIMER_TV_TIMEOUT_, now - start_at);
            }
            else if (timer_state == TIMER_LEARNING) {
                log_event(TIMER_LEARNING_TIMEOUT_, now - start_at);
            }
            else if (timer_state == TIMER_BREAK) {
                log_event(TIMER_BREAK_TIMEOUT_, now - start_at);
            }
            else if (timer_state == TIMER_STOPWATCH) {
                log_event(TIMER_STOPWATCH_STOP, now - start_at);
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
                    digitalWrite(PIN_LED_Y, HIGH);
                    log_event(TIMER_BREAK, 0);
                }
                else if (key_event == KEY_BTN4_PRESSED) {//stopwatch
                    timer_state = TIMER_STOPWATCH;
                    timer_sub_state = 0;
                    start_at = now;
                    counter= reminder = alt = beep_counter = 0;
                    turn_off_all_leds();
                    digitalWrite(PIN_LED_R, HIGH);
                    log_event(TIMER_STOPWATCH, 0);
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
                if (now >= timeout_at) {
                    timer_state = TIMER_IDLE;
                    timer_sub_state = TIMER_BREAK_TIMEOUT_;
                    timeout_at = get_current_time() + DURATION_BEEP * 1000;
                    digitalWrite(PIN_LED_Y, HIGH); //Y always on
                    turn_on_buzzer(); //buzzer on
                    counter = 0;
                    log_event(TIMER_BREAK_TIMEOUT_, now - start_at);
                }
                else {
                    digitalWrite(PIN_LED_Y, (counter & 1) ? LOW: HIGH);
                }
                break;
            case TIMER_STOPWATCH:
                if (key_event == KEY_BTN4_PRESSED) {
                    timer_state = TIMER_IDLE;
                    turn_off_buzzer();
                    turn_off_all_leds();
                    log_event(TIMER_STOPWATCH_STOP, now - start_at);
                }
                else {
                    alt = (alt + 1) % 3;
                    turn_off_all_leds();
                    if (alt == 0)
                        digitalWrite(PIN_LED_R, HIGH);
                    else if (alt == 1)
                        digitalWrite(PIN_LED_G, HIGH);
                    else
                        digitalWrite(PIN_LED_Y, HIGH);

                    if (beep_counter > 0 && --beep_counter <= 0) {
                        turn_off_buzzer();
                    }                    
                    //Reminder every N seconds
                    int to_reminder = (now - start_at) / 1000 / REMINDER_INTERVAL;
                    if (to_reminder > 0 && reminder < to_reminder) {
                        reminder = to_reminder;
                        beep_counter = reminder * 2;
                        turn_on_buzzer();
                    }
                }
                break;
        }
        delay_ms(1000);
        counter = (counter + 1) & 1;
    }
    turn_off_all_leds();
    turn_off_buzzer();
    
    pthread_join(thread_daemon, NULL);
	return 0;
}
