/* SPDX-License-Identifier: Apache-2.0
 * Original COM34 main image 0x1216165c: LM3630A / i2c2 / 0x36,
 * AW9523B GE3 + GE11 high, GE1 input. Original brightness cap=150.
 * Register definitions: TI LM3630A SNVS974B (Rev B), sections 7.5/7.6.
 */
#include <rtthread.h>
#include <rtdevice.h>
#include <finsh.h>
#include <stdlib.h>
#include <string.h>
#include "epd_tps.h"
#include "frontlight.h"

#define LM_ADDR 0x36
#define MAX_CODE 150u
static struct rt_i2c_bus_device *bus;
static struct rt_mutex lock;
static struct rt_semaphore monitor_sem;
static unsigned started,ready,level_a,level_b,sets,errors,checks,generation;
static uint8_t revision,irq_latched,fault_latched,startup_irq;

static int read_reg(uint8_t reg,uint8_t *value)
{
    return rt_i2c_mem_read(bus,LM_ADDR,reg,8,value,1)==1?0:-1;
}
static int write_check(uint8_t reg,uint8_t value)
{
    uint8_t got=0;
    if(rt_i2c_mem_write(bus,LM_ADDR,reg,8,&value,1)!=1 || read_reg(reg,&got) || got!=value) {
        rt_kprintf("FL_REG_FAIL reg=%02x want=%02x got=%02x\n",reg,value,got);
        return -1;
    }
    return 0;
}
static int hard_off(void)
{
    ready=0;level_a=level_b=0;
    return board_frontlight_enable(0);
}
static int fail_off(const char *why)
{
    ++errors;
    int rc=hard_off();
    rt_kprintf("FL_FAIL %s shutdown_rc=%d\n",why,rc);
    return -RT_ERROR;
}
static int check_faults(int verbose)
{
    uint8_t irq=0,fault=0;
    if(read_reg(9,&irq)||read_reg(11,&fault))return fail_off("read_status");
    ++checks;irq_latched|=irq&7;fault_latched|=fault&0x3a;
    if(verbose || (irq&7) || (fault&0x3a))
        rt_kprintf("FL_CHECK irq=%02x fault=%02x checks=%u\n",irq,fault,checks);
    if((irq&7)||(fault&0x3a))return fail_off("hardware_fault");
    return 0;
}
static int check_startup(void)
{
    /* COM36 bounded capture: first IRQ=06, then two reads 20ms apart=00.
     * Only the initial <=5% ramp can classify OCP/OVP as startup history.
     * TSD/manufacturing faults or any recurring event still shut down. */
    uint8_t irq=0,fault=0;
    if(read_reg(9,&irq)||read_reg(11,&fault))return fail_off("startup_read");
    ++checks;startup_irq|=irq;
    rt_kprintf("FL_STARTUP_RAMP irq=%02x fault=%02x\n",irq,fault);
    if((irq&1)||(fault&0x3a)) {
        irq_latched|=irq&7;fault_latched|=fault&0x3a;
        return fail_off("startup_fault");
    }
    for(unsigned n=0;n<2;++n) {
        rt_thread_mdelay(20);
        if(check_faults(1))return -RT_ERROR;
    }
    return 0;
}
static int prepare(void)
{
    uint8_t control=0;
    if(ready && generation==board_expander_generation() && rt_pin_read(3))return 0;
    if(hard_off())return fail_off("expander_off");
    if(!bus) {
        bus=rt_i2c_bus_device_find("i2c2");
        if(!bus || rt_device_open((rt_device_t)bus,RT_DEVICE_OFLAG_RDWR)!=RT_EOK) {
            bus=RT_NULL;return fail_off("open_i2c2");
        }
        /* Shared bus is already configured by AW9523B; do not reconfigure it. */
    }
    rt_thread_mdelay(5);
    if(board_frontlight_enable(1))return fail_off("enable");
    rt_thread_mdelay(5);
    if(read_reg(0x1f,&revision)||read_reg(0,&control))return fail_off("identify");
    rt_kprintf("FL_IDENTIFY i2c2 addr=36 revision=%02x control=%02x\n",revision,control);
    if(revision!=2 || (control&0xc0)!=0xc0)return fail_off("unexpected_device");
    /* Configure while asleep. No LED bank is fed back until it is enabled,
     * so waking for brightness writes cannot run an unloaded boost loop. */
    if(write_check(1,0)||write_check(2,0x38)||
       write_check(5,0x1f)||write_check(6,0x1f)||
       write_check(7,0x11)||write_check(8,0x11)||write_check(0x50,3))
        return fail_off("sleep_configure");
    uint8_t zero=0;
    if(rt_i2c_mem_write(bus,LM_ADDR,0,8,&zero,1)!=1)return fail_off("wake");
    rt_thread_mdelay(5);
    if(read_reg(0,&control)||control)return fail_off("wake_status");
    if(write_check(3,0)||write_check(4,0)||write_check(10,0x87))
        return fail_off("configure");
    generation=board_expander_generation();ready=1;
    uint8_t irq=0,fault=0;
    if(read_reg(9,&irq)||read_reg(11,&fault))return fail_off("startup_status");
    startup_irq|=irq;
    rt_kprintf("FL_STARTUP no_LED_load irq=%02x fault=%02x\n",irq,fault);
    if((irq&5)||(fault&0x3a))return fail_off("startup_fault");
    rt_kprintf("FL_READY LM3630A GE3+GE11 max_code=150 exponential same_as_original\n");
    return 0;
}
static int off(void)
{
    int rc=0;
    if(ready) {
        rc=write_check(3,0)|write_check(4,0);
        rt_thread_mdelay(300); /* Original down-ramp code 1: 261ms. */
    }
    if(hard_off())rc=-1;
    if(rc)++errors;
    rt_kprintf("FL_OFF rc=%d\n",rc);
    return rc;
}
static unsigned brightness(unsigned percent)
{
    unsigned v=(percent*MAX_CODE+50)/100;
    return percent && v<4 ? 4 : v;
}
static int apply_levels(unsigned a,unsigned b)
{
    /* Both codes are programmed before enabling the corresponding banks.
     * Disabled banks always receive zero. LED2 remains mapped to Bank B. */
    unsigned ac=brightness(a),bc=brightness(b);
    uint8_t enable=(a?4:0)|(b?2:0);
    uint8_t feedback=(a?8:0)|(b?16:0);
    int starting=!level_a&&!level_b;
    if(starting) {
        /* Read-to-clear pending no-load startup OVP before the bounded
         * low-brightness start. ALL active-mode faults are checked below. */
        uint8_t pending=0;
        if(read_reg(9,&pending))return fail_off("clear_startup");
        startup_irq|=pending;
        rt_kprintf("FL_BEFORE_ENABLE irq=%02x\n",pending);
        if(pending&5)return fail_off("startup_ocp_tsd");
    }
    if(write_check(3,ac)||write_check(4,bc)||write_check(0,enable)||write_check(1,feedback))
        return fail_off("set_level");
    level_a=a;level_b=b;++sets;
    rt_thread_mdelay(600); /* Original startup code 2: 522ms. */
    rt_kprintf("FL_SET A=%u B=%u codeA=%u codeB=%u ctrl=%02x fb=%02x\n",a,b,ac,bc,enable,feedback);
    return starting && a<=5 && b<=5 ? check_startup() : check_faults(1);
}
static int set_levels(unsigned a,unsigned b)
{
    if(a>100||b>100)return -RT_EINVAL;
    if(!a&&!b)return off();
    if((level_a||level_b) && ((!!a!=!!level_a)||(!!b!=!!level_b))) {
        if(off())return -RT_ERROR;
    }
    if(prepare())return -RT_ERROR;
    if(!level_a&&!level_b && (a>5||b>5)) {
        if(apply_levels(a?5:0,b?5:0))return -RT_ERROR;
    }
    return apply_levels(a,b);
}
static int status(void)
{
    rt_kprintf("FL_STATUS ready=%u revision=%02x A=%u B=%u sets=%u errors=%u checks=%u\n",
        ready,revision,level_a,level_b,sets,errors,checks);
    rt_kprintf("FL_FAULT_LATCH irq=%02x fault=%02x generation=%u\n",irq_latched,fault_latched,generation);
    rt_kprintf("FL_STARTUP_HISTORY irq=%02x\n",startup_irq);
    int rc=board_frontlight_gpio_report();
    if(ready) {
        rt_kprintf("FL_REGS");
        for(unsigned r=0;r<=8;++r) {
            uint8_t v=0;
            if(read_reg(r,&v)){rt_kprintf("\n");return fail_off("dump_regs");}
            rt_kprintf(" %02x:%02x",r,v);
        }
        rt_kprintf("\n");
        if((level_a||level_b) && check_faults(1))rc=-1;
    }
    return rc;
}
static void monitor(void *arg)
{
    (void)arg;
    while(1) {
        rt_sem_take(&monitor_sem,(level_a||level_b)?rt_tick_from_millisecond(500):RT_WAITING_FOREVER);
        rt_mutex_take(&lock,RT_WAITING_FOREVER);
        if(ready) {
            if(!rt_pin_read(3)||generation!=board_expander_generation())fail_off("expander_reset");
            else if(level_a||level_b)check_faults(0);
        }
        rt_mutex_release(&lock);
    }
}
static int parse_percent(const char *s,unsigned *v)
{
    char *end;unsigned long n;
    if(!s[0]||s[0]=='-')return -1;
    n=strtoul(s,&end,10);
    if(*end||n>100)return -1;
    *v=(unsigned)n;return 0;
}
static int fl(int argc,char **argv)
{
    int rc=-RT_EINVAL;unsigned a=0,b=0,p=0;
    if(!started)return -RT_ENOSYS;
    rt_mutex_take(&lock,RT_WAITING_FOREVER);
    if(argc==2 && !strcmp(argv[1],"status"))rc=status();
    else if(argc==2 && !strcmp(argv[1],"probe"))rc=prepare();
    else if(argc==2 && !strcmp(argv[1],"off"))rc=off();
    else if(argc==4 && !strcmp(argv[1],"set") && !parse_percent(argv[2],&a) && !parse_percent(argv[3],&b))rc=set_levels(a,b);
    else if(argc==3 && !parse_percent(argv[2],&p)) {
        if(!strcmp(argv[1],"a"))rc=set_levels(p,0);
        else if(!strcmp(argv[1],"b"))rc=set_levels(0,p);
        else if(!strcmp(argv[1],"both"))rc=set_levels(p,p);
    }
    if(rc==-RT_EINVAL)rt_kprintf("fl probe | status | off | a/b/both <0..100> | set <A%%> <B%%>\n");
    rt_sem_release(&monitor_sem);
    rt_mutex_release(&lock);return rc;
}
MSH_CMD_EXPORT(fl, LM3630A frontlight control);
int frontlight_start(void)
{
    rt_mutex_init(&lock,"fllock",RT_IPC_FLAG_PRIO);
    rt_sem_init(&monitor_sem,"flmon",0,RT_IPC_FLAG_FIFO);
    if(hard_off())return -RT_ERROR;
    rt_thread_t tid=rt_thread_create("flcheck",monitor,RT_NULL,2048,19,10);
    if(!tid)return -RT_ENOMEM;
    started=1;rt_thread_startup(tid);
    rt_kprintf("FRONTLIGHT_READY commands_available=1 light=OFF (device identified by fl probe)\n");
    return 0;
}
void frontlight_display_lock(void){if(started)rt_mutex_take(&lock,RT_WAITING_FOREVER);}
void frontlight_display_unlock(void){if(started)rt_mutex_release(&lock);}

int board527_frontlight_set(unsigned cold,unsigned warm)
{
    if(!started)return -RT_ENOSYS;
    rt_mutex_take(&lock,RT_WAITING_FOREVER);int rc=set_levels(cold,warm);rt_sem_release(&monitor_sem);rt_mutex_release(&lock);return rc;
}
void board527_frontlight_get(unsigned *cold,unsigned *warm)
{
    rt_mutex_take(&lock,RT_WAITING_FOREVER);
    if(cold)*cold=level_a;if(warm)*warm=level_b;
    rt_mutex_release(&lock);
}
