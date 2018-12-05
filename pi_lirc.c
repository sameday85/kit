#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#define CHAR_BIT    8

//LIRC
#define PULSE_BIT       0x01000000
#define PULSE_MASK      0x00FFFFFF

typedef unsigned int __u32;
/* code length in bits, currently only for LIRC_MODE_LIRCCODE */
#define LIRC_GET_LENGTH                _IOR('i', 0x0000000f, __u32)
#define LIRC_GET_REC_MODE              _IOR('i', 0x00000002, __u32)
#define LIRC_MODE_MODE2                0x00000004
/* used heavily by lirc userspace */
#define lirc_t int

//
// the min and max dirations of the pulse and space parts of the header
#define MIN_HEADER_PULSE_WIDTH  4400
#define MAX_HEADER_PULSE_WIDTH  4800
#define MIN_HEADER_SPACE_WIDTH  MIN_HEADER_PULSE_WIDTH
#define MAX_HEADER_SPACE_WIDTH  MAX_HEADER_PULSE_WIDTH

//the min and max durations of the pulse part of a one, zero, or trailer
#define MIN_PULSE_WIDTH         400
#define MAX_PULSE_WIDTH         750

//the min and max durations of the space part of a zero
#define MIN_ZERO_SPACE_WIDTH    350
#define MAX_ZERO_SPACE_WIDTH    750

//the min and max durations of the space part of a one
#define MIN_ONE_SPACE_WIDTH     1500
#define MAX_ONE_SPACE_WIDTH     1820


#ifndef bool

#define bool        int
#define true        1
#define false       0

#endif

size_t count;
int fd;

int pos;
char byte_buffer[255];

unsigned char bit_count;
unsigned char bit_buffer;

void bit_decoder_reset_() {
    bit_count = bit_buffer = 0;
}

void bit_decoder_reset() {
    pos = 0;
    bit_decoder_reset_();
}

int bit_decoder_bit(int val) {
    bit_buffer = (bit_buffer << 1) | (val & 0x1);
    bit_count += 1;
    if (bit_count >= 8) {
        sprintf(&byte_buffer[pos], "%02X", bit_buffer);
        pos += 2;
        bit_decoder_reset_();
    }
    return pos;
}

void bit_decoder_gap() {
    byte_buffer[pos] = '\0';
    bit_decoder_reset();
}

bool pulse_decoder(int pulse_width, int space_width) {
    if (pulse_width <= 0)
        return false;
    
    if (pulse_width > MIN_PULSE_WIDTH &&
            pulse_width < MAX_PULSE_WIDTH &&
                (space_width ==0 || space_width > 10000)) {
        bit_decoder_gap();
        
        return strlen(byte_buffer) > 0;
    }

    if (pulse_width > MIN_HEADER_PULSE_WIDTH &&
            pulse_width < MAX_HEADER_PULSE_WIDTH &&
                space_width > MIN_HEADER_SPACE_WIDTH &&
                    space_width < MAX_HEADER_SPACE_WIDTH) {
            //self.logger.debug("header")
        return false;
    }

    if (pulse_width > MIN_PULSE_WIDTH &&
            pulse_width < MAX_PULSE_WIDTH &&
                space_width > MIN_ONE_SPACE_WIDTH &&
                    space_width < MAX_ONE_SPACE_WIDTH) {

        if (bit_decoder_bit(1) >= 8) {
			bit_decoder_gap();
			return true;
		}
        return false;
    }

    if (pulse_width > MIN_PULSE_WIDTH &&
            pulse_width < MAX_PULSE_WIDTH &&
                space_width > MIN_ZERO_SPACE_WIDTH &&
                    space_width < MAX_ZERO_SPACE_WIDTH) {
    //self.logger.debug("zero")
        if (bit_decoder_bit(0) >= 8) {
			bit_decoder_gap();
			return true;
		}
        return false;
    }
    return false;
}

bool pi_lirc_init () {
    fd = open("/dev/lirc0", O_RDONLY);
    if (fd == -1)
        return false;
    count=4;
    return true;
}

void pi_lirc_close() {
    if (fd > 0)
        close (fd);
    fd = 0;
}

bool wait_for_data() {
    fd_set rfds;
    struct timeval tv;
    int retval;
    
    /* Watch stdin (fd 0) to see when it has input. */
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    tv.tv_sec = 2; //seconds
    tv.tv_usec= 0;
    retval = select(fd, &rfds, NULL, NULL, &tv);
    
    return retval > 0;
}

char * pi_lirc_next_key() {
    int pulse_width, space_width;
    lirc_t data;
    int result;

    pulse_width = space_width = 0;
    byte_buffer[0]='\0';
    bit_decoder_reset();
    result = read(fd, (void *)&data, count);
    while (result == count) {
		//printf("data: 0x%08X\n", data);
        if (data & PULSE_BIT) {
            pulse_width = data & PULSE_MASK;
            space_width = 0;
        }
        else {
            space_width = data & PULSE_MASK;
            if (pulse_decoder(pulse_width, space_width))
                break;
        }
        result = read(fd, (void *)&data, count);
    }
    return byte_buffer;
    //printf("%s %u\n", (data & PULSE_BIT) ? "pulse" : "space", (__u32) (data & PULSE_MASK));
}

void after_mode2() {
    int pulse_width = 0, space_width = 0;
    
    bit_decoder_reset();
    
    char*   line_buffer;
    size_t  len;
    while (getline(&line_buffer, &len, stdin) >= 0) {
        if (strstr(line_buffer, "pulse")) {
            int data = atoi(&line_buffer[6]);
            pulse_width = data & PULSE_MASK;
            space_width = 0;
        }
        else if (strstr(line_buffer, "space")) {
            int data = atoi(&line_buffer[6]);
            space_width = data & PULSE_MASK;
            if (pulse_decoder(pulse_width, space_width))
                break;
        }
    }
    free (line_buffer);
    printf("Key: %s, expected 00FF30CF\n", byte_buffer);
}
/*
int main(int argc, char *argv[]) {
    if (!pi_lirc_init())
        return 1;
    char *key = pi_lirc_next_key();
    while (key) {
		if (strlen(key) <= 0)
			break;
        printf ("Key: %s\n", key);
        key = pi_lirc_next_key();
    }
    pi_lirc_close();
    return 0;
}
*/
