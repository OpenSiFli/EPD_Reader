#include "reader_ui.h"
#include "reader_power.h"
#include "UIRegionsManager.h"
extern "C" {
#include "board527.h"
#include "font_ft.h"
#include "font_manager.h"
#include "bf0_hal.h"
}
extern void reader527_close_book_for_power_test();
extern rt_mq_t ui_queue;

/* Park only buses fed by GE14 and the disabled touch controller. No NOR,
 * PSRAM, UART, AW I2C2, or AW RESET pins are touched. */
static const uint8_t pins[]={0,1,2,4,5,6,7,8,9,10,11,24,25,28,29,32,37,39,40,41,42,43,44};
static uint32_t pads[sizeof(pins)],out[2],enable[2];
static int parked;
static void park_bus(void)
{
    GPIO1_TypeDef *gpio=(GPIO1_TypeDef *)hwp_gpio1;
    out[0]=gpio->DOR0;out[1]=gpio->DOR1;
    enable[0]=gpio->DOER0;enable[1]=gpio->DOER1;
    for(unsigned i=0;i<sizeof(pins);++i) {
        unsigned n=pins[i];
        volatile uint32_t *pad=&hwp_pinmux1->PAD_PA00+n;
        pads[i]=*pad;
        GPIO_InitTypeDef cfg={};cfg.Pin=n;cfg.Mode=GPIO_MODE_INPUT;cfg.Pull=GPIO_NOPULL;
        HAL_GPIO_Init(hwp_gpio1,&cfg);
        HAL_PIN_Set(PAD_PA00+n,(pin_function)(GPIO_A0+n),PIN_NOPULL,1);
        HAL_PIN_SetMode(PAD_PA00+n,1,PIN_ANALOG_INPUT);
    }
    parked=1;
}
static void restore_bus(void)
{
    if(!parked)return;
    for(unsigned i=0;i<sizeof(pins);++i) {
        unsigned n=pins[i],bank=n/32,bit=1u<<(n%32);
        HAL_GPIO_WritePin(hwp_gpio1,n,(out[bank]&bit)?GPIO_PIN_SET:GPIO_PIN_RESET);
        GPIO_InitTypeDef cfg={};cfg.Pin=n;cfg.Mode=(enable[bank]&bit)?GPIO_MODE_OUTPUT:GPIO_MODE_INPUT;cfg.Pull=GPIO_NOPULL;
        HAL_GPIO_Init(hwp_gpio1,&cfg);
        *(&hwp_pinmux1->PAD_PA00+n)=pads[i];
    }
    parked=0;
}
static void rail_card(Renderer *r,const char *stage,const char *rail,unsigned seconds)
{
    clear_areas();r->clear_screen();
    r->draw_text(20,80,"POWER RAIL TEST R30");
    r->draw_text(20,200,stage);r->draw_text(20,300,rail);
    r->draw_text(20,410,"Touch held in reset in ALL stages.");
    r->draw_text(20,490,"TF unmounted; built-in font only.");
    r->draw_text(20,610,"Unplug USB and COM36 to measure.");
    r->draw_text(20,690,"Ignore refresh; read after 10 sec.");
    char text[50];rt_snprintf(text,sizeof(text),"Stage: %u sec. Auto restore at end.",seconds);
    r->draw_text(20,790,text);r->flush_display();
}
static void rail_wait(unsigned seconds)
{
    uint32_t reads=tf_msd_read_count();
    reader_power_idle();rt_thread_mdelay(seconds*1000u);reader_power_busy();
    rt_kprintf("PWR_RAIL_WAIT seconds=%u tf_sectors=%u\n",seconds,tf_msd_read_count()-reads);
}
int reader527_rail_measure(Renderer *r,int seconds)
{
    reader527_close_book_for_power_test();r->cancel_page_preview();board527_power_auto();
    int font=font_manager_get_current(),size=epd_font_ft_get_size(),storage=0,rc=0,restore_rc=0;
    uint32_t ldo=hwp_pmuc->PERI_LDO;
    if(font_manager_select(0,32))return -1;
    if(board527_storage_test_begin()){font_manager_select(font,size);return -1;}
    storage=1;
    board527_frontlight_set(0,0);board527_touch_power(0);
    rail_card(r,"A - RAILS ON","GE14: on / LDO3: original",seconds);rail_wait(seconds);
    rail_card(r,"B - SHARED RAIL OFF","GE14/15: off / LDO3: original",seconds);
    park_bus();rc=board527_logic_rail_test(1);
    if(!rc)rail_wait(seconds);
    restore_rc=board527_logic_rail_test(0);rt_thread_mdelay(80);restore_bus();
    if(!rc&&!restore_rc) {
        rail_card(r,"C - RAIL OFF + LDO3 OFF","GE14/15: off / LDO3: off",seconds);
        park_bus();rc=board527_logic_rail_test(1);
        if(!rc) {
            HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO3_3V3,false,true);
            rail_wait(seconds);
        }
        // Restore only LDO3 bits, leaving unrelated PM bits untouched.
        const uint32_t mask=PMUC_PERI_LDO_EN_VDD33_LDO3|PMUC_PERI_LDO_VDD33_LDO3_PD;
        hwp_pmuc->PERI_LDO=(hwp_pmuc->PERI_LDO&~mask)|(ldo&mask);
        HAL_Delay_us(5000);
        restore_rc|=board527_logic_rail_test(0);rt_thread_mdelay(80);restore_bus();
    }
    if(!rc&&!restore_rc){rail_card(r,"D - RAILS RESTORED","GE14: on / LDO3: original",seconds);rail_wait(seconds);}
    if(storage)restore_rc|=board527_storage_test_end();
    // Reset touch after the shared pullups, buses and card are powered again.
    restore_rc|=board527_touch_test_sleep(0);
    if(!restore_rc)restore_rc|=font_manager_select(font,size);
    uint32_t msg;while(rt_mq_recv(ui_queue,&msg,sizeof(msg),0)==RT_EOK){}
    rt_kprintf("PWR_RAIL_DONE rc=%d restore=%d\n",rc,restore_rc);
    if(rc||restore_rc) {
        // Do not start another HV refresh if rail recovery failed.
        reader_power_busy();
        return -1;
    }
    return 0;
}
