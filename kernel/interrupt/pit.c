#include "interrupt/pit.h"

#include "interrupt/irq.h"
#include "interrupt/pic.h"
#include "interrupt/ports.h"
#include "log/printk.h"

static volatile uint64_t pit_ticks;
static uint32_t pit_ticks_per_second;

static void pit_set_channel0(uint16_t divisor)
{
    outb(PIT_PORT_CMD, 0x36u); /* canal 0, lobyte/hibyte, modo 3 */
    outb(PIT_PORT_DATA, (uint8_t)(divisor & 0xFFu));
    outb(PIT_PORT_DATA, (uint8_t)((divisor >> 8) & 0xFFu));
}

void pit_init(void)
{
    pit_ticks = 0;
    pit_ticks_per_second = PIT_TICK_HZ;
    uint16_t divisor = (uint16_t)(PIT_BASE_HZ / PIT_TICK_HZ);
    pit_set_channel0(divisor);
    pic_unmask_irq(IRQ_TIMER);
    printk("[pit] init %u Hz (divisor %u)\n", PIT_TICK_HZ, (unsigned)divisor);
}

void pit_on_irq(void)
{
    pit_ticks++;
    if (pit_ticks_per_second == 0u) {
        return;
    }
    if ((pit_ticks % pit_ticks_per_second) == 0u) {
        uint64_t seconds = pit_ticks / pit_ticks_per_second;
        printk("[tick] %llu s\n", (unsigned long long)seconds);
    }
}

uint64_t pit_ticks_get(void)
{
    return pit_ticks;
}

void pit_sleep_ms(uint32_t ms)
{
    if (pit_ticks_per_second == 0u) {
        return;
    }
    uint64_t target = pit_ticks + ((uint64_t)ms * pit_ticks_per_second) / 1000u;
    while (pit_ticks < target) {
        __asm__ volatile("hlt");
    }
}