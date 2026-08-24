#include "SF32_ButtonControls.h"
#include "SF32Paper.h"
#include "button.h"


static ActionCallback_t action_cbk ;

#ifdef SF32LB57X

/*
 * 57 板：1 个按键 + 2 个 ADC 按键。
 * 三个按键共用一个 GPIO 引脚（BSP_KEY1_PIN）作为唤醒中断，
 * 按下后通过 ADC channel（ADC_BUTTON_GROUP1_ADC_DEV_CHANNEL）采样电压区分具体按键。
 */
static void dummy_button_event_handler(int32_t pin, button_action_t action)
{
    // 57 的按键全部走 ADC 按键路径，此 GPIO 回调仅作占位，不会真正被调用。
    (void)pin;
    (void)action;
}

#ifdef USING_ADC_BUTTON
static void adc_button_handler(uint8_t group_idx, int32_t btn_idx, button_action_t button_action)
{
    (void)group_idx;

    if (button_action == BUTTON_CLICKED)
    {
        if (0 == btn_idx)        action_cbk(UIAction::UP);
        else if (1 == btn_idx)   action_cbk(UIAction::SELECT);
        else if (2 == btn_idx)   action_cbk(UIAction::DOWN);
    }
    else if (button_action == BUTTON_LONG_PRESSED)
    {
        if (0 == btn_idx)        action_cbk(UIAction::UPGLIDE);
    }
}
#endif /* USING_ADC_BUTTON */

#else /* !SF32LB57X */

void button_event_handler(int32_t pin, button_action_t action)
{
#if defined (BSP_USING_BOARD_SF32_OED_EPD_V11) || defined(BSP_USING_BOARD_SF32_OED_EPD_V12_SPI) || defined(BSP_USING_BOARD_SF32_OED_EPD_V12)
  {
      if (action == BUTTON_CLICKED)
      {
          action_cbk(UIAction::UP); 
      }
      else if (action == BUTTON_LONG_PRESSED)
      {
          rt_kprintf("长按   1");
          action_cbk(UIAction::UPGLIDE);
      }
  }
#else
  if (pin == EPD_KEY1)
  {
      if (action == BUTTON_CLICKED)
      {
          action_cbk(UIAction::DOWN); 
      }
  }

    else if (pin == EPD_KEY2)
    {
        if (action == BUTTON_CLICKED)
        {
            action_cbk(UIAction::SELECT); 
        }
        else if (action == BUTTON_LONG_PRESSED)
        {
            action_cbk(UIAction::UPGLIDE);
        }
    }
    else if (pin == EPD_KEY3)
    {
        if (action == BUTTON_CLICKED)
        {
            action_cbk(UIAction::UP); 
        }
    }
#endif
  
}

#ifdef EPD_KEY_GPADC
static void dummy_button_event_handler(int32_t pin, button_action_t action)
{
    // This function is a placeholder for buttons that do not require action handling.
    // It can be used to prevent assertion errors if no action is needed.
    (void)pin;
    (void)action;
}


#ifdef USING_ADC_BUTTON
static void adc_button_handler(uint8_t group_idx, int32_t pin, button_action_t button_action)
{
  rt_kprintf("adc_button_handler:%d,%d,%d\n", group_idx, pin, button_action);
  if (button_action == BUTTON_CLICKED)
  {
    if (0 == pin)        action_cbk(UIAction::SELECT);
    else if (1 == pin)   action_cbk(UIAction::DOWN);
  }
  else if (button_action == BUTTON_LONG_PRESSED)
  {
    if (0 == pin)        action_cbk(UIAction::UPGLIDE);
  }
}

#endif /* USING_ADC_BUTTON */
#endif /* EPD_KEY_GPADC */

#endif /* !SF32LB57X */

SF32_ButtonControls::SF32_ButtonControls(
    ActionCallback_t on_action)
    : on_action(on_action)
{
  int32_t id;
  button_cfg_t cfg;

#ifdef SF32LB57X
  cfg.pin = BSP_KEY1_PIN;

#ifdef BSP_KEY1_ACTIVE_HIGH
  cfg.active_state = BUTTON_ACTIVE_HIGH;
#else
  cfg.active_state = BUTTON_ACTIVE_LOW;
#endif

  cfg.mode = PIN_MODE_INPUT;
  cfg.button_handler = dummy_button_event_handler;
  id = button_init(&cfg);
  RT_ASSERT(id >= 0);

#ifdef USING_ADC_BUTTON
  {
    adc_button_handler_t handlers[ADC_BUTTON_GROUP1_MAX_NUM];
    for (uint8_t i = 0; i < ADC_BUTTON_GROUP1_MAX_NUM; i++)
    {
      handlers[i] = adc_button_handler;
    }
    rt_err_t err = button_bind_adc_button(id, 0, ADC_BUTTON_GROUP1_MAX_NUM, handlers);
    RT_ASSERT(0 == err);
  }
#endif /* USING_ADC_BUTTON */

  if (SF_EOK != button_enable(id))
  {
      RT_ASSERT(0);
  }
#else /* !SF32LB57X */
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
 
#if !defined(BSP_USING_BOARD_SF32_OED_EPD_V12_SPI) && !defined(BSP_USING_BOARD_SF32_OED_EPD_V11) && !defined(BSP_USING_BOARD_SF32_OED_EPD_V12)
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
#endif /*!BSP_USING_BOARD_SF32_OED_EPD_V11*/
#ifdef EPD_KEY_GPADC
  cfg.pin = EPD_KEY_GPADC;
  cfg.active_state = BUTTON_ACTIVE_HIGH;
  cfg.mode = PIN_MODE_INPUT;
  cfg.button_handler = dummy_button_event_handler;
  id = button_init(&cfg);
  RT_ASSERT(id >= 0);
#ifdef USING_ADC_BUTTON
    adc_button_handler_t handler[2] = {adc_button_handler, adc_button_handler};
    rt_err_t err = button_bind_adc_button(id, 0, sizeof(handler)/sizeof(handler[0]), &handler[0]);
    RT_ASSERT(0 == err);
#endif
  if (SF_EOK != button_enable(id))
  {
      RT_ASSERT(0);
  }
#endif
#endif /* !SF32LB57X */

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
