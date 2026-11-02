#include "reader_ui.h"
#include "UIRegionsManager.h"
#include "epub_screen.h"
#include "type.h"
#include "reading_settings.h"
#include <string>
#include <algorithm>
extern "C" {
#include "board527.h"
#include "board.h"
#include "drv_flash.h"
#include "../../../dfu527/common/reader_dfu.h"
}
extern int settings_selected_idx;
volatile int reader527_light_hit=-1;
volatile int reader527_dfu_hit=-1;

static std::string fit(Renderer *r,const char *text,int width)
{
    std::string value=text;
    if(r->get_text_width(value.c_str())<=width)return value;
    while(!value.empty()) {
        size_t pos=value.size()-1;
        while(pos && (static_cast<unsigned char>(value[pos])&0xc0)==0x80)--pos;
        value.resize(pos);
        std::string shorter=value+"...";
        if(r->get_text_width(shorter.c_str())<=width)return shorter;
    }
    return "";
}
static void label(Renderer *r,int x,int y,int w,int h,const char *text)
{
    std::string value=fit(r,text,w-16);
    r->draw_text(x+(w-r->get_text_width(value.c_str()))/2,
                 y+(h-r->get_line_height())/2,value.c_str());
}
static void button(Renderer *r,int index,int x,int y,int w,int h,const char *text,bool selected=false)
{
    for(int n=0;n<(selected?3:1);++n)r->draw_rect(x+n,y+n,w-2*n,h-2*n,0);
    label(r,x,y,w,h,text);static_add_area(x,y,w,h,index);
}
void reader527_home(Renderer *r,int selected)
{
    clear_areas();r->clear_screen();int w=r->get_page_width(),h=r->get_page_height();
    label(r,0,60,w,80,"SIFLI READER");
    label(r,0,150,w,50,"书籍 · 字体 · 冷暖前光");
    const char *labels[]={"打开书库","功能设置","查看天气","继续阅读"};
    int gap=24,side=24,cw=(w-2*side-gap)/2,ch=std::min(210,(h-360-gap)/2);
    int top=260;
    for(int i=0;i<4;++i)button(r,i,side+(i%2)*(cw+gap),top+(i/2)*(ch+gap),cw,ch,labels[i],i==selected);
    label(r,20,h-120,w-40,64,"轻点进入 · 阅读时上滑打开菜单");
}
void reader527_settings(Renderer *r)
{
    clear_areas();r->clear_screen();int w=r->get_page_width(),h=r->get_page_height();
    label(r,0,20,w,64,"功能设置");
    int side=16,arrow=56,gap=10,top=112,row=std::min(100,(h-290-8*gap)/9);
    int body_x=side+arrow,body_w=w-2*(side+arrow);
    char texts[9][96];
    rt_snprintf(texts[0],sizeof(texts[0]),"触控：已启用");
    int minutes=screen_get_timeout_shutdown_minutes();
    if(minutes)rt_snprintf(texts[1],sizeof(texts[1]),"待机：%d 分钟",minutes);
    else rt_snprintf(texts[1],sizeof(texts[1]),"待机：不自动待机");
    int period=screen_get_full_refresh_period();
    if(period)rt_snprintf(texts[2],sizeof(texts[2]),"全刷：每 %d 次",period);
    else rt_snprintf(texts[2],sizeof(texts[2]),"全刷：每次");
    rt_snprintf(texts[3],sizeof(texts[3]),"蓝牙：%s",bluetooth_ui_enabled?"开":"关");
    rt_snprintf(texts[4],sizeof(texts[4]),"阅读设置");
    unsigned cold=0,warm=0;board527_frontlight_get(&cold,&warm);
    rt_snprintf(texts[5],sizeof(texts[5]),"前光：冷 %u%% / 暖 %u%%",cold,warm);
    rt_snprintf(texts[6],sizeof(texts[6]),"USB 固件升级");
    unsigned seconds=reading_auto_turn_seconds();
    if(seconds)rt_snprintf(texts[7],sizeof(texts[7]),"自动翻页：%u 秒",seconds);
    else rt_snprintf(texts[7],sizeof(texts[7]),"自动翻页：关闭");
    rt_snprintf(texts[8],sizeof(texts[8]),"时钟待机 · 按电源键唤醒");
    for(int i=0;i<9;++i) {
        int y=top+i*(row+gap);
        button(r,i*3+2,body_x,y,body_w,row,texts[i],settings_selected_idx==i);
        if((i>=SET_TIMEOUT&&i<=SET_BLUETOOTH)||i==SET_AUTO_TURN) {
            button(r,i*3,side,y,arrow-8,row,"-");
            button(r,i*3+1,w-side-arrow+8,y,arrow-8,row,"+");
        }
    }
    button(r,SET_CONFIRM*3+2,side,h-120,w-2*side,88,"返回主页",settings_selected_idx==SET_CONFIRM);
}
bool reader527_frontlight(Renderer *r,UIAction action)
{
    unsigned cold=0,warm=0;board527_frontlight_get(&cold,&warm);
    if(action==SELECT) {
        int hit=reader527_light_hit;reader527_light_hit=-1;
        if(hit==5)return false;
        if(hit==0)cold=cold>=10?cold-10:0;
        if(hit==1)cold=std::min(100u,cold+10);
        if(hit==2)warm=warm>=10?warm-10:0;
        if(hit==3)warm=std::min(100u,warm+10);
        if(hit==4)cold=warm=0;
        if(board527_frontlight_set(cold,warm))rt_kprintf("UI_FRONTLIGHT_ERROR\n");
        board527_frontlight_get(&cold,&warm);
    }
    clear_areas();r->clear_screen();int w=r->get_page_width(),h=r->get_page_height();
    label(r,0,30,w,70,"冷暖前光");
    label(r,0,115,w,60,"两路可独立调节");
    const unsigned levels[]={cold,warm};
    for(int i=0;i<2;++i) {
        int top=260+i*270;char value[64];
        rt_snprintf(value,sizeof(value),"%s  %u%%",i?"暖光":"冷光",levels[i]);
        label(r,90,top,w-180,90,value);
        button(r,i*2,24,top,78,90,"-");
        button(r,i*2+1,w-102,top,78,90,"+");
        r->draw_rect(30,top+125,w-60,22,0);
        if(levels[i])r->fill_rect(34,top+129,(w-68)*levels[i]/100,14,0);
    }
    int gap=20,cw=(w-48-gap)/2;
    button(r,4,24,h-145,cw,96,"关闭前光");
    button(r,5,24+cw+gap,h-145,cw,96,"返回设置");
    return true;
}
bool reader527_dfu_page(Renderer *r,UIAction action)
{
    static bool failed=false;
    if(action==NONE)failed=false;
    if(action==SELECT) {
        int hit=reader527_dfu_hit;reader527_dfu_hit=-1;
        if(hit==1)return false;
        if(hit==0) {
            uint32_t vectors[2];
            if(rt_flash_read(R527_LOADER_BASE,(uint8_t *)vectors,8)!=8||
               vectors[0]<0x20000000u||vectors[0]>0x2007fc00u||
               !(vectors[1]&1)||vectors[1]<R527_LOADER_BASE||
               vectors[1]>=R527_LOADER_BASE+R527_LOADER_SIZE)failed=true;
            else {
                r->clear_screen();label(r,20,260,r->get_page_width()-40,90,"正在进入 USB 升级模式");
                label(r,20,380,r->get_page_width()-40,90,"连接电脑，运行配套升级工具");
                r->flush_display();board527_frontlight_set(0,0);board527_touch_power(0);
                board527_display_off();HAL_Set_backup(RTC_BAKCUP_OTA_FORCE_MODE,R527_COOKIE);
                rt_thread_mdelay(100);drv_reboot();
            }
        }
    }
    clear_areas();r->clear_screen();int w=r->get_page_width(),h=r->get_page_height();
    label(r,0,60,w,90,"USB 固件升级");
    label(r,20,250,w-40,70,"进入后暂时退出阅读器");
    label(r,20,340,w-40,70,"使用 USB 数据线连接电脑");
    label(r,20,430,w-40,70,"升级完成后自动返回阅读器");
    if(failed)label(r,20,550,w-40,70,"未安装恢复程序，请先首次安装");
    int cw=(w-68)/2;
    button(r,0,24,h-165,cw,100,"进入升级");
    button(r,1,44+cw,h-165,cw,100,"返回设置");
    return true;
}
