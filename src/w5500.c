#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <linux/spi/spi.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/device.h>
#include "w5500.h"



static int w5500_open(struct net_device *ndev);
static int w5500_stop(struct net_device *ndev);
static netdev_tx_t w5500_start_xmit(struct sk_buff *skb,
                                        struct net_device *ndev);

/* Netdevice ops table */
const struct net_device_ops w5500_netdev_ops = {
    .ndo_open = w5500_open,
    .ndo_stop = w5500_stop,
    .ndo_start_xmit = w5500_start_xmit,
};

/*
 * IRQ handler for W5500 INTn
 *
 * Flow:
 *   1. Called when INTn pin is asserted low
 */
static irqreturn_t w5500_irq(int irq, void *dev_id)
{
    struct w5500_priv *priv = dev_id;
    dev_info(&priv->spi->dev, "W5500 IRQ fired (INTn=%d)\n", irq);

    disable_irq_nosync(irq);
    return IRQ_HANDLED;
    
}



/*
 * Flow:
 *   1. Start TX queue
 */
static int w5500_open(struct net_device *ndev){
    struct w5500_priv *priv = netdev_priv(ndev);
    dev_info(&priv->spi->dev, "%s: ndo_open\n", ndev->name);

    netif_start_queue(ndev); // allow kernel to call start_xmit
    return 0;
}


/*
 *
 * Flow:
 *   1. Stop TX queue
 */
static int w5500_stop(struct net_device *ndev)
{
    struct w5500_priv *priv = netdev_priv(ndev);
    dev_info(&priv->spi->dev, "%s: ndo_stop\n", ndev->name);

    netif_stop_queue(ndev); // stop outgoing packets
    return 0;
}

/*
 * w5500_tx_frame - push skb into hardware
 *
 * Flow :
 *   1. Log skb length
 *   2. Return success
 */
int w5500_tx_frame(struct w5500_priv *priv, struct sk_buff *skb)
{
    dev_info(&priv->spi->dev, "w5500_tx_frame: len=%u\n", skb->len);
    return 0;
}

/*
 * ndo_start_xmit - send packet from kernel
 *
 * Flow:
 *   1. Validate length
 *   2. Call w5500_tx_frame
 *   3. Update TX stats
 *   4. Free skb
 *
 * Returns:
 *   NETDEV_TX_OK always (Phase 3)
 */
static netdev_tx_t w5500_start_xmit(struct sk_buff *skb,
                                        struct net_device *ndev)
{
    struct w5500_priv *priv = netdev_priv(ndev);
    int ret;

    dev_info(&priv->spi->dev, "%s: start_xmit len=%u\n",
            ndev->name, skb->len);

    // Safety: drop oversized frames
    if (skb->len > 1500) {
        dev_warn(&priv->spi->dev, "Packet too large: %u\n", skb->len);
        ndev->stats.tx_dropped++;
        dev_kfree_skb_any(skb);
        return NETDEV_TX_OK;
    }

    // Send via helper 
    ret = w5500_tx_frame(priv, skb);
    if (ret) {
        dev_err(&priv->spi->dev, "tx_frame failed: %d\n", ret);
        netif_stop_queue(ndev);
        ndev->stats.tx_errors++;
        dev_kfree_skb_any(skb);
        netif_wake_queue(ndev);
        return NETDEV_TX_OK;
    }

    // Update stats
    ndev->stats.tx_packets++;
    ndev->stats.tx_bytes += skb->len;
    dev_kfree_skb_any(skb);

    return NETDEV_TX_OK;
}


/*
* Probe - called when SPI device matches
*
* Flow:
*   1. Allocate net_device + priv
*   2. Assign netdev_ops
*   3. Set MAC
*   4. Request reset GPIO + IRQ
*   5. Reset chip, check VERSIONR
*   6. Register net_device
*/
static int	w5500_probe(struct spi_device *spi){

    struct device *dev = &spi->dev;
    struct net_device *ndev;
    struct w5500_priv *priv;
    int ret;
    u8 version;

    dev_info(dev,"Probing W5500.. \n");

    // 1.Alloc net_device 

    ndev = alloc_etherdev(sizeof(*priv));
    if(!ndev){
        return -ENOMEM;
    }

    SET_NETDEV_DEV(ndev,&spi->dev);
    priv = netdev_priv(ndev);
    memset(priv,0,sizeof(*priv));
    priv->spi = spi;
    priv->netdev = ndev;
    spin_lock_init(&priv->lock);
    priv->irq = -1;
    spi_set_drvdata(spi,priv);

    // 2. Set net_dev ops
    priv->netdev->netdev_ops = &w5500_netdev_ops;

    // 3. MAC addr random
    eth_hw_addr_random(ndev);
    dev_info(dev, "Random MAC: %pM\n", ndev->dev_addr);



    // 4. reset GPIO
    priv->reset_gpio = devm_gpiod_get_optional(dev,"reset",GPIOD_OUT_LOW);
    if(IS_ERR(priv->reset_gpio)){
        dev_info(dev,"Failed to get reset GPIO \n");
        ret = PTR_ERR(priv->reset_gpio);
        dev_err(dev, "reset gpio failed: %d\n", ret);
        free_netdev(ndev);
        return ret;
    }
    
    // 4.b IRQ
    priv->irq = of_irq_get(dev->of_node, 0);
    if(priv->irq >=0 ){
        dev_info(dev, "INTn IRQ=%d\n", priv->irq);
        ret = devm_request_irq(dev,priv->irq,w5500_irq,IRQF_TRIGGER_LOW,dev_name(dev),priv);

        if(ret){
            dev_err(dev," request irq failed:%d  \n",ret);
            free_netdev(ndev);
            return ret;
        }
    }

    //5. Reset + Read Version
    if(priv->reset_gpio){
        ret = w5500_hw_reset(priv);
        if(ret){
            free_netdev(ndev);
            return ret;
        }
    }

    /* Read version */
    ret = w5500_spi_read8(priv,W5500_VERSIONR,&version);
    if(ret){
        dev_err(dev,"Failed to read VERSIONR\n");
        free_netdev(ndev);
        return ret;
    }

    if(version==0x04){
        dev_info(dev,"VERSION = 0x%02x (OK) \n",version);
    }
    else{
        dev_warn(dev,"VERSION = 0x%02x (unexpected, expected 0x04)\n",version);
    }

    //6. Register netdev
    ret = register_netdev(ndev);
    if(ret){
        dev_err(dev,"Failed to register netdev \n");
        free_netdev(ndev);
        return ret;
    }

    dev_info(dev,"W5500 regsitered as %s \n",ndev->name);

    return 0;
}


/*
 * Remove - cleanup
 *
 * Flow:
 *   1. Unregister net_device
 *   2. Free memory
 */
static void w5500_remove(struct spi_device *spi){

    struct w5500_priv *priv = spi_get_drvdata(spi);
    if (priv && priv->netdev) {
        unregister_netdev(priv->netdev);
        free_netdev(priv->netdev);
    }
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
