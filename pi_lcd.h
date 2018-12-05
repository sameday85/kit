#include <lcd.h>
#include "common.h"

extern bool init_lcd();
extern void deinit_lcd();
extern void turn_on_lcd();
extern void turn_off_lcd();
extern void show_lcd (int line, const char *msg);
extern void show_lcd_center (int line, const char *msg);
