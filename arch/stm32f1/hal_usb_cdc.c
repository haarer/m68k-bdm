#include <string.h>
#include "config.h"
#include "board_config.h"
#include "hal.h"
#include "ringbuf.h"

/* ------------------------------------------------------------------ */
/*  USB CDC Descriptors                                                */
/* ------------------------------------------------------------------ */

static const uint8_t dev_desc[] = {
    18,                    /* bLength */
    0x01,                  /* bDescriptorType (Device) */
    0x00, 0x02,            /* bcdUSB 2.00 */
    0x02,                  /* bDeviceClass (Communications) */
    0x00,                  /* bDeviceSubClass */
    0x00,                  /* bDeviceProtocol */
    64,                    /* bMaxPacketSize0 */
    0x83, 0x04,            /* idVendor 0x0483 (ST) */
    0x40, 0x57,            /* idProduct 0x5740 */
    0x00, 0x02,            /* bcdDevice 2.00 */
    0x01,                  /* iManufacturer */
    0x02,                  /* iProduct */
    0x03,                  /* iSerialNumber */
    0x01                   /* bNumConfigurations */
};

static const uint8_t cfg_desc[] = {
    /* Configuration descriptor */
    9, 0x02,               /* bLength, bDescriptorType */
    67, 0x00,              /* wTotalLength */
    0x02,                  /* bNumInterfaces */
    0x01,                  /* bConfigurationValue */
    0x00,                  /* iConfiguration */
    0x80,                  /* bmAttributes (bus powered) */
    0xFA,                  /* bMaxPower (500mA) */

    /* CDC Interface (control) */
    9, 0x04,               /* bLength, bDescriptorType (Interface) */
    0x00,                  /* bInterfaceNumber */
    0x00,                  /* bAlternateSetting */
    0x01,                  /* bNumEndpoints */
    0x02,                  /* bInterfaceClass (CDC) */
    0x02,                  /* bInterfaceSubClass (ACM) */
    0x01,                  /* bInterfaceProtocol (AT commands) */
    0x00,                  /* iInterface */

    /* Header functional descriptor */
    5, 0x24, 0x00, 0x10, 0x01,

    /* Call management functional descriptor */
    5, 0x24, 0x01, 0x00, 0x01,

    /* ACM functional descriptor */
    4, 0x24, 0x02, 0x02,

    /* Union functional descriptor */
    5, 0x24, 0x06, 0x00, 0x01,

    /* Endpoint 3 (interrupt, notification) */
    7, 0x05,               /* bLength, bDescriptorType (Endpoint) */
    0x83,                  /* bEndpointAddress (EP3 IN) */
    0x03,                  /* bmAttributes (interrupt) */
    0x08, 0x00,            /* wMaxPacketSize */
    0xFF,                  /* bInterval */

    /* Data Interface */
    9, 0x04,               /* bLength, bDescriptorType (Interface) */
    0x01,                  /* bInterfaceNumber */
    0x00,                  /* bAlternateSetting */
    0x02,                  /* bNumEndpoints */
    0x0A,                  /* bInterfaceClass (CDC Data) */
    0x00,                  /* bInterfaceSubClass */
    0x00,                  /* bInterfaceProtocol */
    0x00,                  /* iInterface */

    /* Endpoint 1 (bulk IN) */
    7, 0x05,
    0x81,                  /* EP1 IN */
    0x02,                  /* bulk */
    0x40, 0x00,            /* wMaxPacketSize 64 */
    0x00,

    /* Endpoint 2 (bulk OUT) */
    7, 0x05,
    0x02,                  /* EP2 OUT */
    0x02,                  /* bulk */
    0x40, 0x00,            /* wMaxPacketSize 64 */
    0x00
};

static const uint8_t str_desc_lang[] = {
    4, 0x03, 0x09, 0x04    /* English (US) */
};

static const uint16_t str_desc_man[] = {
    (13 << 8) | 0x03,      /* bLength=13+2, bDescriptorType=3 */
    'm', '6', '8', 'k', '-', 'b', 'd', 'm'
};

static const uint16_t str_desc_prod[] = {
    (18 << 8) | 0x03,
    'B', 'D', 'M', ' ', 'B', 'r', 'i', 'd', 'g', 'e'
};

static const uint16_t str_desc_serial[] = {
    (10 << 8) | 0x03,
    '1', '2', '3', '4', '5', '6', '7', '8'
};

static const uint8_t *const str_descs[] = {
    str_desc_lang,
    (const uint8_t *)str_desc_man,
    (const uint8_t *)str_desc_prod,
    (const uint8_t *)str_desc_serial
};

/* ------------------------------------------------------------------ */
/*  USB state                                                          */
/* ------------------------------------------------------------------ */

static uint8_t dev_addr;
static ringbuf_t rx_buf;
static uint8_t ep1_tx_buf[64];
static uint16_t ep1_tx_len;
static uint16_t ep1_tx_pos;
static volatile uint8_t ep1_tx_active;
static uint8_t ep2_rx_buf[64];

/* ------------------------------------------------------------------ */
/*  PMA helpers                                                        */
/* ------------------------------------------------------------------ */

static void pma_write(uint16_t addr, const uint8_t *data, uint16_t len)
{
    volatile uint16_t *pma = (volatile uint16_t *)(USB_PMA_BASE + 2 * addr);
    for (uint16_t i = 0; i < len; i += 2) {
        uint16_t val = data[i];
        if (i + 1 < len)
            val |= (uint16_t)data[i + 1] << 8;
        *pma++ = val;
    }
}

static void pma_read(uint16_t addr, uint8_t *data, uint16_t len)
{
    volatile uint16_t *pma = (volatile uint16_t *)(USB_PMA_BASE + 2 * addr);
    for (uint16_t i = 0; i < len; i += 2) {
        uint16_t val = *pma++;
        data[i] = val & 0xFF;
        if (i + 1 < len)
            data[i + 1] = (val >> 8) & 0xFF;
    }
}

/* ------------------------------------------------------------------ */
/*  Endpoint helpers                                                   */
/* ------------------------------------------------------------------ */

static void ep_set_tx(uint8_t ep, uint16_t addr)
{
    volatile uint16_t *epreg = &USB->EP0R + ep * 2;
    uint16_t reg = *epreg;
    reg &= ~(USB_EP_STAT_TX_MASK | 0x03FF);
    reg |= USB_EP_STAT_TX_NAK | (addr & 0x03FF);
    *epreg = reg;
}

static void ep_set_rx(uint8_t ep, uint16_t addr, uint16_t count)
{
    volatile uint16_t *epreg = &USB->EP0R + ep * 2;
    uint16_t reg = *epreg;
    reg &= ~(USB_EP_STAT_RX_MASK | 0xFC00);
    /* Block count encoding: (count/32 - 1) << 10 for count > 62 */
    if (count <= 62) {
        reg |= (count << 10);
    } else {
        uint16_t blocks = (count / 32) - 1;
        reg |= (1 << 15) | (blocks << 10);
    }
    reg |= (addr & 0x03FF);
    *epreg = reg;
}

static void ep_set_stat_tx(uint8_t ep, uint16_t stat)
{
    volatile uint16_t *epreg = &USB->EP0R + ep * 2;
    uint16_t reg = *epreg;
    reg &= ~USB_EP_STAT_TX_MASK;
    reg ^= stat;  /* XOR because of toggle bits */
    *epreg = reg;
}

static void ep_set_stat_rx(uint8_t ep, uint16_t stat)
{
    volatile uint16_t *epreg = &USB->EP0R + ep * 2;
    uint16_t reg = *epreg;
    reg &= ~USB_EP_STAT_RX_MASK;
    reg ^= stat;
    *epreg = reg;
}

static void ep_init(uint8_t ep, uint16_t type, uint8_t addr)
{
    volatile uint16_t *epreg = &USB->EP0R + ep * 2;
    *epreg = ep | type | (addr & USB_EP_ADDR_MASK);
}

/* ------------------------------------------------------------------ */
/*  Control transfer state                                             */
/* ------------------------------------------------------------------ */

static uint8_t ctrl_data[64];
static uint16_t ctrl_data_len;
static uint16_t ctrl_data_pos;
static uint16_t ctrl_total_len;

static void ctrl_send_next(void)
{
    uint16_t remaining = ctrl_total_len - ctrl_data_pos;
    uint16_t chunk = (remaining < 64) ? remaining : 64;
    pma_write(0, &ctrl_data[ctrl_data_pos], chunk);
    volatile uint16_t *ep0r = &USB->EP0R;
    uint16_t reg = *ep0r;
    reg &= ~0x03FF;
    reg |= chunk;
    *ep0r = reg;
    ep_set_stat_tx(0, USB_EP_STAT_TX_VALID);
    ctrl_data_pos += chunk;
}

static void ctrl_recv_next(void)
{
    ep_set_stat_rx(0, USB_EP_STAT_RX_VALID);
}

/* ------------------------------------------------------------------ */
/*  Handle standard device requests                                    */
/* ------------------------------------------------------------------ */

static void handle_setup(uint8_t bmRequestType, uint8_t bRequest,
                         uint16_t wValue, uint16_t wIndex, uint16_t wLength)
{
    switch (bmRequestType) {
    case 0x00: /* Host to device, standard, device */
    case 0x80: /* Device to host, standard, device */
        switch (bRequest) {
        case 0: /* GET_DESCRIPTOR */
            switch (wValue >> 8) {
            case 0x01: /* Device */
                ctrl_data_len = sizeof(dev_desc);
                memcpy(ctrl_data, dev_desc, ctrl_data_len);
                break;
            case 0x02: /* Configuration */
                ctrl_data_len = sizeof(cfg_desc);
                memcpy(ctrl_data, cfg_desc, ctrl_data_len);
                break;
            case 0x03: /* String */
                {
                    uint8_t idx = wValue & 0xFF;
                    if (idx < 4) {
                        ctrl_data_len = str_descs[idx][0];
                        memcpy(ctrl_data, str_descs[idx], ctrl_data_len);
                    } else {
                        ctrl_data_len = 0;
                    }
                }
                break;
            default:
                ctrl_data_len = 0;
                break;
            }
            ctrl_total_len = (ctrl_data_len < wLength) ? ctrl_data_len : wLength;
            ctrl_data_pos = 0;
            ctrl_send_next();
            return;

        case 5: /* SET_ADDRESS */
            dev_addr = wValue & 0x7F;
            ep_set_stat_tx(0, USB_EP_STAT_TX_VALID); /* ZLP */
            return;

        case 9: /* SET_CONFIGURATION */
            /* Configure endpoints */
            ep_init(1, USB_EP_TYPE_BULK, 1);
            ep_set_tx(1, 0x40);   /* TX buffer at PMA 0x40 */
            ep_set_stat_tx(1, USB_EP_STAT_TX_NAK);

            ep_init(2, USB_EP_TYPE_BULK, 2);
            ep_set_rx(2, 0x80, 64); /* RX buffer at PMA 0x80 */
            ep_set_stat_rx(2, USB_EP_STAT_RX_VALID);

            ep_init(3, USB_EP_TYPE_INTERRUPT, 3);
            ep_set_tx(3, 0xC0);
            ep_set_stat_tx(3, USB_EP_STAT_TX_NAK);

            ep_set_stat_tx(0, USB_EP_STAT_TX_VALID); /* ZLP */
            return;
        }
        break;

    case 0x21: /* Host to device, class, interface (CDC) */
        switch (bRequest) {
        case 0x20: /* SET_LINE_CODING */
            ep_set_stat_rx(0, USB_EP_STAT_RX_VALID);
            return;
        case 0x22: /* SET_CONTROL_LINE_STATE */
            ep_set_stat_tx(0, USB_EP_STAT_TX_VALID); /* ZLP */
            return;
        }
        break;

    case 0xA1: /* Device to host, class, interface (CDC) */
        switch (bRequest) {
        case 0x21: /* GET_LINE_CODING */
            ctrl_data[0] = 0x80; ctrl_data[1] = 0x25; /* 9600 baud */
            ctrl_data[2] = 0x00; ctrl_data[3] = 0x00;
            ctrl_data[4] = 0x00; /* 1 stop bit */
            ctrl_data[5] = 0x00; /* no parity */
            ctrl_data[6] = 0x08; /* 8 data bits */
            ctrl_data_len = 7;
            ctrl_total_len = (ctrl_data_len < wLength) ? ctrl_data_len : wLength;
            ctrl_data_pos = 0;
            ctrl_send_next();
            return;
        }
        break;
    }

    /* Stall unsupported requests */
    ep_set_stat_tx(0, USB_EP_STAT_TX_STALL);
    ep_set_stat_rx(0, USB_EP_STAT_RX_VALID);
}

/* ------------------------------------------------------------------ */
/*  USB interrupt handler                                              */
/* ------------------------------------------------------------------ */

void USB_LP_CAN1_RX0_IRQHandler(void)
{
    uint16_t istr = USB->ISTR;

    if (istr & USB_ISTR_RESET) {
        USB->ISTR = ~USB_ISTR_RESET;
        /* Reset endpoints */
        for (int i = 0; i < 8; i++) {
            volatile uint16_t *epreg = &USB->EP0R + i * 2;
            *epreg = 0;
        }
        ep_init(0, USB_EP_TYPE_CONTROL, 0);
        ep_set_rx(0, 0x18, 64);
        ep_set_stat_rx(0, USB_EP_STAT_RX_VALID);
        ep_set_stat_tx(0, USB_EP_STAT_TX_NAK);
        dev_addr = 0;
        return;
    }

    if (istr & USB_ISTR_CTR) {
        uint8_t ep = istr & USB_ISTR_EP_ID_MASK;
        volatile uint16_t *epreg = &USB->EP0R + ep * 2;
        uint16_t reg = *epreg;

        if (ep == 0) {
            /* Control endpoint */
            if (reg & USB_EP_CTR_RX) {
                *epreg = reg & (USB_EP_CTR_RX | USB_EP_CTR_TX | 0x0F);
                if (!(reg & USB_ISTR_DIR)) {
                    /* OUT: data received (SET_LINE_CODING payload) */
                    uint16_t count = reg & 0x03FF;
                    pma_read(0, ctrl_data, count);
                    ctrl_recv_next();
                } else {
                    /* IN: data sent or ZLP */
                    if (ctrl_data_pos < ctrl_total_len)
                        ctrl_send_next();
                    else
                        ctrl_recv_next();
                }
            } else if (reg & USB_EP_CTR_TX) {
                *epreg = reg & (USB_EP_CTR_RX | USB_EP_CTR_TX | 0x0F);
                /* IN: data sent */
                if (ctrl_data_pos < ctrl_total_len)
                    ctrl_send_next();
                else {
                    if (dev_addr)
                        USB->DADDR = USB_DADDR_EF | dev_addr;
                    ctrl_recv_next();
                }
            }
        } else if (ep == 1) {
            /* EP1 IN (bulk TX) */
            if (reg & USB_EP_CTR_TX) {
                *epreg = reg & (USB_EP_CTR_RX | USB_EP_CTR_TX | 0x0F);
                ep1_tx_active = 0;
            }
        } else if (ep == 2) {
            /* EP2 OUT (bulk RX) */
            if (reg & USB_EP_CTR_RX) {
                uint16_t count = (reg >> 10) & 0x03FF;
                /* Block count decoding */
                if (reg & (1 << 15)) {
                    count = ((reg >> 10) & 0x1F) + 1;
                    count *= 32;
                }
                *epreg = reg & (USB_EP_CTR_RX | USB_EP_CTR_TX | 0x0F);
                pma_read(0x80, ep2_rx_buf, count);
                for (uint16_t i = 0; i < count; i++)
                    ringbuf_push(&rx_buf, ep2_rx_buf[i]);
                ep_set_stat_rx(2, USB_EP_STAT_RX_VALID);
            }
        }
        return;
    }

    /* Clear other events */
    USB->ISTR = ~(istr & (USB_ISTR_SUSP | USB_ISTR_WKUP));
}

/* ------------------------------------------------------------------ */
/*  HAL serial API                                                     */
/* ------------------------------------------------------------------ */

void hal_serial_init(uint32_t baud)
{
    (void)baud;

    /* Enable clocks */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN;
    RCC->APB1ENR |= RCC_APB1ENR_USBEN;

    /* Configure USB pull-up on PA12 (D+) and PA11 (D-) */
    /* PA11/PA12 are USB D-/D+ — alternate function push-pull */
    {
        uint32_t crh = GPIOA->CRH;
        crh &= ~(0xFF << 12);  /* clear PA11, PA12 */
        crh |= (0xB << 12) | (0xB << 16);  /* AF push-pull 50MHz */
        GPIOA->CRH = crh;
    }

    /* Configure LED on PC13 (optional, for debug) */
    {
        uint32_t crh = GPIOC->CRH;
        crh &= ~(0xF << 20);
        crh |= (0x1 << 20);  /* PC13 output push-pull */
        GPIOC->CRH = crh;
        GPIOC->BSRR = (1 << 13);  /* LED off */
    }

    /* Init ring buffer */
    ringbuf_init(&rx_buf);
    ep1_tx_active = 0;
    ep1_tx_pos = 0;
    ep1_tx_len = 0;

    /* USB peripheral init with disconnect/reconnect sequence */
    /* Step 1: Force reset (disconnects D+ pull-up) */
    USB->CNTR = USB_CNTR_FRES;
    USB->ISTR = 0;
    USB->BTABLE = 0;

    /* Step 2: Wait ~100ms so host detects disconnect */
    for (volatile uint32_t i = 0; i < 720000; i++)
        ;

    /* Step 3: Clear reset (reconnects D+ pull-up) — host sees new device */
    USB->CNTR = 0;

    /* Small delay for USB peripheral to stabilize */
    for (volatile uint32_t i = 0; i < 72000; i++)
        ;

    /* Enable interrupts */
    USB->CNTR = USB_CNTR_CTRM | USB_CNTR_RESETM | USB_CNTR_SUSPM | USB_CNTR_WKUPM;
    USB->DADDR = USB_DADDR_EF;

    /* Enable USB interrupt in NVIC (IRQ 20) */
    NVIC_ISER[0] = (1 << 20);
}

void hal_serial_putc(char c)
{
    while (ep1_tx_active)
        ;

    ep1_tx_buf[0] = (uint8_t)c;
    ep1_tx_len = 1;
    ep1_tx_pos = 0;
    ep1_tx_active = 1;

    pma_write(0x40, ep1_tx_buf, 1);
    ep_set_stat_tx(1, USB_EP_STAT_TX_VALID);
}

int hal_serial_try_putc(char c)
{
    if (ep1_tx_active)
        return 0;

    ep1_tx_buf[0] = (uint8_t)c;
    ep1_tx_len = 1;
    ep1_tx_pos = 0;
    ep1_tx_active = 1;

    pma_write(0x40, ep1_tx_buf, 1);
    ep_set_stat_tx(1, USB_EP_STAT_TX_VALID);
    return 1;
}

char hal_serial_getc(void)
{
    uint8_t data = 0;
    hal_irq_disable();
    ringbuf_pop(&rx_buf, &data);
    hal_irq_enable();
    return (char)data;
}

int hal_serial_has_data(void)
{
    int count;
    hal_irq_disable();
    count = (int)ringbuf_count(&rx_buf);
    hal_irq_enable();
    return count;
}

void hal_serial_flush(void)
{
    /* USB CDC is asynchronous; nothing to flush */
}

void hal_serial_rx_isr_handler(uint8_t byte)
{
    ringbuf_push(&rx_buf, byte);
}

/* ------------------------------------------------------------------ */
/*  Timer — SysTick at 1ms intervals                                   */
/* ------------------------------------------------------------------ */

static volatile uint32_t systick_ms;

void SysTick_Handler(void)
{
    systick_ms++;
}

void hal_timer_init(void)
{
    systick_ms = 0;
    SYSTICK->LOAD = (F_CPU / 1000) - 1;  /* 1ms tick */
    SYSTICK->VAL = 0;
    SYSTICK->CTRL = SYSTICK_CTRL_ENABLE | SYSTICK_CTRL_TICKINT | SYSTICK_CTRL_CLKSOURCE;
}

uint32_t hal_timer_get_us(void)
{
    return systick_ms * 1000 + (1000 - SYSTICK->VAL) / (F_CPU / 1000000UL);
}
