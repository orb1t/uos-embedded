#include <uos-conf.h>
#include <runtime/lib.h>
#include <stream/stream.h>

static stream_interface_t debug_interface = {
	.putc = (void (*) (stream_t*, short)) debug_putchar,
	.getc = (unsigned short (*) (stream_t*)) debug_getchar,
	.peekc = (int (*) (stream_t*)) debug_peekchar,
};

stream_t debug = { &debug_interface };

#ifndef NO_DEBUG_PRINT

int
debug_printf (const char *fmt, ...)
{
	va_list	args;
	int err;

	va_start (args, fmt);
	err = vprintf (&debug, fmt, args);
	va_end (args);
	return err;
}

int
debug_vprintf (const char *fmt, va_list args)
{
	int err;

	err = vprintf (&debug, fmt, args);
	return err;
}

void debug_putc (char c)
{
	debug_putchar (0, c);
}

#endif //NO_DEBUG_PRINT
