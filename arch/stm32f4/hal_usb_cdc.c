#include <string.h>
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
/*  USB OTG FS FIFO layout                                             */
/* ------------------------------------------------------------------ */

/* USB RAM is 1280 bytes (320 x 32-bit words) at 0x50001000 */
#define USB_FIFO_RX_SIZE  128  /* 128 words = 512 bytes */
#define USB_FIFO_TX0_SIZE 64   /* EP0 TX: 64 words = 256 bytes */
#define USB_FIFO_TX1_SIZE 64   /* EP1 TX: 64 words = 256 bytes */
#define USB_FIFO_TX3_SIZE 16   /* EP3 TX: 16 words = 64 bytes */

#define USB_FIFO_RX_START   0
#define USB_FIFO_TX0_START  (USB_FIFO_RX_START + USB_FIFO_RX_SIZE)
#define USB_FIFO_TX1_START  (USB_FIFO_TX0_START + USB_FIFO_TX0_SIZE)
#define USB_FIFO_TX3_START  (USB_FIFO_TX1_START + USB_FIFO_TX1_SIZE)

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

/* Control transfer state */
static uint8_t ctrl_data[128];
static uint16_t ctrl_data_len;
static uint16_t ctrl_data_pos;
static uint16_t ctrl_total_len;

/* ------------------------------------------------------------------ */
/*  LED debug helpers                                                  */
/* ------------------------------------------------------------------ */

static void led_on(void)
{
    GPIOC->BSRR = (1 << (13 + 16));  /* PC13 low = LED on (sink) */
}

static void led_off(void)
{
    GPIOC->BSRR = (1 << 13);  /* PC13 high = LED off */
}

static void delay_ms(uint32_t ms)
{
    for (volatile uint32_t i = 0; i < ms * (F_CPU / 1000); i++)
        ;
}

/* ------------------------------------------------------------------ */
/*  FIFO access                                                        */
/* ------------------------------------------------------------------ */

static void fifo_write(uint32_t *fifo, const uint8_t *data, uint16_t len)
{
    uint16_t i = 0;
    while (i < len) {
        uint32_t val = data[i];
        if (i + 1 < len) val |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) val |= (uint32_t)data[i + 2] << 16;
        if (i + 3 < len) val |= (uint32_t)data[i + 3] << 24;
        *fifo = val;
        i += 4;
    }
}

static void fifo_read(uint32_t *fifo, uint8_t *data, uint16_t len)
{
    uint16_t i = 0;
    while (i < len) {
        uint32_t val = *fifo;
        data[i] = val & 0xFF;
        if (i + 1 < len) data[i + 1] = (val >> 8) & 0xFF;
        if (i + 2 < len) data[i + 2] = (val >> 16) & 0xFF;
        if (i + 3 < len) data[i + 3] = (val >> 24) & 0xFF;
        i += 4;
    }
}

/* ------------------------------------------------------------------ */
/*  Endpoint helpers                                                   */
/* ------------------------------------------------------------------ */

static void ep_in_activate(uint8_t ep, uint32_t mpsiz, uint32_t type, uint32_t txfnum)
{
    volatile uint32_t *ctl = &USB_OTG_FS_IN_EP(ep)->DIEPCTL;
    *ctl = mpsiz | type | (txfnum << 22) | USB_OTG_DIEPCTL_USBAEP |
           USB_OTG_DIEPCTL_SD0PID | USB_OTG_DIEPCTL_SNAK;
}

static void ep_out_activate(uint8_t ep, uint32_t mpsiz, uint32_t type)
{
    volatile uint32_t *ctl = &USB_OTG_FS_OUT_EP(ep)->DOEPCTL;
    *ctl = mpsiz | type | USB_OTG_DOEPCTL_USBAEP |
           USB_OTG_DOEPCTL_SD0PID | USB_OTG_DOEPCTL_SNAK;
}

static void ep_in_enable(uint8_t ep)
{
    volatile uint32_t *ctl = &USB_OTG_FS_IN_EP(ep)->DIEPCTL;
    *ctl |= USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA;
}

static void ep_out_enable(uint8_t ep)
{
    volatile uint32_t *ctl = &USB_OTG_FS_OUT_EP(ep)->DOEPCTL;
    *ctl |= USB_OTG_DOEPCTL_CNAK | USB_OTG_DOEPCTL_EPENA;
}

static void ep_in_set_size(uint8_t ep, uint16_t len)
{
    volatile uint32_t *siz = &USB_OTG_FS_IN_EP(ep)->DIEPTSIZ;
    *siz = (1 << 19) | len;  /* 1 packet, xfer size */
}

static void ep_out_set_size(uint8_t ep, uint16_t len)
{
    volatile uint32_t *siz = &USB_OTG_FS_OUT_EP(ep)->DOEPTSIZ;
    *siz = (1 << 19) | (1 << 29) | len;  /* 1 packet, 1 setup, xfer size */
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
            return;

        case 5: /* SET_ADDRESS */
            dev_addr = wValue & 0x7F;
            return;

        case 9: /* SET_CONFIGURATION */
            /* Configure endpoints */
            ep_in_activate(1, USB_OTG_DIEPCTL_MPSIZ_64, USB_OTG_DIEPCTL_EPTYP_BULK, 1);
            ep_in_enable(1);

            ep_out_activate(2, USB_OTG_DOEPCTL_MPSIZ_64, USB_OTG_DOEPCTL_EPTYP_BULK);
            ep_out_set_size(2, 64);
            ep_out_enable(2);

            ep_in_activate(3, USB_OTG_DIEPCTL_MPSIZ_64, USB_OTG_DIEPCTL_EPTYP_INTR, 2);
            ep_in_enable(3);
            return;
        }
        break;

    case 0x21: /* Host to device, class, interface (CDC) */
        switch (bRequest) {
        case 0x20: /* SET_LINE_CODING */
            return;
        case 0x22: /* SET_CONTROL_LINE_STATE */
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
            return;
        }
        break;
    }

    /* Stall unsupported requests */
    ctrl_data_len = 0;
    ctrl_total_len = 0;
}

/* ------------------------------------------------------------------ */
/*  USB interrupt handler                                              */
/* ------------------------------------------------------------------ */

void OTG_FS_IRQHandler(void)
{
    uint32_t gintsts = USB_OTG_FS->GINTSTS & USB_OTG_FS->GINTMSK;

    if (gintsts & USB_OTG_GINTSTS_USBRST) {
        USB_OTG_FS->GINTSTS = USB_OTG_GINTSTS_USBRST;
        /* Reset device address */
        USB_OTG_FS_DEV->DCFG &= ~(0x7F << 4);
        /* Setup EP0 OUT */
        ep_out_activate(0, USB_OTG_DIEPCTL_MPSIZ_64, USB_OTG_DOEPCTL_EPTYP_CTRL);
        ep_out_set_size(0, 64);
        ep_out_enable(0);
        /* Setup EP0 IN */
        ep_in_activate(0, USB_OTG_DIEPCTL_MPSIZ_64, USB_OTG_DIEPCTL_EPTYP_CTRL, 0);
        ep_in_enable(0);
        return;
    }

    if (gintsts & USB_OTG_GINTSTS_ENUMDNE) {
        USB_OTG_FS->GINTSTS = USB_OTG_GINTSTS_ENUMDNE;
        /* Set device speed */
        USB_OTG_FS_DEV->DCFG = (USB_OTG_FS_DEV->DCFG & ~(0x3 << 0)) | USB_OTG_DCFG_DSPD_FS;
        /* Enable RX and EP interrupts */
        USB_OTG_FS->GINTMSK |= USB_OTG_GINTMSK_RXFLVLM | USB_OTG_GINTMSK_CTRM;
        /* USB enumerated successfully — LED on */
        led_on();
        return;
    }

    if (gintsts & USB_OTG_GINTSTS_RXFLVL) {
        uint32_t grxstsp = USB_OTG_FS->GRXSTSP;
        uint8_t ep_num = grxstsp & 0xF;
        uint32_t pktsts = (grxstsp >> 17) & 0xF;
        uint16_t bcnt = (grxstsp >> 4) & 0x7FF;

        switch (pktsts) {
        case 0x02: /* OUT data received */
            if (ep_num == 2) {
                fifo_read((uint32_t *)(USB_OTG_FS_BASE + 0x1000), ep2_rx_buf, bcnt);
                for (uint16_t i = 0; i < bcnt; i++)
                    ringbuf_push(&rx_buf, ep2_rx_buf[i]);
                ep_out_set_size(2, 64);
                ep_out_enable(2);
            } else if (ep_num == 0) {
                fifo_read((uint32_t *)(USB_OTG_FS_BASE + 0x1000), ctrl_data, bcnt);
                if (ctrl_data_pos < ctrl_total_len) {
                    ctrl_data_pos += bcnt;
                }
            }
            break;

        case 0x06: /* SETUP data received */
            fifo_read((uint32_t *)(USB_OTG_FS_BASE + 0x1000), ctrl_data, 8);
            {
                uint16_t wLength = (ctrl_data[6] | (ctrl_data[7] << 8));
                uint16_t wValue = (ctrl_data[2] | (ctrl_data[3] << 8));
                uint16_t wIndex = (ctrl_data[4] | (ctrl_data[5] << 8));
                handle_setup(ctrl_data[0], ctrl_data[1], wValue, wIndex, wLength);
            }
            if (ctrl_data[0] & 0x80) {
                /* IN data stage */
                uint16_t remaining = ctrl_total_len - ctrl_data_pos;
                uint16_t chunk = (remaining < 64) ? remaining : 64;
                ep_in_set_size(0, chunk ? chunk : 1);
                fifo_write((uint32_t *)(USB_OTG_FS_BASE + 0x1000),
                          &ctrl_data[ctrl_data_pos], chunk ? chunk : 1);
                ep_in_enable(0);
                ctrl_data_pos += chunk;
            } else {
                /* OUT data stage or status */
                ep_out_set_size(0, 64);
                ep_out_enable(0);
            }
            break;

        case 0x03: /* OUT completed */
        case 0x04: /* SETUP completed */
            /* Read to clear FIFO */
            (void)USB_OTG_FS->GRXSTSP;
            break;
        }
        return;
    }

    /* IN endpoint interrupts */
    if (gintsts & USB_OTG_GINTSTS_IEPINT) {
        /* EP1 IN (bulk TX) */
        if (USB_OTG_FS_IN_EP(1)->DIEPINT & USB_OTG_DIEPINT_XFRC) {
            USB_OTG_FS_IN_EP(1)->DIEPINT = USB_OTG_DIEPINT_XFRC;
            ep1_tx_active = 0;
        }
        /* EP0 IN */
        if (USB_OTG_FS_IN_EP(0)->DIEPINT & USB_OTG_DIEPINT_XFRC) {
            USB_OTG_FS_IN_EP(0)->DIEPINT = USB_OTG_DIEPINT_XFRC;
            if (ctrl_data_pos < ctrl_total_len) {
                uint16_t remaining = ctrl_total_len - ctrl_data_pos;
                uint16_t chunk = (remaining < 64) ? remaining : 64;
                ep_in_set_size(0, chunk ? chunk : 1);
                fifo_write((uint32_t *)(USB_OTG_FS_BASE + 0x1000),
                          &ctrl_data[ctrl_data_pos], chunk ? chunk : 1);
                ep_in_enable(0);
                ctrl_data_pos += chunk;
            } else {
                /* Status OUT expected */
                ep_out_set_size(0, 64);
                ep_out_enable(0);
            }
        }
        return;
    }

    /* OUT endpoint interrupts */
    if (gintsts & USB_OTG_GINTSTS_OEPINT) {
        if (USB_OTG_FS_OUT_EP(0)->DOEPINT & (USB_OTG_DOEPINT_XFRC | USB_OTG_DOEPINT_STUP)) {
            USB_OTG_FS_OUT_EP(0)->DOEPINT = USB_OTG_DOEPINT_XFRC | USB_OTG_DOEPINT_STUP;
            ep_out_set_size(0, 64);
            ep_out_enable(0);
        }
        return;
    }

    /* Clear other events */
    USB_OTG_FS->GINTSTS = gintsts & (USB_OTG_GINTSTS_USBSUSP | USB_OTG_GINTSTS_WKUPINT);
}

/* ------------------------------------------------------------------ */
/*  HAL serial API                                                     */
/* ------------------------------------------------------------------ */

void hal_serial_init(uint32_t baud)
{
    (void)baud;

    /* Enable clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_OTGFSEN;

    /* Configure LED on PC13 (sink: low=on, high=off) */
    {
        uint32_t shift = 13 * 2;
        GPIOC->MODER = (GPIOC->MODER & ~(0x3 << shift)) | (0x1 << shift);
        GPIOC->OTYPER &= ~(1 << 13);  /* Push-pull */
        led_off();
    }

    /* 5 short flashes — confirms code is running and LED works */
    for (int i = 0; i < 5; i++) {
        led_on();
        delay_ms(100);
        led_off();
        delay_ms(100);
    }

    /* Configure USB pins PA11 (D-) and PA12 (D+) as AF10 (OTG_FS) */
    {
        uint32_t shift = 11 * 2;
        GPIOA->MODER = (GPIOA->MODER & ~(0x3 << shift)) | (0x2 << shift);
        shift = 12 * 2;
        GPIOA->MODER = (GPIOA->MODER & ~(0x3 << shift)) | (0x2 << shift);
        GPIOA->OSPEEDR |= (0x3 << (11 * 2)) | (0x3 << (12 * 2));
        GPIOA->PUPDR |= (0x1 << (11 * 2)) | (0x1 << (12 * 2));
        GPIOA->AFR[1] = (GPIOA->AFR[1] & ~((0xF << 12) | (0xF << 16))) |
                        (0xA << 12) | (0xA << 16);
    }

    /* Init ring buffer */
    ringbuf_init(&rx_buf);
    ep1_tx_active = 0;
    ep1_tx_pos = 0;
    ep1_tx_len = 0;

    /* USB OTG FS core reset */
    USB_OTG_FS->GRSTCTL = USB_OTG_GRSTCTL_CSRST;
    while (!(USB_OTG_FS->GRSTCTL & USB_OTG_GRSTCTL_AHBIDL))
        ;

    /* Force device mode */
    USB_OTG_FS->GUSBCFG = USB_OTG_GUSBCFG_PHYSEL | USB_OTG_GUSBCFG_TRDT_6 | USB_OTG_GUSBCFG_FDMOD;

    /* Wait for mode switch (min 25ms per RM) */
    delay_ms(30);

    /* Core reset again after mode switch */
    USB_OTG_FS->GRSTCTL = USB_OTG_GRSTCTL_CSRST;
    while (!(USB_OTG_FS->GRSTCTL & USB_OTG_GRSTCTL_AHBIDL))
        ;

    /* Flush TX and RX FIFOs */
    USB_OTG_FS->GRSTCTL = (3 << 6) | USB_OTG_GRSTCTL_TXFFLSH;  /* Flush all TX */
    while (USB_OTG_FS->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH)
        ;
    USB_OTG_FS->GRSTCTL = USB_OTG_GRSTCTL_RXFFLSH;  /* Flush RX */
    while (USB_OTG_FS->GRSTCTL & USB_OTG_GRSTCTL_RXFFLSH)
        ;

    /* Configure device */
    USB_OTG_FS_DEV->DCFG = USB_OTG_DCFG_DSPD_FS | USB_OTG_DCFG_NZLSOHSK;

    /* Configure FIFOs */
    USB_OTG_FS->GRXFSIZ = USB_FIFO_RX_SIZE;
    USB_OTG_FS->DIEPTXF0_HNPTXFSIZ = (USB_FIFO_TX0_SIZE << 16) | USB_FIFO_TX0_START;
    USB_OTG_FS->DIEPTXF[0] = (USB_FIFO_TX1_SIZE << 16) | USB_FIFO_TX1_START;  /* EP1 */
    USB_OTG_FS->DIEPTXF[2] = (USB_FIFO_TX3_SIZE << 16) | USB_FIFO_TX3_START;  /* EP3 */

    /* Enable PHY and disable VBUS sensing */
    USB_OTG_FS->GCCFG = USB_OTG_GCCFG_PWRDWN | USB_OTG_GCCFG_NOVBUSSENS;

    /* Soft disconnect, then reconnect */
    USB_OTG_FS_DEV->DCTL |= USB_OTG_DCTL_SDIS;
    delay_ms(50);
    USB_OTG_FS_DEV->DCTL &= ~USB_OTG_DCTL_SDIS;

    /* Enable interrupts */
    USB_OTG_FS->GAHBCFG = USB_OTG_GAHBCFG_GINT;
    USB_OTG_FS->GINTMSK = USB_OTG_GINTMSK_USBRST | USB_OTG_GINTMSK_ENUMDNEM |
                          USB_OTG_GINTMSK_USBSUSPM | USB_OTG_GINTMSK_WUIM;

    /* Enable USB interrupt in NVIC (IRQ 67) */
    NVIC_ISER[2] = (1 << (67 - 64));
}

void hal_serial_putc(char c)
{
    while (ep1_tx_active)
        ;

    ep1_tx_buf[0] = (uint8_t)c;
    ep1_tx_len = 1;
    ep1_tx_pos = 0;
    ep1_tx_active = 1;

    ep_in_set_size(1, 1);
    fifo_write((uint32_t *)(USB_OTG_FS_BASE + 0x1000 + USB_FIFO_TX1_START * 4),
              ep1_tx_buf, 1);
    ep_in_enable(1);
}

int hal_serial_try_putc(char c)
{
    if (ep1_tx_active)
        return 0;

    ep1_tx_buf[0] = (uint8_t)c;
    ep1_tx_len = 1;
    ep1_tx_pos = 0;
    ep1_tx_active = 1;

    ep_in_set_size(1, 1);
    fifo_write((uint32_t *)(USB_OTG_FS_BASE + 0x1000 + USB_FIFO_TX1_START * 4),
              ep1_tx_buf, 1);
    ep_in_enable(1);
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
