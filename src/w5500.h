#ifndef _W5500_H_
#define _W5500_H_

#include <linux/spi/spi.h>
#include <linux/skbuff.h>
#include <linux/gpio/consumer.h>

/* W5500 Register Blocks */
#define W5500_BLOCK_COMMON 0x00

/* Operation Modes (OM Bits) */
#define W5500_OM_VDM 0x00 /* Variable Data Length (Default) */
#define W5500_OM_FDM1 0x01 /* Fixed data length = 1 byte */
#define W5500_OM_FDM2 0x02 /* Fixed data length = 2 bytes */
#define W5500_OM_FDM3 0x03 /* Fixed data length = 4 bytes */

/* Common Register Addresses */
#define W5500_VERSIONR 0x0039 /* Always return 0x04 for W5500 */


/* Private Data Structure
- Holds state for one W5500 instance.
- Each spi device that matches this driver will get its own copy
*/

struct w5500_priv {
    struct spi_device *spi; /* Spi Device Pointer */
    struct net_device *netdev; 
    struct gpio_desc *reset_gpio; /* Reset Pin */
    unsigned int irq;   /* Interrupt line */

    spinlock_t lock;
};

int w5500_hw_reset(struct w5500_priv *priv);


/* SPI helpers */
void w5500_build_header(u16 addr, u8 block, bool write, u8 om, u8 *header);
int w5500_spi_read8(struct w5500_priv *priv, u16 addr, u8 *val);
int w5500_spi_write8(struct w5500_priv *priv, u16 addr, u8 val);
int w5500_spi_read_bulk(struct w5500_priv *priv, u16 addr, u8 *buf, size_t len);
int w5500_spi_write_bulk(struct w5500_priv *priv, u16 addr, const u8 *buf, size_t len);

int w5500_tx_frame(struct w5500_priv *priv, struct sk_buff *skb);

#endif