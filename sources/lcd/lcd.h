/*
 * Generic character LCD display with printf ability (with ESC-sequences)
 */
#include <stream/stream.h>

typedef struct {
	stream_interface_t *interface;	/* for printf */
	int esc_mode;
	unsigned short esc_buf;
} lcd_t;

void lcd_init (lcd_t *lcd);
void lcd_clear (lcd_t *lcd);
void lcd_set_brightness (lcd_t *lcd, int percent);

