#include "../SF32Paper.h"
#include "../SF32PaperRenderer.h"
#include "../controls/SF32_TouchControls.h"
#include <hourglass.h>
extern "C" {
#include "board527.h"
#include "font_ft.h"
extern const EpdFont regular_font;
}

/* Key wiring and battery ADC routing have not been verified on this board.
 * The reader remains fully touch-operated; do not initialize old-board pins. */
class NoButtons : public ButtonControls {
public:
    bool did_wake_from_deep_sleep() override {return false;}
    UIAction get_deep_sleep_action() override {return NONE;}
};
void SF32Paper::power_up()
{
    if(board527_init())rt_kprintf("BOARD527 peripheral initialization failed\n");
    if(epd_font_ft_init_builtin(32))rt_kprintf("BOARD527 built-in font initialization failed\n");
}
void SF32Paper::start_filesystem()
{
    if(board527_storage_mount())rt_kprintf("BOARD527 TF unavailable; insert a formatted card and run tf init\n");
}
void SF32Paper::stop_filesystem() {}
void SF32Paper::sleep_filesystem() { (void)board527_storage_idle(0); }
void SF32Paper::wakeup_filesystem() {}
void SF32Paper::prepare_to_sleep()
{
    board527_frontlight_set(0,0);board527_touch_power(0);board527_display_off();
}
Renderer *SF32Paper::get_renderer()
{
    return new SF32PaperRenderer(&regular_font,&regular_font,&regular_font,&regular_font,
                                 hourglass_data,hourglass_width,hourglass_height);
}
ButtonControls *SF32Paper::get_button_controls(rt_mq_t queue)
{
    (void)queue;return new NoButtons;
}
TouchControls *SF32Paper::get_touch_controls(Renderer *renderer,rt_mq_t queue)
{
    return new SF32_TouchControls(renderer,[queue](UIAction action){rt_mq_send(queue,&action,sizeof(action));});
}
