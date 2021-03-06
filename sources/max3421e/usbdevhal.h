#ifndef __MAX3421E_USB_DEV_HAL_H__
#define __MAX3421E_USB_DEV_HAL_H__

#include <mem/mem.h>

#ifndef USBDEV_MAX_EP_NB
#define USBDEV_MAX_EP_NB   4
#endif

typedef struct _ep0_state_t {
    int     in_transfers;
    int     in_bytes;
    uint8_t max_size;
    int     in_ack_pending;
    int     out_ack_pending;
    int     in_packet_num;
    int     last_in_packet_num;
} ep0_state_t;

typedef struct _ep1out_state_t {
    uint8_t max_size;
} ep1out_state_t;

typedef struct _ep2in_state_t {
    int     in_transfers;
    int     in_bytes[2];
    uint8_t max_size;
} ep2in_state_t;

typedef struct _ep3in_state_t {
    int     in_transfers;
    int     in_bytes;
    uint8_t max_size;
} ep3in_state_t;


void max3421e_usbdev_init (usbdev_t *owner, spimif_t *spi, 
    unsigned spi_freq, unsigned spi_cs_num, int irq, 
    int io_prio, mem_pool_t *pool, mutex_t *m);

#endif
