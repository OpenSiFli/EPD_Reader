/* SPDX-License-Identifier: Apache-2.0 */
#include <rtthread.h>
#include "board.h"
#include "drv_flash.h"
#include "reader_dfu.h"
#include <string.h>

extern void reader_usb_init(void);
extern int reader_usb_get(uint8_t *value);
extern int reader_usb_send(const void *data,uint32_t size);
extern int reader_usb_reset_pending(void);
static r527_session session;
static int read_flash(uint32_t a,void *p,uint32_t n){return rt_flash_read(a,p,n)==(int)n?0:-1;}
static int write_flash(uint32_t a,const void *p,uint32_t n){return rt_flash_write(a,p,n)==(int)n?0:-1;}
static int erase_flash(uint32_t a,uint32_t n){return rt_flash_erase(a,n)==RT_EOK?0:-1;}
static const r527_io io={read_flash,write_flash,erase_flash};
static uint32_t packet[5+R527_MAX_PAYLOAD/4],reply[8];
static unsigned used,expected=20;
static uint32_t last_sequence,last_crc;
static int have_reply;

static void respond(uint32_t command,uint32_t sequence,int status,uint32_t value)
{
    reply[0]=R527_WIRE_MAGIC;reply[1]=command|0x80000000u;reply[2]=sequence;
    reply[3]=12;reply[5]=(uint32_t)status;reply[6]=value;reply[7]=R527_BOARD_ID;
    uint32_t crc=r527_crc_update(~0u,reply,16);
    reply[4]=~r527_crc_update(crc,reply+5,12);
    reader_usb_send(reply,sizeof(reply));
}
static void dispatch(void)
{
    uint32_t command=packet[1],sequence=packet[2],length=packet[3];
    uint32_t crc=r527_crc_update(~0u,packet,16);
    crc=~r527_crc_update(crc,packet+5,length);
    if(crc!=packet[4]){respond(command,sequence,R527_CRC,0);have_reply=0;return;}
    if(have_reply&&sequence==last_sequence) {
        if(crc==last_crc)reader_usb_send(reply,sizeof(reply));
        else {respond(command,sequence,R527_BAD_REQUEST,0);have_reply=0;}
        return;
    }
    int rc=R527_BAD_REQUEST;uint32_t value=0;
    if(command==R527_INFO&&length==0) {
        r527_record r;rc=0;value=r527_latest(&io,&r,RT_NULL)==0?r.state:0;
    } else if(command==R527_BEGIN&&length==sizeof(r527_manifest)) {
        r527_manifest m;memcpy(&m,packet+5,sizeof(m));rc=r527_begin(&io,&session,&m);
    } else if(command==R527_DATA&&length>8) {
        rc=r527_data(&io,&session,packet[5],packet[6],packet+7,length-8);
        if(!rc)value=session.received[packet[5]];
    } else if(command==R527_COMMIT&&length==0) {
        rc=r527_commit(&io,&session);
    } else if(command==R527_ABORT&&length==0) {
        session.active=0;rc=0;
    } else if(command==R527_REBOOT&&length==0) {
        r527_record r;rc=r527_latest(&io,&r,RT_NULL);
        if(!rc)rc=r527_verify(&io,&r);
    }
    respond(command,sequence,rc,value);last_sequence=sequence;last_crc=crc;have_reply=1;
    if(command!=R527_DATA)rt_kprintf("DFU527_CMD op=%u rc=%d\n",command,rc);
    if(command==R527_REBOOT&&!rc) {
        rt_thread_mdelay(100);HAL_Set_backup(RTC_BAKCUP_OTA_FORCE_MODE,0);drv_reboot();
    }
}
int main(void)
{
    /* Hold all enables off until the one-time recovery notice is ready. */
    rt_pin_mode(3,PIN_MODE_OUTPUT);rt_pin_write(3,0);
#ifdef BSP_USING_BOARD_SF32LB527_OED_E0470A03
    extern int reader_recovery_screen(void);
    /* Display failure must not disable the independent USB recovery path. */
    reader_recovery_screen();
#endif
    HAL_PIN_Set_Analog(PAD_PA35,1);HAL_PIN_Set_Analog(PAD_PA36,1);
    rt_kprintf("DFU527_READY USB=CDC board=%08x protocol=1 protected=12000000..12200000\n",R527_BOARD_ID);
    reader_usb_init();
    while(1) {
        if(reader_usb_reset_pending()){used=0;expected=20;have_reply=0;session.active=0;}
        uint8_t byte;
        if(!reader_usb_get(&byte)){rt_thread_mdelay(1);continue;}
        ((uint8_t *)packet)[used++]=byte;
        if(used==4&&packet[0]!=R527_WIRE_MAGIC) {
            memmove(packet,(uint8_t *)packet+1,3);used=3;continue;
        }
        if(used==20) {
            if(packet[3]>R527_MAX_PAYLOAD){used=0;expected=20;session.active=0;continue;}
            expected=20+packet[3];
        }
        if(used==expected){dispatch();used=0;expected=20;}
    }
}
