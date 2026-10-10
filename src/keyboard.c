#include "keyboard.h"
#include "io.h"
#include "irq.h"
#include <stdint.h>

#define KB_DATA      0x60
#define KB_STATUS    0x64
#define KB_BUF_SIZE  128

#define SC_LSHIFT    0x2A
#define SC_RSHIFT    0x36
#define SC_CAPSLOCK  0x3A

/* Scancode set 1 -> ASCII, no modifiers */
static const char map_normal[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t', 'q','w','e','r','t','y','u','i','o','p','[',']', '\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' '
};

/* Same keys with Shift held */
static const char map_shift[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t', 'Q','W','E','R','T','Y','U','I','O','P','{','}', '\n',
    0, 'A','S','D','F','G','H','J','K','L',':','"','~',
    0, '|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0, ' '
};

/* Ring buffer: the IRQ handler writes at `head`, keyboard_getchar() reads at `tail`. */
static volatile char     buffer[KB_BUF_SIZE];
static volatile uint32_t head = 0;
static volatile uint32_t tail = 0;

/* Modifier state: only touched from the IRQ handler. */
static int shift_down = 0;
static int caps_on    = 0;
static int extended   = 0;     /* previous byte was the 0xE0 prefix */

static void keyboard_irq(struct registers *r) {
    (void)r;

    uint8_t sc = inb(KB_DATA);

    /* Extended keys (arrows, Delete, Home...) send 0xE0 followed by a code. Ignore for now. */
    if (sc == 0xE0) { extended = 1; return; }
    if (extended)   { extended = 0; return; }

    int released = sc & 0x80;          /* top bit set = key release */
    sc &= 0x7F;

    if (sc == SC_LSHIFT || sc == SC_RSHIFT) { shift_down = !released; return; }
    if (sc == SC_CAPSLOCK) { if (!released) caps_on = !caps_on; return; }
    if (released) return;

    char base = map_normal[sc];
    if (base == 0) return;             /* unmapped or modifier key */

    char c;
    if (base >= 'a' && base <= 'z') {
        /* Letters: Shift and Caps Lock cancel each other out */
        c = (shift_down != caps_on) ? (char)(base - 'a' + 'A') : base;
    } else {
        /* Everything else: only Shift matters */
        c = shift_down ? map_shift[sc] : base;
    }
    if (c == 0) return;

    uint32_t next = (head + 1) % KB_BUF_SIZE;
    if (next == tail) return;          /* buffer full: drop the keystroke */
    buffer[head] = c;
    head = next;
}

void keyboard_init(void) {
    /* Discard anything the controller already has queued */
    while (inb(KB_STATUS) & 0x01)
        inb(KB_DATA);

    irq_register_handler(1, keyboard_irq);    /* also unmasks IRQ 1 */
}

char keyboard_getchar(void) {
    /* Interrupts are enabled, so hlt wakes on the next interrupt (keyboard, or at
     * worst the next timer tick). No CPU is burned while idle. */
    while (head == tail)
        __asm__ volatile ("hlt");

    char c = buffer[tail];
    tail = (tail + 1) % KB_BUF_SIZE;
    return c;
}