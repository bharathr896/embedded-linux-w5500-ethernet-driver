#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <linux/spi/spi.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include "w5500.h"


static int	w5500_probe(struct spi_device *spi){

    struct device *dev = &spi->dev;
    struct w5500_priv *priv;
    int ret;

    priv = devm_kzalloc(dev,sizeof(struct w5500_priv),GFP_KERNEL);
    if(!priv){
        return -ENOMEM;
    }
    priv->spi = spi;
    spi_set_drvdata(spi,priv);

    /* Get reset gpio */
    priv->reset_gpio = devm_gpiod_get_optional(dev,"reset",GPIOD_OUT_LOW);
    if(IS_ERR(priv->reset_gpio)){
        dev_info(dev,"Failed to get reset GPIO \n");
    }
    if(priv->reset_gpio){
        dev_info(dev,"Got reset pin\n");
    }

    /* Get IRQ directly from DT */
    priv->irq = of_irq_get(dev->of_node, 0);
    if(priv->irq < 0){
        dev_warn(dev,"No IRQ defined in DT \n");
    }
    else{
        dev_info(dev,"Got INTn IRQ:%d \n",priv->irq);
    }

    /* Hardware reset if pin availabe */
    if(priv->reset_gpio){
        ret = w5500_hw_reset(priv);
        if(ret){
            return ret;
        }
    }

    dev_info(dev,"W5500 probe finished successfully \n");

    return 0;
}

static void w5500_remove(struct spi_device *spi){

    dev_info(&spi->dev,"Removing W5500 driver \n");
}


static const struct of_device_id w5500_dt_ids[] = {
    {.compatible = "custom,w5500-ethernet",},
    {}
};

MODULE_DEVICE_TABLE(of,w5500_dt_ids);

static struct spi_driver w5500_driver = {
    .driver = {
        .name = "w5500-ethernet",
        .of_match_table = w5500_dt_ids,
    },
    .probe = w5500_probe,
    .remove = w5500_remove,
};

module_spi_driver(w5500_driver);

MODULE_AUTHOR("Bharath Reddappa");
MODULE_DESCRIPTION("W5500 Ethernet SPI Driver");
MODULE_LICENSE("GPL");
