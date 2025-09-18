#ifndef _W5500_H_
#define _W5500_H_

#include <linux/spi/spi.h>
#include <linux/gpio/consumer.h>

/* Private Data Structure
- Holds state for one W5500 instance.
- Each spi device that matches this driver will get its own copy
*/

struct w5500_priv {
    struct spi_device *spi; /* Spi Device Pointer */
    struct gpio_desc *reset_gpio; /* Reset Pin */
    int irq;   /* Interrupt line */
};

int w5500_hw_reset(struct w5500_priv *priv);



#endif