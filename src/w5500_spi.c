#include <linux/delay.h>
#include "w5500.h"

/*
Hardware Reset Sequence

RESET pin is active-low

DT overlay declares it as GPIO_ACTIVE_LOW

logical 1 -> drive physical low (asserted)
logical 0 -> drive physical high (deasserted)

*/
int w5500_hw_reset(struct w5500_priv *priv){
    
    if(!priv->reset_gpio){
        return -ENODEV;
    }

    /* Assert reset */
    dev_info(&priv->spi->dev,"Asserting RESETn (driving low) \n");
    gpiod_set_value_cansleep(priv->reset_gpio,1);
    msleep(10);

    /* Deassert reset */
    dev_info(&priv->spi->dev,"Dasserting RESETn (driving high) \n");
    gpiod_set_value_cansleep(priv->reset_gpio,0);
    msleep(100);

    dev_info(&priv->spi->dev,"W5500 hardware reset completed \n");
    return 0;

}