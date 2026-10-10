#ifndef CONSOLE_H
#define CONSOLE_H

/* Print one character to both the screen and the serial port. */
void kputc(char c);

/* Print a string to both the screen and the serial port. */
void kprint(const char *s);

#endif