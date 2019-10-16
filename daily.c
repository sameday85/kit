
#include <wiringPi.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include "TM1637.h"

#define LAUNCH_MODE_CMD_LINE        0
#define LAUNCH_MODE_ON_BOOT         1

#define PIN_BTN1    29
#define PIN_BTN2    26
#define PIN_BTN3    10
#define PIN_BTN4    0
#define PIN_LDR     15
#define PIN_BUZZER	16

#define PIN_LED_R   28
#define PIN_LED_G   21
#define PIN_LED_Y   6

#define PIN_CLK     9//pins definitions for TM1637 and can be changed to other ports       
#define PIN_DIO     8

#define PIN_DHT11   1

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
#define TIMER_TV_PAUSED         9
#define TIMER_LEARNING_PAUSED   10

#define TIMER_MAINTENANCE_      98
#define TIMER_HARD_RESET        99

#define DURATION_TV             15 //minutes
#define DURATION_LEARNING       15 //minutes
#define DURATION_BREAK          5  //minutes
#define DURATION_BEEP           20 //seconds
#define REMINDER_INTERVAL       300 //seconds, 5 minutes

#define LOG_FILE_MAX_LINE       100
#define LOG_FILE_PATH           "/var/www/html/album/daily.html"
#define LOG_MAINTENANCE_HOUR    2 //3am

#define DISPLAY_OFF             0
#define DISPLAY_SYS_TIME        1
#define DISPLAY_COUNT           2

#define TOGGLE_NONE             0
#define TOGGLE_TIME             1
#define TOGGLE_TEMPERATURE      2
#define TOGGLE_HUMIDITY         3
#define TOGGLE_OUTSIDE_TEMPERATURE      4

#define SYS_TIME_ON_IF_IDLE     60  //seconds, must be bigger than DURATION_BEEP
#define SYS_TIME_OFF_HOUR       0 //am
#define SYS_TIME_ON_HOUR        7 //am

#ifndef bool
#define bool    int
#define true    1
#define false   0
#endif

static int launch_mode, display_mode;
static bool done = false, buzzer_on = false;
static int cadence, beeps;
static int keys[MAX_KEY_BUFF], tail, header;
static int last_dht11_timestamp, last_outside_timestamp;
static int temperature, humidity, outside_temperature;

extern bool frederick(int *outside_temperature);

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
    if (x > 0)
	    usleep(x * 1000);
}

unsigned long long get_current_time() {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (unsigned long long)(tv.tv_sec) * 1000 + (unsigned long long)(tv.tv_usec) / 1000;
}

void get_hour_day(int *hour, int *mday) {
    struct tm *timeinfo ;
    time_t rawtime ;

    rawtime = time (NULL) ;
    timeinfo = localtime(&rawtime);

    *hour=timeinfo->tm_hour;
    *mday=timeinfo->tm_mday;
}

void turn_off_all_leds() {
    digitalWrite(PIN_LED_R, LOW);
    digitalWrite(PIN_LED_G, LOW);
    digitalWrite(PIN_LED_Y, LOW);
}

void turn_on_buzzer(int total_beeps) {
    buzzer_on = true;
    digitalWrite(PIN_BUZZER, HIGH);
    cadence = 0;
    beeps = total_beeps;
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

//daylight: around 100ms, dark: >200ms
int get_ldr_measurement() {
    pinMode (PIN_LDR, OUTPUT);
    digitalWrite(PIN_LDR, LOW); 
    delay(200);

    unsigned long long start = get_current_time();
    pinMode (PIN_LDR, INPUT);
    int wait = 400, step=10, val=digitalRead(PIN_LDR);
    while ((val == LOW) && (wait > 0)) {
        delay(step);
        wait -= step;
        val=digitalRead(PIN_LDR);
    }
    unsigned long long end = get_current_time();
  
    return (int)(end - start);
}

bool read_dht11_dat(int *temperature, int *humidity)
{
    int dht11_dat[5];
    uint8_t laststate   = HIGH, thisstate;
    uint8_t counter     = 0;
    uint8_t j       = 0, i;

    dht11_dat[0] = dht11_dat[1] = dht11_dat[2] = dht11_dat[3] = dht11_dat[4] = 0;
    *temperature = *humidity = 0;

    /* pull pin down for 18 milliseconds */
    pinMode( PIN_DHT11, OUTPUT );
    digitalWrite( PIN_DHT11, LOW );
    delay( 18 );
    /* then pull it up for 40 microseconds */
    digitalWrite( PIN_DHT11, HIGH );
    delayMicroseconds( 40 );
    /* prepare to read the pin */
    pinMode( PIN_DHT11, INPUT );

    /* detect change and read data */
    for ( i = 0; i < 85; i++ ) {
        counter = 0;
		thisstate=digitalRead( PIN_DHT11 );
        while ( thisstate == laststate ) {
            counter++;
            delayMicroseconds( 1 );
            if ( counter == 255 ) {
                break;
            }
			thisstate=digitalRead( PIN_DHT11 );
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
        *temperature = (int)((dht11_dat[2] + dht11_dat[3] / 10.)  * 9. / 5. + 32);
        *humidity = (int)(dht11_dat[0] + dht11_dat[1] / 10.);
        data_available = true;
    }
    
    return data_available;
}

char *load_log_file(size_t *ptr_len) {
    *ptr_len = 0;
    //load the log file into memory
    char *content = NULL; size_t len = 0;
    FILE * file = fopen(LOG_FILE_PATH, "rb");
    if (file) {
        fseek(file, 0, SEEK_END);
        len = (size_t)ftell(file);
        fseek(file, 0, SEEK_SET);
        //allocate memory for loading the whole log file into memory
        content = (len > 0) ? malloc(len + 12) : NULL;
        if (content) {
            fread(content, len, 1, file);
            *ptr_len = len;
        }
        fclose(file);
    }
    return content;
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
        sprintf(description, "Finished watching TV. <font color=\"red\">%02d:%02d</font>", elapsed_minutes, elapsed_seconds);
        break;
        case TIMER_LEARNING:
        strcpy (description, "Started spelling bee");
        break;
        case TIMER_LEARNING_TIMEOUT_:
        sprintf(description, "Finished spelling bee. <font color=\"green\">%02d:%02d</font>", elapsed_minutes, elapsed_seconds);
        break;
        case TIMER_BREAK:
        strcpy (description, "Break time");
        break;
        case TIMER_BREAK_TIMEOUT_:
        sprintf(description, "Break timed out. %02d:%02d", elapsed_minutes, elapsed_seconds);
        break;
        case TIMER_STOPWATCH:
        strcpy (description, "Started reading");
        break;
        case TIMER_STOPWATCH_STOP:
        sprintf(description, "Finished reading. <font color=\"blue\">%02d:%02d</font>", elapsed_minutes, elapsed_seconds);
        break;
        case TIMER_HARD_RESET:
        strcpy(description, "Hard reset");
        break;
        case TIMER_MAINTENANCE_: {
            int total_tv = elapsed_ms >> 16;
            int total_spellingbee= (elapsed_ms & 0xffff) % 60;
            int total_reading = (elapsed_ms & 0xffff) / 60;
            sprintf(description, "====TV %02d, Spellingbee %02d, Reading %02d====", total_tv, total_spellingbee, total_reading);
        }
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
    sprintf (buffer, "[%02d@%02d/%02d %02d:%02d:%02d]%s<br>\r\n", event, timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, description);

    //load the log file into memory
    char *content = NULL; size_t len = 0;
    content = load_log_file (&len);
    if (content) {
        //truncate it if needed
        int lines = 0;
        for (int i = 0; i < len; ++i) {
            if (content[i] == '\n') {
                if (++lines >= LOG_FILE_MAX_LINE) {
                    len = i + 1;
                    break;
                }
            }
        }
    }

    FILE *ouput_file = fopen(LOG_FILE_PATH, "wb");
    if (ouput_file) {
        fputs(buffer, ouput_file);
        if (content && (len > 0))
            fwrite (content, len, 1, ouput_file);
        fclose(ouput_file);
    }
    if (content)
        free (content);
}

int to_int(char *ptr) {
    int value = 0;
    //not a digit
    while (*ptr < '0' || *ptr > '9')
        ++ptr;
    //digits
    while (*ptr >= '0' && *ptr <= '9') {
        value = value * 10 + *ptr - '0';
        ++ptr;
    }
    return value;
}

void daily_maintenance() {
    long long total_tv = 0, total_spellingbee = 0, total_reading = 0;
    char *content = NULL; size_t len = 0;
    content = load_log_file (&len);
    if (content && len > 0) {  
        char *header = content;
        while (header) {
            char *find = strchr(header, '@');
            if (find == NULL)
                break;
            int code = to_int(header);
            if (code == TIMER_MAINTENANCE_)//reached another day
                break;
            char *end = strchr(header, '\n');
            if (end)
                *end = '\0';
            char *open_tag = strchr(header, '>');
            char *close_tag= open_tag ? strchr(open_tag, '<') : NULL;
            if (open_tag && close_tag) {
                *close_tag = '\0';
                char *mid = strchr(open_tag, ':'); //like 12:04
                int minutes = to_int(open_tag);
                int seconds = to_int(mid);
                switch (code) {
                    case TIMER_TV_TIMEOUT_:
                    total_tv += minutes * 60 +seconds;
                    break;
                    case TIMER_LEARNING_TIMEOUT_:
                    total_spellingbee += minutes * 60 + seconds;
                    break;
                    case TIMER_STOPWATCH_STOP:
                    total_reading += minutes * 60 + seconds;
                    break;
                }
            }
            header = end ? end + 1 : NULL;
        }
    }
    if (content)
        free(content);
    total_tv /= 60; //to minutes, up to 32767 minutes
    total_spellingbee /= 60; //up to 60 minutes
    total_reading /= 60; //up to 32767/60=546 miutes
    log_event(TIMER_MAINTENANCE_, (total_tv << 16) | (total_reading * 60 + total_spellingbee));
    
}

void generate_one_key(int event) {
    keys[tail]=event;
    tail = (tail + 1) % MAX_KEY_BUFF;
    beep();
}

int consume_one_key() {
    int key_event = 0;
    if (header != tail) {
        key_event = keys[header];
        header = (header + 1) % MAX_KEY_BUFF;
    }
    return key_event;
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
                if (beeps > 0 && (--beeps <= 0)) {
                    buzzer_on = false;
                }
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

void display(int new_mode, int value) {
    if (new_mode == DISPLAY_OFF && (display_mode != DISPLAY_OFF)) {
        //turn off
        TM1637_point(POINT_OFF);
        TM1637_clearDisplay();
    }
    else if (display_mode == DISPLAY_OFF && new_mode != DISPLAY_OFF) {
        //turn on first
    }
    
    int left_part = 0, right_part = 0;
    int toggle_mode = TOGGLE_TIME;
    if (new_mode == DISPLAY_SYS_TIME) {
        struct tm *timeinfo ;
        time_t rawtime ;
        rawtime = time (NULL) ;
        timeinfo = localtime(&rawtime);
        
        left_part = timeinfo->tm_hour;
        right_part= timeinfo->tm_min;
        uint8_t point_status = (timeinfo->tm_sec & 1) ? POINT_ON : POINT_OFF;
        //display time, temperature & humidity every 5 seconds
        int interval_seconds = 15; //4, 4, 4, 3
        int step = timeinfo->tm_sec % interval_seconds;
        if (step < 4) {
            toggle_mode = TOGGLE_TIME;
        }
        else if (step < 12) { //temperature & humidity
            int mark = timeinfo->tm_min / 5; //update every five minutes
            if (last_dht11_timestamp != mark) {
                if (read_dht11_dat(&temperature, &humidity)) {
                    last_dht11_timestamp = mark;
                }
            }
            if (last_dht11_timestamp == mark) {
                toggle_mode = (step < 8) ? TOGGLE_TEMPERATURE : TOGGLE_HUMIDITY;
                point_status = POINT_OFF;
            }
        }
        else { //outside temperature
            int mark = timeinfo->tm_min / 30; //update every half hour
            if (mark != last_outside_timestamp) {
                if (frederick(&outside_temperature)) {
                    last_outside_timestamp = mark;
                }
            }
            if (last_outside_timestamp == mark) {
                toggle_mode = TOGGLE_OUTSIDE_TEMPERATURE;
                point_status = POINT_OFF;
            }
        }
        TM1637_point(point_status);
    }
    else if (new_mode == DISPLAY_COUNT) {
        if (value < 0)
            value = 0;
        left_part = value / 60;
        right_part= value % 60;
        TM1637_point(POINT_OFF);
    }
    else {
        toggle_mode = TOGGLE_NONE;
    }
    display_mode = new_mode;
    
    if (toggle_mode == TOGGLE_NONE)
        return;

    int8_t digits[4];
    switch (toggle_mode) {
        case TOGGLE_TIME:
        digits[0]=left_part / 10;
        digits[1]=left_part % 10;
        digits[2]=right_part / 10;
        digits[3]=right_part % 10;
        break;
        case TOGGLE_TEMPERATURE:
        digits[0]=temperature / 10;
        digits[1]=temperature % 10;
        digits[2]=18; //blank
        digits[3]=15; //'F'
        break;
        case TOGGLE_HUMIDITY:
        digits[0]=humidity / 10;
        digits[1]=humidity % 10;
        digits[2]=18; //blank
        digits[3]=17; //'H'
        break;
        case TOGGLE_OUTSIDE_TEMPERATURE:
        digits[0]=outside_temperature / 10;
        digits[1]=outside_temperature % 10;
        digits[2]=18; //blank
        digits[3]=19; //'O'
        break;
    }
    TM1637_display_str(digits);
}

//gcc -o daily daily.c TM1637.c yahoo.c -lpthread -lwiringPi -lwiringPiDev -lm -lssl -lcrypto
int main(int argc, char *argv[]) 
{
    launch_mode = LAUNCH_MODE_CMD_LINE;
	for (int i = 1; i < argc; ++i) {
		if (strcmp (argv[i], "-boot") == 0)
			launch_mode = LAUNCH_MODE_ON_BOOT;
	}
    
	hook_signal();
    wiringPiSetup();
    pinMode(PIN_BTN1, INPUT);
    pinMode(PIN_BTN2, INPUT);
    pinMode(PIN_BTN3, INPUT);
    pinMode(PIN_BTN4, INPUT);
    pinMode(PIN_LDR,  INPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_Y, OUTPUT);
    
    pinMode(PIN_DIO, INPUT);
    pinMode(PIN_CLK, INPUT);

    turn_off_buzzer();
    TM1637_init(PIN_CLK,PIN_DIO);
    TM1637_set(BRIGHTEST,0x40,0xc0);//BRIGHT_TYPICAL = 2,BRIGHT_DARKEST = 0,BRIGHTEST = 7;
    TM1637_point(POINT_OFF);
    TM1637_clearDisplay();

    if (launch_mode == LAUNCH_MODE_ON_BOOT) {
        digitalWrite(PIN_LED_R, HIGH);
        digitalWrite(PIN_LED_G, HIGH);
        digitalWrite(PIN_LED_Y, HIGH);

        delay_ms(30 * 1000);//30 seconds
    }   
    turn_off_all_leds();

    int timer_state = TIMER_IDLE, timer_sub_state = 0;
    header = tail = 0;
    display_mode = DISPLAY_OFF;
    last_dht11_timestamp = last_outside_timestamp = -1;
    
    pthread_t thread_daemon;
    pthread_create(&thread_daemon, NULL, timer_daemon, NULL);

    unsigned long long start_at, timeout_at = 0, now = 0, elapsed = 0;
    unsigned long long system_idle = 0;
    int counter = 0, reminder = 0, alt = 0;
    int last_maintenance_day = 0;

    while (!done) {
        now = get_current_time();
        int key_event = consume_one_key();
        if (key_event == 0 && timer_state == TIMER_IDLE && timer_sub_state == 0) {
            if (system_idle >= 60 * 60) { //idle for at least one hour
                int hour, mday;
                get_hour_day(&hour, &mday);
                if ((hour >= LOG_MAINTENANCE_HOUR) && (hour < (LOG_MAINTENANCE_HOUR+1)) && (mday != last_maintenance_day)) {
                    last_maintenance_day = mday;
                    daily_maintenance();
                }
            }
            switch (display_mode) {
                case DISPLAY_OFF:
                if (system_idle >= SYS_TIME_ON_IF_IDLE) {
                    int hour, mday;
                    get_hour_day(&hour, &mday);
                    //for example, on: 7am, off: 0am
                    if (hour >= SYS_TIME_ON_HOUR || hour < SYS_TIME_OFF_HOUR)
                        display(DISPLAY_SYS_TIME, 0);
                }
                break;
                case DISPLAY_COUNT: //for example, in tv time
                break;
                case DISPLAY_SYS_TIME: {
                    int hour, mday;
                    get_hour_day(&hour, &mday);
                    if (hour >= SYS_TIME_ON_HOUR || hour < SYS_TIME_OFF_HOUR)
                        display(DISPLAY_SYS_TIME, 0);
                    else
                        display(DISPLAY_OFF, 0);
                }
                break;
            }
            ++system_idle;
        }
        else {
            system_idle = 0;
        }
        //hard reset
        if (key_event == KEY_BTN1_LONG_PRESSED || key_event == KEY_BTN2_LONG_PRESSED ||
                    key_event == KEY_BTN3_LONG_PRESSED || key_event == KEY_BTN4_LONG_PRESSED) {
            log_event(TIMER_HARD_RESET, 0);
            switch (timer_state) {
                case TIMER_TV:
                log_event(TIMER_TV_TIMEOUT_, now - start_at);
                break;
                case TIMER_TV_PAUSED:
                log_event(TIMER_TV_TIMEOUT_, elapsed);
                break;
                case TIMER_LEARNING:
                log_event(TIMER_LEARNING_TIMEOUT_, now - start_at);
                break;
                case TIMER_LEARNING_PAUSED:
                log_event(TIMER_LEARNING_TIMEOUT_, elapsed);
                break;
                case TIMER_BREAK:
                log_event(TIMER_BREAK_TIMEOUT_, now - start_at);
                break;
                case TIMER_STOPWATCH:
                log_event(TIMER_STOPWATCH_STOP, now - start_at);
                break;
            }
            turn_off_all_leds();
            turn_off_buzzer();
            display(DISPLAY_OFF, 0);
            timer_state = TIMER_IDLE;
            timer_sub_state = 0;
            key_event = 0;
        }
        switch (timer_state) {
            case TIMER_IDLE:
                if (timer_sub_state) {
                    //user turns the beep off after the learning/watching tv timeout
                    if ((key_event == KEY_BTN1_PRESSED && timer_sub_state == TIMER_TV_TIMEOUT_) ||
                            (key_event == KEY_BTN2_PRESSED && timer_sub_state == TIMER_LEARNING_TIMEOUT_) ||
                                (key_event == KEY_BTN3_PRESSED && timer_sub_state == TIMER_BREAK_TIMEOUT_)) {
                        timer_sub_state = 0;
                        turn_off_all_leds();
                        turn_off_buzzer();
                        display(DISPLAY_OFF, 0);

                        key_event = 0;
                    }
                    //auto turn the beep off after 20 seconds(DURATION_BEEP)
                    if (timer_sub_state && (now >= timeout_at)) {
                        timer_sub_state = 0;
                        turn_off_all_leds();
                        turn_off_buzzer();
                        display(DISPLAY_OFF, 0);
                    }
                }
                if (key_event == KEY_BTN1_PRESSED) {//tv time
                    timer_state = TIMER_TV;
                    timer_sub_state = 0;
                    start_at = now;
                    timeout_at = now + DURATION_TV * 60 * 1000;
                    counter = 0;
                    turn_off_buzzer();
                    turn_off_all_leds();
                    display(DISPLAY_COUNT, DURATION_TV * 60);
                    digitalWrite(PIN_LED_R, HIGH);
                    log_event(TIMER_TV, 0);
                }
                else if (key_event == KEY_BTN2_PRESSED) {//learning time
                    timer_state = TIMER_LEARNING;
                    timer_sub_state = 0;
                    start_at = now;
                    timeout_at = now + DURATION_LEARNING * 60 * 1000;
                    counter = 0;
                    turn_off_buzzer();
                    turn_off_all_leds();
                    display(DISPLAY_COUNT, DURATION_LEARNING * 60);
                    digitalWrite(PIN_LED_G, HIGH);
                    log_event(TIMER_LEARNING, 0);
                }
                else if (key_event == KEY_BTN3_PRESSED) {//break time
                    timer_state = TIMER_BREAK;
                    timer_sub_state = 0;
                    start_at = now;
                    timeout_at = now + DURATION_BREAK * 60 * 1000;
                    counter= 0;
                    turn_off_buzzer();
                    turn_off_all_leds();
                    display(DISPLAY_COUNT, DURATION_BREAK * 60);
                    digitalWrite(PIN_LED_Y, HIGH);
                    log_event(TIMER_BREAK, 0);
                }
                else if (key_event == KEY_BTN4_PRESSED) {//stopwatch
                    timer_state = TIMER_STOPWATCH;
                    timer_sub_state = 0;
                    start_at = now;
                    counter= reminder = alt = 0;
                    turn_off_buzzer();
                    turn_off_all_leds();
                    display(DISPLAY_COUNT, 0);
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
                    turn_on_buzzer(0); //buzzer on
                    display(DISPLAY_COUNT, 0);
                    counter = 0;
                    log_event(TIMER_TV_TIMEOUT_, now - start_at);
                }
                else {
                    elapsed = now - start_at;
                    if (key_event == KEY_BTN1_PRESSED) {
                        timer_state = TIMER_TV_PAUSED;
                        digitalWrite(PIN_LED_R, HIGH);
                    }
                    else {
                        digitalWrite(PIN_LED_R, (counter & 1) ? LOW: HIGH);
                    }
                    display(DISPLAY_COUNT, DURATION_TV * 60 - elapsed/1000); //time left
                }
                break;
            case TIMER_TV_PAUSED:
                if (key_event == KEY_BTN1_PRESSED) {
                    start_at = now - elapsed;
                    timeout_at = start_at + DURATION_TV * 60 * 1000;
                    timer_state = TIMER_TV;
                    digitalWrite(PIN_LED_R, LOW);
                }
                break;
            case TIMER_LEARNING:
                if (now >= timeout_at) {
                    timer_state = TIMER_IDLE;
                    timer_sub_state = TIMER_LEARNING_TIMEOUT_;
                    timeout_at = get_current_time() + DURATION_BEEP * 1000;
                    digitalWrite(PIN_LED_G, HIGH); //G always on
                    turn_on_buzzer(0); //buzzer on
                    display(DISPLAY_COUNT, 0);
                    counter = 0;
                    log_event(TIMER_LEARNING_TIMEOUT_, now - start_at);
                }
                else {
                    elapsed = now - start_at;
                    if (key_event == KEY_BTN2_PRESSED) {
                        timer_state = TIMER_LEARNING_PAUSED;
                        digitalWrite(PIN_LED_G, HIGH);
                    }
                    else {
                        digitalWrite(PIN_LED_G, (counter & 1) ? LOW: HIGH);
                    }
                    display(DISPLAY_COUNT, DURATION_LEARNING * 60 - elapsed/1000);
                }
                break;
            case TIMER_LEARNING_PAUSED:
                if (key_event == KEY_BTN2_PRESSED) {
                    start_at = now - elapsed;
                    timeout_at = start_at + DURATION_LEARNING * 60 * 1000;
                    timer_state = TIMER_LEARNING;
                    digitalWrite(PIN_LED_G, LOW);
                }
                break;
            case TIMER_BREAK:
                if (now >= timeout_at) {
                    timer_state = TIMER_IDLE;
                    timer_sub_state = TIMER_BREAK_TIMEOUT_;
                    timeout_at = get_current_time() + DURATION_BEEP * 1000;
                    digitalWrite(PIN_LED_Y, HIGH); //Y always on
                    turn_on_buzzer(0); //buzzer on
                    display(DISPLAY_OFF, 0);
                    counter = 0;
                    log_event(TIMER_BREAK_TIMEOUT_, now - start_at);
                }
                else {
                    display(DISPLAY_COUNT, DURATION_BREAK * 60 - (now - start_at)/1000);
                    digitalWrite(PIN_LED_Y, (counter & 1) ? LOW: HIGH);
                }
                break;
            case TIMER_STOPWATCH:
                if (key_event == KEY_BTN4_PRESSED) {
                    timer_state = TIMER_IDLE;
                    turn_off_buzzer();
                    turn_off_all_leds();
                    display(DISPLAY_OFF, 0);
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
                    display(DISPLAY_COUNT, (now - start_at)/1000);//elapsed time
                    //Reminder every N seconds
                    int to_reminder = (now - start_at) / 1000 / REMINDER_INTERVAL;
                    if (to_reminder > 0 && reminder < to_reminder) {
                        reminder = to_reminder;
                        turn_on_buzzer(reminder);
                    }
                }
                break;
        }
        counter = (counter + 1) & 1;
        int consumed = (int)(get_current_time() - now);
        delay_ms(1000 - consumed);
    }
    turn_off_all_leds();
    turn_off_buzzer();
    TM1637_point(POINT_OFF);
    TM1637_clearDisplay();
    
    pthread_join(thread_daemon, NULL);
	return 0;
}
