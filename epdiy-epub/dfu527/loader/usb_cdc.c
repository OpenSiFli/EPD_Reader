/* SPDX-License-Identifier: Apache-2.0
 * CDC descriptors/endpoint setup follow the pinned SiFli CherryUSB example.
 */
#include "usbd_core.h"
#include "usbd_cdc_acm.h"
#include "bf0_hal.h"
#include <rthw.h>
#include <string.h>
#define OUT_EP 0x03
#define IN_EP 0x85
#define INT_EP 0x86
#define MPS 64
#define RING_BYTES 8192u
static const uint8_t device_desc[]={USB_DEVICE_DESCRIPTOR_INIT(USB_2_0,0xef,2,1,0x38f4,1,0x0100,1)};
static const uint8_t config_desc[]={
    USB_CONFIG_DESCRIPTOR_INIT(9+CDC_ACM_DESCRIPTOR_LEN,2,1,USB_CONFIG_BUS_POWERED,100),
    CDC_ACM_DESCRIPTOR_INIT(0,INT_EP,OUT_EP,IN_EP,MPS,2)
};
static const char *strings[]={(const char[]){9,4},"SiFli","EPD Reader 527 Recovery","EPD527-RECOVERY-1"};
static const uint8_t *device_cb(uint8_t speed){(void)speed;return device_desc;}
static const uint8_t *config_cb(uint8_t speed){(void)speed;return config_desc;}
static const char *string_cb(uint8_t speed,uint8_t index){(void)speed;return index<4?strings[index]:RT_NULL;}
static const struct usb_descriptor descriptor={.device_descriptor_callback=device_cb,
    .config_descriptor_callback=config_cb,.string_descriptor_callback=string_cb};
static uint8_t ring[RING_BYTES];
static volatile unsigned head,tail,reset_pending,configured,tx_busy;
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX static uint8_t rx[MPS],tx[64];
static void event(uint8_t busid,uint8_t e)
{
    if(e==USBD_EVENT_RESET||e==USBD_EVENT_DISCONNECTED) {
        configured=0;tx_busy=0;head=tail=0;reset_pending=1;
    } else if(e==USBD_EVENT_CONFIGURED) {
        configured=1;tx_busy=0;usbd_ep_start_read(busid,OUT_EP,rx,sizeof(rx));
    }
}
static void out_cb(uint8_t busid,uint8_t ep,uint32_t n)
{
    (void)ep;
    for(unsigned i=0;i<n;++i) {
        unsigned next=(head+1)&(RING_BYTES-1);
        if(next==tail){head=tail=0;reset_pending=1;break;}
        ring[head]=rx[i];head=next;
    }
    usbd_ep_start_read(busid,OUT_EP,rx,sizeof(rx));
}
static void in_cb(uint8_t busid,uint8_t ep,uint32_t n)
{
    (void)ep;
    if(n&&n%MPS==0)usbd_ep_start_write(busid,IN_EP,RT_NULL,0);
    else tx_busy=0;
}
static struct usbd_endpoint out={.ep_addr=OUT_EP,.ep_cb=out_cb},in={.ep_addr=IN_EP,.ep_cb=in_cb};
static struct usbd_interface intf0,intf1;
void reader_usb_init(void)
{
    usbd_desc_register(0,&descriptor);
    usbd_add_interface(0,usbd_cdc_acm_init_intf(0,&intf0));
    usbd_add_interface(0,usbd_cdc_acm_init_intf(0,&intf1));
    usbd_add_endpoint(0,&out);usbd_add_endpoint(0,&in);
    usbd_initialize(0,(uintptr_t)USBC_BASE,event);
}
int reader_usb_reset_pending(void)
{
    rt_base_t level=rt_hw_interrupt_disable();int result=reset_pending;
    reset_pending=0;rt_hw_interrupt_enable(level);return result;
}
int reader_usb_get(uint8_t *value)
{
    rt_base_t level=rt_hw_interrupt_disable();int result=tail!=head;
    if(result){*value=ring[tail];tail=(tail+1)&(RING_BYTES-1);}
    rt_hw_interrupt_enable(level);return result;
}
int reader_usb_send(const void *data,uint32_t size)
{
    if(!configured||size>sizeof(tx))return -1;
    rt_tick_t start=rt_tick_get();
    while(tx_busy&&rt_tick_get()-start<rt_tick_from_millisecond(2000))rt_thread_mdelay(1);
    /* Never reuse an in-flight buffer, even after timeout. */
    if(tx_busy||!configured)return -1;
    memcpy(tx,data,size);tx_busy=1;
    if(usbd_ep_start_write(0,IN_EP,tx,size)<0){tx_busy=0;return -1;}
    return 0;
}
void usbd_cdc_acm_set_dtr(uint8_t busid,uint8_t intf,bool dtr)
{
    (void)busid;(void)intf;
    if(!dtr){head=tail=0;reset_pending=1;}
}
void usbd_cdc_acm_set_rts(uint8_t busid,uint8_t intf,bool rts){(void)busid;(void)intf;(void)rts;}
void usbd_cdc_acm_set_line_coding(uint8_t busid,uint8_t intf,struct cdc_line_coding *c){(void)busid;(void)intf;(void)c;}
void usbd_cdc_acm_get_line_coding(uint8_t busid,uint8_t intf,struct cdc_line_coding *c)
{(void)busid;(void)intf;c->dwDTERate=1000000;c->bDataBits=8;c->bParityType=0;c->bCharFormat=0;}
