#include "SF32_ButtonControls.h"
#include "SF32Paper.h"
#include "button.h"


static ActionCallback_t action_cbk ;

void button_event_handler(int32_t pin, button_action_t action)
{
   
  if (pin == EPD_KEY2)
  {
      if (action == BUTTON_CLICKED)
      {
          action_cbk(UIAction::SELECT); 
      }
  }
  else if (pin == EPD_KEY1)
  {
      if (action == BUTTON_CLICKED)
      {
          action_cbk(UIAction::DOWN); 
      }
  }
  else if (pin == EPD_KEY3)
  {
      if (action == BUTTON_CLICKED)
      {
          action_cbk(UIAction::UP); 
      }
  }
}


SF32_ButtonControls::SF32_ButtonControls(
    ActionCallback_t on_action)
    : on_action(on_action)
{
  int32_t id;
  button_cfg_t cfg;
  cfg.pin = EPD_KEY1;

  cfg.active_state = BUTTON_ACTIVE_HIGH;
  cfg.mode = PIN_MODE_INPUT;
  cfg.button_handler = button_event_handler;
  id = button_init(&cfg);
  RT_ASSERT(id >= 0);
  if (SF_EOK != button_enable(id))
  {
      RT_ASSERT(0);
  }
 
  cfg.pin = EPD_KEY2;
  cfg.active_state = BUTTON_ACTIVE_HIGH;
  cfg.mode = PIN_MODE_INPUT;
  cfg.button_handler = button_event_handler;
  id = button_init(&cfg);
  RT_ASSERT(id >= 0);
  if (SF_EOK != button_enable(id))
  {
      RT_ASSERT(0);
  }

  cfg.pin = EPD_KEY3;
  cfg.active_state = BUTTON_ACTIVE_HIGH;
  cfg.mode = PIN_MODE_INPUT;
  cfg.button_handler = button_event_handler;
  id = button_init(&cfg);
  RT_ASSERT(id >= 0);
  if (SF_EOK != button_enable(id))
  {
      RT_ASSERT(0);
  }

  action_cbk = on_action;
}


bool SF32_ButtonControls::did_wake_from_deep_sleep()
{

  return false;
}

UIAction SF32_ButtonControls::get_deep_sleep_action()
{
  return UIAction::NONE;
}
// void SF32_ButtonControls::setup_deep_sleep()
// {

// }