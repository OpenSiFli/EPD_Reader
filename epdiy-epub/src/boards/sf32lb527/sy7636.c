/* SPDX-License-Identifier: Apache-2.0
 * Wiring recovered from original application: AW9523B i2c2 PA20/30 @0x59,
 * reset PA3; GE6 SY7636 EN, GE4 VCOM_EN; SY7636 i2c1 PA10/11 @0x62.
 * Register definitions: Silergy SY7636A Rev 0.9B. Never write reserved registers.
 */
#include <rtthread.h>
#include <rtdevice.h>
#ifndef READER527_RECOVERY_DISPLAY
#include <finsh.h>
#else
#define MSH_CMD_EXPORT(name,description)
#endif
#include "board.h"
#include "epd_tps.h" /* Existing panel API names retained. */
#include "board527.h"

static struct rt_i2c_bus_device *aw_bus,*sy_bus;
static int configured,powered,aw_ready;
static uint16_t vcom_saved=1100;
static unsigned cycles,errors;
static struct rt_mutex aw_lock;
static int aw_lock_ready;
static unsigned aw_generation;
static rt_tick_t power_off_tick;
static int discharge_pending;

static int raw_read(struct rt_i2c_bus_device *bus,unsigned addr,uint8_t reg,uint8_t *value)
{
    struct rt_i2c_msg m[2]={{.addr=addr,.flags=RT_I2C_WR,.buf=&reg,.len=1},
                          {.addr=addr,.flags=RT_I2C_RD,.buf=value,.len=1}};
    return bus && rt_i2c_transfer(bus,m,2)==2?0:-1;
}
static int reg_read(struct rt_i2c_bus_device *bus,unsigned addr,uint8_t reg,uint8_t *value)
{
    if(addr!=0x62)return raw_read(bus,addr,reg,value);
    if(board527_io_enter())return -1;
    int rc=raw_read(bus,addr,reg,value);board527_io_exit();return rc;
}
static int raw_write(struct rt_i2c_bus_device *bus,unsigned addr,uint8_t reg,uint8_t value)
{
    uint8_t data[2]={reg,value},readback=0;
    struct rt_i2c_msg m={.addr=addr,.flags=RT_I2C_WR,.buf=data,.len=2};
    if(!bus || rt_i2c_transfer(bus,&m,1)!=1 || raw_read(bus,addr,reg,&readback) || readback!=value) {
        rt_kprintf("PMIC_WRITE_FAIL addr=%02x reg=%02x want=%02x got=%02x\n",addr,reg,value,readback);
        return -1;
    }
    return 0;
}
static int reg_write(struct rt_i2c_bus_device *bus,unsigned addr,uint8_t reg,uint8_t value)
{
    if(addr!=0x62)return raw_write(bus,addr,reg,value);
    if(board527_io_enter())return -1;
    int rc=raw_write(bus,addr,reg,value);board527_io_exit();return rc;
}
static struct rt_i2c_bus_device *open_bus(const char *name)
{
    struct rt_i2c_bus_device *bus=rt_i2c_bus_device_find(name);
    struct rt_i2c_configuration cfg={.mode=0,.addr=0,.timeout=50,.max_hz=100000};
    if(!bus || rt_device_open((rt_device_t)bus,RT_DEVICE_OFLAG_RDWR)!=RT_EOK || rt_i2c_configure(bus,&cfg)!=RT_EOK) return RT_NULL;
    return bus;
}
static int ge(unsigned pin,int value)
{
    uint8_t v,reg=pin<8?2:3,bit=1u<<(pin&7);
    if(!aw_ready || !rt_pin_read(3))return -1;
    rt_mutex_take(&aw_lock,RT_WAITING_FOREVER);
    if(reg_read(aw_bus,0x59,reg,&v)) {rt_mutex_release(&aw_lock);return -1;}
    v=value?(v|bit):(v&~bit);
    int rc=reg_write(aw_bus,0x59,reg,v);
    rt_mutex_release(&aw_lock);
    return rc;
}
int board_touch_reset(int high) { return ge(13,high!=0); }
int board527_logic_sleep_ready(void)
{
    return !powered&&!configured&&
        (!discharge_pending||(rt_tick_t)(rt_tick_get()-power_off_tick)>=rt_tick_from_millisecond(450));
}
int board527_logic_power(int on)
{
    if(!on&&!board527_logic_sleep_ready())return -RT_EBUSY;
    uint8_t in=0;int rc=ge(14,on!=0);
    if(!rc)rc=reg_read(aw_bus,0x59,1,&in);
    if(!rc&&((in>>6)&1)!=(on!=0))rc=-RT_EIO;
    return rc;
}
int board_tf_present(void)
{
    uint8_t value=0;
    if(!aw_ready || !rt_pin_read(3) || reg_read(aw_bus,0x59,0,&value))return -1;
    return !(value&1); /* Original pin1000=GE0, active low. */
}
int board_frontlight_enable(int high)
{
    /* Original LM3630A driver controls both pins. Enable the companion
     * path before HWEN; remove HWEN before switching the companion off. */
    int a=ge(high?11:3,high!=0);
    int b=ge(high?3:11,high!=0);
    return a||b ? -1 : 0;
}
int board_frontlight_gpio_report(void)
{
    uint8_t p0=0,p1=0,o0=0,o1=0;
    int rc=reg_read(aw_bus,0x59,0,&p0)|reg_read(aw_bus,0x59,1,&p1)|
        reg_read(aw_bus,0x59,2,&o0)|reg_read(aw_bus,0x59,3,&o1);
    rt_kprintf("FL_GPIO rc=%d GE3_actual=%u latch=%u GE11_actual=%u latch=%u GE1_in=%u\n",
        rc,(p0>>3)&1,(o0>>3)&1,(p1>>3)&1,(o1>>3)&1,(p0>>1)&1);
    return rc;
}
unsigned board_expander_generation(void) { return aw_generation; }
int board527_steps_aw(int reset_hold,int logic_on)
{
    if(powered||configured||!aw_bus)return -RT_EBUSY;
    if(reset_hold) {
        rt_pin_write(3,0);aw_ready=0;++aw_generation;
        return 0;
    }
    rt_pin_write(3,1);rt_thread_mdelay(5);
    uint8_t id=0,p0=0,p1=0;int rc=0;
    rt_mutex_take(&aw_lock,RT_WAITING_FOREVER);
    if(reg_read(aw_bus,0x59,0x10,&id)||id!=0x23){rc=-RT_ERROR;goto steps_aw_done;}
    // Never re-enable the original default GE14/15 during this initialization.
    rc=reg_write(aw_bus,0x59,2,0)|reg_write(aw_bus,0x59,3,0)|
       reg_write(aw_bus,0x59,0x11,0x10)|reg_write(aw_bus,0x59,0x12,0xff)|reg_write(aw_bus,0x59,0x13,0xff)|
       reg_write(aw_bus,0x59,4,0x27)|reg_write(aw_bus,0x59,5,0x07)|
       reg_write(aw_bus,0x59,6,0xff)|reg_write(aw_bus,0x59,7,0xff);
    if(!rc&&logic_on)rc=reg_write(aw_bus,0x59,3,0x40);
    rc|=reg_read(aw_bus,0x59,0,&p0)|reg_read(aw_bus,0x59,1,&p1);
    if((p0&0xd8)||(p1&0xf8)!=(logic_on?0x40:0))rc=-RT_ERROR;
steps_aw_done:
    aw_ready=!rc;
    rt_mutex_release(&aw_lock);
    if(rc||BOARD527_VERBOSE_LOGS)
        rt_kprintf("STEP_AW reset=0 logic=%d id=%02x p0=%02x p1=%02x rc=%d\n",logic_on,id,p0,p1,rc);
    return rc;
}
static uint8_t test_p1_out,test_p1_dir;
static int test_rail_saved;
static uint8_t test_amp_out,test_amp_dir;
static int test_amp_saved;
int board527_amplifier_report(void)
{
    uint8_t input=0,output=0,dir=0;
    rt_mutex_take(&aw_lock,RT_WAITING_FOREVER);
    int rc=reg_read(aw_bus,0x59,0,&input)|reg_read(aw_bus,0x59,2,&output)|reg_read(aw_bus,0x59,4,&dir);
    rt_kprintf("AMP_STATUS rc=%d GE7_actual=%u latch=%u input_mode=%u\n",rc,(input>>7)&1,(output>>7)&1,(dir>>7)&1);
    rt_mutex_release(&aw_lock);return rc;
}
int board527_amplifier_test(int mute)
{
    uint8_t output=0,dir=0;int rc=-RT_ERROR;
    if(!aw_ready||!rt_pin_read(3))return rc;
    rt_mutex_take(&aw_lock,RT_WAITING_FOREVER);
    if(reg_read(aw_bus,0x59,2,&output)||reg_read(aw_bus,0x59,4,&dir))goto amp_done;
    if(mute) {
        if(!test_amp_saved){test_amp_out=output&0x80;test_amp_dir=dir&0x80;test_amp_saved=1;}
        // Original PA control at 0x12203fc0: pin1007 output, level=!mute.
        rc=reg_write(aw_bus,0x59,2,output&0x7f)|reg_write(aw_bus,0x59,4,dir&0x7f);
    } else if(test_amp_saved) {
        rc=reg_write(aw_bus,0x59,2,(output&0x7f)|test_amp_out)|reg_write(aw_bus,0x59,4,(dir&0x7f)|test_amp_dir);
        if(!rc)test_amp_saved=0;
    } else rc=0;
amp_done:
    rt_mutex_release(&aw_lock);
    if(rc||BOARD527_VERBOSE_LOGS)board527_amplifier_report();
    return rc;
}
int board527_logic_rail_test(int off)
{
    int rc=0;uint8_t inputs=0;
    if(!aw_ready||!rt_pin_read(3)||powered||configured)return -RT_EBUSY;
    rt_mutex_take(&aw_lock,RT_WAITING_FOREVER);
    if(off) {
        if(test_rail_saved){rc=-RT_EBUSY;goto done;}
        if(reg_read(aw_bus,0x59,3,&test_p1_out)||reg_read(aw_bus,0x59,5,&test_p1_dir)) {rc=-RT_ERROR;goto done;}
        test_rail_saved=1;
        /* Original main firmware cuts GE15/GE14 for GUI sleep. Keep AW and
         * all other outputs alive; snapshot direction as GE15 was an input. */
        rc=reg_write(aw_bus,0x59,3,test_p1_out&~0xc0)|reg_write(aw_bus,0x59,5,test_p1_dir&~0xc0);
    } else if(test_rail_saved) {
        rc=reg_write(aw_bus,0x59,3,test_p1_out)|reg_write(aw_bus,0x59,5,test_p1_dir);
        if(!rc)test_rail_saved=0;
    }
    rc|=reg_read(aw_bus,0x59,1,&inputs);
    rt_kprintf("PWR_LOGIC_RAIL off=%d rc=%d GE14=%u GE15=%u\n",off,rc,(inputs>>6)&1,(inputs>>7)&1);
done:
    rt_mutex_release(&aw_lock);return rc;
}

int board527_minimum_external_off(void)
{
    uint8_t in0=0,in1=0;
    if(!aw_ready||powered||configured)return -RT_EBUSY;
    rt_mutex_take(&aw_lock,RT_WAITING_FOREVER);
    /* Clear latches BEFORE selecting output. All mapped enables/reset lines:
     * GE3/11 frontlight, GE4/6 panel HV, GE7 amplifier, GE12 Wi-Fi reset,
     * GE13 touch reset, GE14 common 3V3, GE15 Wi-Fi power. */
    int rc=reg_write(aw_bus,0x59,2,0)|reg_write(aw_bus,0x59,3,0)|
           reg_write(aw_bus,0x59,4,0x27)|reg_write(aw_bus,0x59,5,0x07);
    rc|=reg_read(aw_bus,0x59,0,&in0)|reg_read(aw_bus,0x59,1,&in1);
    if((in0&0xd8)||(in1&0xf8))rc=-RT_ERROR;
    if(rc||BOARD527_VERBOSE_LOGS)
        rt_kprintf("MINPOWER_AW enables_low=%d P0=%02x P1=%02x rc=%d\n",!rc,in0,in1,rc);
    rt_mutex_release(&aw_lock);return rc;
}
int board_touch_gpio_report(void)
{
    uint8_t in=0,out=0,dir=0,mode=0;
    int rc=reg_read(aw_bus,0x59,1,&in)|reg_read(aw_bus,0x59,3,&out)|
           reg_read(aw_bus,0x59,5,&dir)|reg_read(aw_bus,0x59,0x13,&mode);
    rt_kprintf("TP_AW rc=%d P1_in=%02x out=%02x dir=%02x mode=%02x GE13_actual=%u GE13_latch=%u GE13_input=%u\n",
               rc,in,out,dir,mode,(in>>5)&1,(out>>5)&1,(dir>>5)&1);
    return rc;
}
static int aw_init(void)
{
    uint8_t id=0;
    if(!aw_lock_ready) {rt_mutex_init(&aw_lock,"awlock",RT_IPC_FLAG_PRIO);aw_lock_ready=1;}
    /* VCOM changes must not reset the shared expander or drop touch reset. */
    if(aw_ready && rt_pin_read(3) && !reg_read(aw_bus,0x59,0x10,&id) && id==0x23)return 0;
    aw_ready=0;
    rt_pin_mode(3,PIN_MODE_OUTPUT);rt_pin_write(3,0);rt_thread_mdelay(2);
    rt_pin_write(3,1);rt_thread_mdelay(5);
    if(!aw_bus)aw_bus=open_bus("i2c2");
    if(reg_read(aw_bus,0x59,0x10,&id)||id!=0x23) {
        rt_kprintf("AW9523B_FAIL id=%02x i2c2 SCL_PA20=%d SDA_PA30=%d RST_PA3=%d\n",id,rt_pin_read(20),rt_pin_read(30),rt_pin_read(3));return -1;
    }
    /* GE13 is touch reset, initially low. Latches precede directions. */
    if(reg_write(aw_bus,0x59,2,0) || reg_write(aw_bus,0x59,3,0x40) ||
       reg_write(aw_bus,0x59,4,0xa7) || reg_write(aw_bus,0x59,5,0x97) ||
       reg_write(aw_bus,0x59,6,0xff) || reg_write(aw_bus,0x59,7,0xff) ||
       reg_write(aw_bus,0x59,0x11,0x10) || reg_write(aw_bus,0x59,0x12,0xff) ||
       reg_write(aw_bus,0x59,0x13,0xff))return -1;
    aw_ready=1;
    ++aw_generation;
    rt_thread_mdelay(50);
    rt_kprintf("AW9523B_READY id=23 GE4=0 GE6=0 GE13=TOUCH_RESET_LOW GE14=1 others=input\n");
    rt_kprintf("AW_FRONTLIGHT GE3=0 GE11=0 outputs; GE1=input\n");
    return 0;
}
static int sy_config(void)
{
    uint8_t op=0,h=0,ldo=0;
    /* Preserve the complete 450 ms EN-low discharge interval. Let the UI and
     * waveform preparation run during it, then wait only for any remainder. */
    if(discharge_pending) {
        rt_tick_t interval=rt_tick_from_millisecond(450);
        rt_tick_t elapsed=rt_tick_get()-power_off_tick;
        if(elapsed<interval)rt_thread_delay(interval-elapsed);
        discharge_pending=0;
    }
    if(ge(4,0)||ge(6,1))return -1;
    rt_thread_mdelay(5);
    if(!sy_bus)sy_bus=open_bus("i2c1");
    if(reg_read(sy_bus,0x62,0,&op))return -1;
    /* ON_OFF=0, manual VCOM. Preserve only reserved bit5. */
    if(reg_write(sy_bus,0x62,0,(op&0x20)|0x40) ||
       reg_read(sy_bus,0x62,2,&h) || reg_read(sy_bus,0x62,3,&ldo) ||
       reg_write(sy_bus,0x62,1,(vcom_saved/10)&255) ||
       reg_write(sy_bus,0x62,2,(h&0x7f)|((vcom_saved/10)&0x100?0x80:0)) ||
       reg_write(sy_bus,0x62,3,(ldo&0x1f)|0x60))return -1;
    configured=1;
    return 0;
}
rt_err_t tps_init(uint16_t vcom)
{
    configured=powered=0;vcom_saved=vcom;
    if(vcom>5000 || aw_init() || sy_config()) {
        ++errors;ge(4,0);ge(6,0);
        rt_kprintf("SY7636_INIT_FAIL; no ON_OFF enable sent\n");return -RT_ERROR;
    }
    uint8_t op=0,fault=0;
    reg_read(sy_bus,0x62,0,&op);reg_read(sy_bus,0x62,7,&fault);
    rt_kprintf("SY7636_READY addr=62 op=%02x fault=%02x VCOM=-%umV HV_COMMAND=OFF\n",op,fault,vcom);
    return RT_EOK;
}
rt_err_t tps_enter_sleep(void)
{
    if(!rt_pin_read(3)) {configured=powered=aw_ready=0;return -RT_ERROR;}
    int rc=ge(4,0);
    uint8_t op=0;
    if(configured) {
        if(reg_read(sy_bus,0x62,0,&op)||reg_write(sy_bus,0x62,0,(op&0x20)|0x40))rc=-1;
    }
    rt_thread_mdelay(150);
    if(ge(6,0))rc=-1;
    power_off_tick=rt_tick_get();discharge_pending=1;
    configured=powered=0;
    return rc? -RT_ERROR:RT_EOK;
}
rt_err_t tps_exit_sleep(void)
{
    uint8_t op=0,fault=0;
    if(!aw_ready || (!configured && sy_config()))goto fail;
    if(reg_read(sy_bus,0x62,0,&op)||reg_write(sy_bus,0x62,0,(op&0x20)|0xc0))goto fail;
    for(unsigned i=0;i<100;++i) {
        if(reg_read(sy_bus,0x62,7,&fault))goto fail;
        if(fault&1) {
            if(ge(4,1))goto fail;
            powered=1;++cycles;rt_thread_mdelay(10);
#if BOARD527_VERBOSE_LOGS
            rt_kprintf("SY7636_POWER_OK fault=%02x cycle=%u\n",fault,cycles);
#endif
            return RT_EOK;
        }
        rt_thread_mdelay(5);
    }
fail:
    ++errors;rt_kprintf("SY7636_POWER_FAIL fault=%02x\n",fault);tps_enter_sleep();return -RT_ERROR;
}
int tps_bus_probe(void)
{
    uint8_t id=0;
    int rc=reg_read(aw_bus,0x59,0x10,&id);
    rt_kprintf("AW_PROBE rc=%d id=%02x\n",rc,id);
    if(configured)for(uint8_t r=0;r<=8;++r) {
        uint8_t v=0;rc=reg_read(sy_bus,0x62,r,&v);
        rt_kprintf("SY_REG %02x=%02x rc=%d\n",r,v,rc);
    }
    return rc;
}
static int epd_power(int argc,char **argv)
{
    (void)argc;(void)argv;
    tps_bus_probe();
    rt_kprintf("PMIC_STATUS configured=%d powered=%d cycles=%u errors=%u GE4=VCOM GE6=EN GE14=VDD33\n",configured,powered,cycles,errors);
    return 0;
}
MSH_CMD_EXPORT(epd_power, SY7636 and AW9523B register diagnostics);
