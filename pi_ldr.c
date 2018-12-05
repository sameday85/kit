#include <stdio.h>
#include <sys/time.h>

#include <wiringPi.h>
#include "pi_ldr.h"


int timedifference_msec(struct timeval t0, struct timeval t1)
{
    return (t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_usec - t0.tv_usec) / 1000;
}

int get_charging_time(int pin) {
	int val, wait=MAX_WAIT;
	struct timeval t0, t1;
   	
	pinMode (pin, OUTPUT);
	digitalWrite(pin, LOW);	
	delay(1000);
	
	gettimeofday(&t0, 0);
	pinMode (pin, INPUT);
	val=digitalRead(pin);
	
	while ((val == LOW) && (wait > 0)) {
		delay(10);
		wait -= 10;
		val=digitalRead(pin);
	}
	pinMode (pin, OUTPUT);
	digitalWrite(pin, LOW);	
	
	gettimeofday(&t1, 0);
	
	return timedifference_msec(t0, t1);
}
