#include "reader_ui.h"
#include "standby_clock.h"
#include "UIRegionsManager.h"
#include "type.h"
#include "reading_settings.h"
#include "EpubList/EpubReader.h"
#include <cstring>
extern "C" {
#include <finsh.h>
#include "board527.h"
}
extern rt_mq_t ui_queue;
extern Renderer *renderer;
extern AppUIState ui_state;
extern int settings_selected_idx;
extern EpubListState epub_list_state;
extern void handleUserInteraction(Renderer *,UIAction,bool);
extern void back_to_main_page();
extern void reader527_reading_settings_open(Renderer *);
extern int reader527_power_measure(Renderer *,int);
extern int reader527_lcpu_measure(Renderer *,int);
extern int reader527_rail_measure(Renderer *,int);
extern int reader527_amp_measure(Renderer *,int);
extern int reader527_minimum_power(Renderer *,bool);
extern int reader527_power_steps(Renderer *,unsigned);
extern int reader527_touch_states(Renderer *,unsigned);
enum {DEBUG_HOME=0x52710000,DEBUG_LIBRARY,DEBUG_SETTINGS,DEBUG_LIGHT,DEBUG_READING,DEBUG_DUMP,DEBUG_DFU,DEBUG_DFU_ENTER,DEBUG_POWER,DEBUG_POWER_QUICK,DEBUG_LCPU,DEBUG_LCPU_QUICK,DEBUG_RAIL,DEBUG_RAIL_QUICK,DEBUG_AMP,DEBUG_AMP_QUICK,DEBUG_MINIMUM,DEBUG_MINIMUM_TEST,DEBUG_STEPS,DEBUG_STEPS_QUICK,DEBUG_TOUCH_STATES,DEBUG_TOUCH_STATES_QUICK,DEBUG_AUTO_OFF,DEBUG_AUTO_10,DEBUG_AUTO_15,DEBUG_AUTO_20,DEBUG_AUTO_FOCUS,DEBUG_STANDBY,DEBUG_STANDBY_TEST,DEBUG_CLOCK_PREVIEW,DEBUG_CLOCK_RAM,DEBUG_CLOCK_RAM_TEST};
static void dump_frame(void)
{
    const uint8_t *frame=board527_framebuffer();const unsigned size=1216*684/2;
    const char hex[]="0123456789abcdef";uint32_t hash=2166136261u;
    rt_kprintf("FB_BEGIN bytes=%u width=1216 height=684 rotation=3\n",size);
    for(unsigned at=0;at<size;at+=32) {
        char line[65];unsigned n=size-at;if(n>32)n=32;
        for(unsigned i=0;i<n;++i) {
            uint8_t v=frame[at+i];line[i*2]=hex[v>>4];line[i*2+1]=hex[v&15];hash=(hash^v)*16777619u;
        }
        line[n*2]=0;rt_kprintf("FB:%06x %s\n",at,line);
    }
    rt_kprintf("FB_END fnv=%08x\n",hash);
}
bool reader527_debug_message(uint32_t msg,Renderer *r)
{
    if(msg<DEBUG_HOME||msg>DEBUG_CLOCK_RAM_TEST)return false;
    if(msg==DEBUG_CLOCK_RAM||msg==DEBUG_CLOCK_RAM_TEST){reader527_standby_ram(r,msg==DEBUG_CLOCK_RAM_TEST);reader527_standby_ram_take_restored();return true;}
    if(msg==DEBUG_CLOCK_PREVIEW){ui_state=SETTINGS_PAGE;reader527_standby_preview(r);return true;}
    if(msg==DEBUG_STANDBY||msg==DEBUG_STANDBY_TEST) {
        int rc=reader527_standby_enter(r,msg==DEBUG_STANDBY_TEST);
        rt_kprintf("CLOCK_ENTRY_RESULT rc=%d\n",rc);return true;
    }
    if(msg>=DEBUG_AUTO_OFF&&msg<=DEBUG_AUTO_20) {
        const unsigned seconds[]={0,10,15,20};
        bool ok=reading_auto_turn_set(seconds[msg-DEBUG_AUTO_OFF]);
        rt_kprintf("AUTO_TURN_DEBUG seconds=%u rc=%d\n",reading_auto_turn_seconds(),ok?0:-1);return true;
    }
    if(msg==DEBUG_AUTO_FOCUS) {
        settings_selected_idx=SET_AUTO_TURN;ui_state=SETTINGS_PAGE;
        handleUserInteraction(r,NONE,true);r->flush_display();
        rt_kprintf("AUTO_TURN_FOCUS seconds=%u\n",reading_auto_turn_seconds());return true;
    }
    if(msg==DEBUG_TOUCH_STATES||msg==DEBUG_TOUCH_STATES_QUICK) {
        reader527_touch_states(r,msg==DEBUG_TOUCH_STATES?120:5);return true;
    }
    if(msg==DEBUG_STEPS||msg==DEBUG_STEPS_QUICK) {
        reader527_power_steps(r,msg==DEBUG_STEPS?120:5);return true;
    }
    if(msg==DEBUG_MINIMUM||msg==DEBUG_MINIMUM_TEST) {
        reader527_minimum_power(r,msg==DEBUG_MINIMUM_TEST);return true;
    }
    if(msg==DEBUG_AMP||msg==DEBUG_AMP_QUICK) {
        if(reader527_amp_measure(r,msg==DEBUG_AMP?120:5))return true;
        ui_state=MAIN_PAGE;handleUserInteraction(r,NONE,true);r->flush_display();
        rt_kprintf("UI_DEBUG_DONE state=%u\n",(unsigned)ui_state);return true;
    }
    if(msg==DEBUG_RAIL||msg==DEBUG_RAIL_QUICK) {
        if(reader527_rail_measure(r,msg==DEBUG_RAIL?120:5))return true;
        ui_state=MAIN_PAGE;handleUserInteraction(r,NONE,true);r->flush_display();
        rt_kprintf("UI_DEBUG_DONE state=%u\n",(unsigned)ui_state);return true;
    }
    if(msg==DEBUG_LCPU||msg==DEBUG_LCPU_QUICK) {
        if(reader527_lcpu_measure(r,msg==DEBUG_LCPU?120:5))return true;
        ui_state=MAIN_PAGE;handleUserInteraction(r,NONE,true);r->flush_display();
        rt_kprintf("UI_DEBUG_DONE state=%u\n",(unsigned)ui_state);return true;
    }
    if(msg==DEBUG_POWER||msg==DEBUG_POWER_QUICK) {
        if(reader527_power_measure(r,msg==DEBUG_POWER?120:5))return true;
        ui_state=MAIN_PAGE;handleUserInteraction(r,NONE,true);r->flush_display();
        rt_kprintf("UI_DEBUG_DONE state=%u\n",(unsigned)ui_state);return true;
    }
    if(msg==DEBUG_DUMP){dump_frame();return true;}
    if(msg==DEBUG_DFU_ENTER) {
        if(ui_state==USB_DFU_PAGE){reader527_dfu_hit=0;handleUserInteraction(r,SELECT,false);}
        return true;
    }
    r->cancel_page_preview();
    if(msg==DEBUG_HOME){ui_state=MAIN_PAGE;}
    else if(msg==DEBUG_LIBRARY)ui_state=SELECTING_EPUB;
    else if(msg==DEBUG_SETTINGS)ui_state=SETTINGS_PAGE;
    else if(msg==DEBUG_LIGHT)ui_state=FRONTLIGHT_PAGE;
    else if(msg==DEBUG_READING){reader527_reading_settings_open(r);}
    else if(msg==DEBUG_DFU)ui_state=USB_DFU_PAGE;
    handleUserInteraction(r,NONE,true);r->flush_display();
    rt_kprintf("UI_DEBUG_DONE state=%u\n",(unsigned)ui_state);return true;
}
static int ui(int argc,char **argv)
{
    if(!ui_queue||!renderer)return -RT_ENOSYS;
    if(argc==2&&!strcmp(argv[1],"status")) {
        if(epub_list_state.num_epubs&&epub_list_state.selected_item<epub_list_state.num_epubs) {
            const EpubListItem &book=epub_list_state.epub_list[epub_list_state.selected_item];
            rt_kprintf("UI_BOOK section=%u page=%u pages=%u\n",book.current_section,book.current_page,book.pages_in_current_section);
        }
        rt_kprintf("AUTO_TURN_STATUS seconds=%u\n",reading_auto_turn_seconds());
        rt_kprintf("UI_STATUS state=%u width=684 height=1216 page=%d,%d margins=%d,%d\n",
            (unsigned)ui_state,renderer->get_page_width(),renderer->get_page_height(),renderer->get_margin_left(),renderer->get_margin_top());
        for(unsigned n=0;n<MAX_AREAS;++n) {
            AreaRect &a=g_area_array[n];
            if(a.end_x>a.start_x&&a.end_y>a.start_y)
                rt_kprintf("UI_AREA %u %d,%d..%d,%d\n",n,a.start_x,a.start_y,a.end_x,a.end_y);
        }
        return 0;
    }
    if(argc==2) {
        const char *names[]={"home","library","settings","light","reading","dump","dfu","dfu-enter","power","power-quick","lp-power","lp-power-quick","rail-power","rail-power-quick","amp-power","amp-power-quick","min-power","min-power-test","power-steps","power-steps-quick","touch-power","touch-power-quick","auto-off","auto-10","auto-15","auto-20","auto-settings","standby","standby-test","clock-preview","clock-ram","clock-ram-test"};
        for(unsigned n=0;n<sizeof(names)/sizeof(names[0]);++n)if(!strcmp(argv[1],names[n])) {
            uint32_t msg=DEBUG_HOME+n;return rt_mq_send(ui_queue,&msg,sizeof(msg));
        }
        const char *actions[]={"prev","next","select","menu"};
        const UIAction values[]={UP,DOWN,SELECT,UPGLIDE};
        for(unsigned n=0;n<4;++n)if(!strcmp(argv[1],actions[n])) {
            uint32_t msg=values[n];return rt_mq_send(ui_queue,&msg,sizeof(msg));
        }
    }
    rt_kprintf("ui home/library/settings/light/reading/dump/dfu/dfu-enter/power/power-quick/lp-power/lp-power-quick/rail-power/rail-power-quick/amp-power/amp-power-quick/min-power/min-power-test/power-steps/power-steps-quick/touch-power/touch-power-quick/auto-off/auto-10/auto-15/auto-20/auto-settings/standby/standby-test/clock-preview/clock-ram/clock-ram-test/status/prev/next/select/menu\n");return -RT_EINVAL;
}
MSH_CMD_EXPORT(ui, Reader UI diagnostics);
