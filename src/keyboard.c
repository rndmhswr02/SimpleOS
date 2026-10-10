#include "keyboard.h"
#include "io.h"
#include <stdint.h>

static const char scancode_map[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t', 'q','w','e','r','t','y','u','i','o','p','[',']', '\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' '
};

char keyboard_getchar(void) {
    for (;;) {
        if (inb(0x64) & 0x01) {                 /* output buffer full? */
            uint8_t sc = inb(0x60);
            if (sc & 0x80) continue;            /* key release: ignore */
            if (scancode_map[sc])
                return scancode_map[sc];
        }
    }
}