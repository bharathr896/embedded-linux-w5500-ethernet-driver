#include <linux/delay.h>
#include "w5500.h"

/*
* Hardware Reset 
* 
* RESET pin is active-low
* DT overlay declares it as GPIO_ACTIVE_LOW
* 
* Flow:
*     1. Drive RESETn Low (assert)
*     2. Delay 10ms
*     3. Drive RESETn high (deassert)
*     4. Delay 100ms for chip init 
* 
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

/*
 * Build W5500 SPI Header
 *
 * Frame = [Addr High] [Addr Low] [Control Byte]
 *
 * Control Byte format (8 bits):
 *   [7:3] Block Select (BSB)
 *   [2]   R/W bit (0=Read, 1=Write)
 *   [1:0] OM bits (Operation Mode)
 * Example:
 *   Read VERSIONR (0x0039, block=0x00):
 *     Header = 00 39 00

*/

void w5500_build_header(u16 addr, u8 block, bool write, u8 om, u8 *header){
    header[0] = ((addr) >> 8 ) & 0xFF; /* Address high byte */
    header[1] = (addr & 0xFF); /* Address low byte */
    header[2] = (
                (block & 0x1F) << 3 |  /* Block filed -> bits 7:3 */
                (write ? 1 : 0) << 2 | /* R/W bit -> bit 2  */
                (om & 0x03) /* OM field -> bits 1:0 */
    );
}


/*
* Read 1 byte from a W5500 register
* Flow:
*   1. Send 3-byte header (address + control byte)
*   2. Clock 1 dummy byte, capture MISO response
*/
int w5500_spi_read8(struct w5500_priv *priv, u16 addr, u8 *val){
    u8 header[3];
    u8 data;
    int ret;

    struct spi_transfer transfers[2] = {};
    struct spi_message msg;

    /* Builder Header - block=common,read,VDM  */
    w5500_build_header(addr,W5500_BLOCK_COMMON,false,W5500_OM_VDM,header);
    spi_message_init(&msg);

    /* Transfer 0: send header */
    transfers[0].tx_buf = header;
    transfers[0].len = 3;
    spi_message_add_tail(&transfers[0],&msg);

     /* Transfer 1: read 1 byte */
    transfers[1].rx_buf = &data;
    transfers[1].len = 1;
    spi_message_add_tail(&transfers[1],&msg);
    
    /* Run transaction */
    ret = spi_sync(priv->spi, &msg);
    if (ret < 0) {
        dev_err(&priv->spi->dev,
                "SPI read failed (addr=0x%04x, err=%d)\n", addr, ret);
        return ret;   /* Propagate real error */
    }
    
    *val = data;
    return 0;

}


/*
* Write 1 byte to a W5500 register
* Flow:
*   1. Send 3-byte header (address + control byte)
*   2. Send 1 data byte
*/
int w5500_spi_write8(struct w5500_priv *priv, u16 addr, u8 val){

    u8 buf[3];
    int ret;

    struct spi_transfer transfers = {};
    struct spi_message msg;

    w5500_build_header(addr,W5500_BLOCK_COMMON,true,W5500_OM_VDM,buf);
    buf[3] = val; /* Append data */

    spi_message_init(&msg);

    transfers.tx_buf = buf;
    transfers.len = sizeof(buf);

    spi_message_add_tail(&transfers,&msg);

    /* Run transaction */
    ret = spi_sync(priv->spi, &msg);
    if (ret < 0) {
        dev_err(&priv->spi->dev,
                "SPI write failed (addr=0x%04x, val=0x%02x, err=%d)\n",
                addr, val, ret);
        return ret;
    }

    return 0;
}