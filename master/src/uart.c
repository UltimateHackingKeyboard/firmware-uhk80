#include <zephyr/drivers/uart.h>

const struct device *uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart1));

char bufferIn[20];
char *ch = bufferIn;
void serial_cb(const struct device *dev, void *user_data)
{
    if (!uart_irq_update(uart_dev)) {
        return;
    }

    while (uart_irq_rx_ready(uart_dev)) {
        // put characters into bufferI until newline, at which point printk() the buffer and clear it
        uart_fifo_read(uart_dev, (uint8_t*)ch, 1);
        if (*ch == '\n') {
            printk("got %s", bufferIn);
            ch = bufferIn;
        } else {
            ch++;
        }
    }
}

void InitUart(void) {
    if (!device_is_ready(uart_dev)) {
        printk("UART device not found!");
        return 1;
    }

    uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);
    uart_irq_rx_enable(uart_dev);
}
