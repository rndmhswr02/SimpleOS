#include "pic.h"
#include "io.h"

#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1

#define PIC_EOI        0x20
#define ICW1_ICW4      0x01   /* ICW4 will be sent */
#define ICW1_INIT      0x10   /* begin initialization */
#define ICW4_8086      0x01   /* 8086 mode */
#define OCW3_READ_ISR  0x0B   /* next read of the command port returns the ISR */

void pic_remap(uint8_t offset1, uint8_t offset2) {
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4); io_wait();

    outb(PIC1_DATA, offset1);              io_wait();   /* vector offset, master */
    outb(PIC2_DATA, offset2);              io_wait();   /* vector offset, slave */

    outb(PIC1_DATA, 4);                    io_wait();   /* master: slave is on IRQ2 (bit 2) */
    outb(PIC2_DATA, 2);                    io_wait();   /* slave: its cascade identity is 2 */

    outb(PIC1_DATA, ICW4_8086);            io_wait();
    outb(PIC2_DATA, ICW4_8086);            io_wait();

    outb(PIC1_DATA, 0xFF);                              /* mask all IRQs on both PICs */
    outb(PIC2_DATA, 0xFF);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);   /* slave first... */
    outb(PIC1_CMD, PIC_EOI);       /* ...then master (always) */
}

void pic_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t line  = (irq < 8) ? irq : (uint8_t)(irq - 8);
    outb(port, inb(port) | (1 << line));
}

void pic_unmask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t line  = (irq < 8) ? irq : (uint8_t)(irq - 8);
    outb(port, inb(port) & ~(1 << line));

    if (irq >= 8)
        pic_unmask(2);             /* slave IRQs only reach the CPU via the cascade line */
}

/* Read the In-Service Register of both PICs (bit N set = IRQ N being serviced). */
static uint16_t pic_read_isr(void) {
    outb(PIC1_CMD, OCW3_READ_ISR);
    outb(PIC2_CMD, OCW3_READ_ISR);
    return (uint16_t)((inb(PIC2_CMD) << 8) | inb(PIC1_CMD));
}

int pic_is_spurious(uint8_t irq) {
    if (irq == 7)
        return !(pic_read_isr() & (1 << 7));

    if (irq == 15) {
        if (!(pic_read_isr() & (1 << 15))) {
            outb(PIC1_CMD, PIC_EOI);   /* the master still saw the cascade IRQ */
            return 1;
        }
    }
    return 0;
}