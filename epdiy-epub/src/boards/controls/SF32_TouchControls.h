#pragma once

#include "TouchControls.h"
#include <rtthread.h>

class Renderer;

class SF32_TouchControls : public TouchControls
{
private:
  ActionCallback_t on_action;
  Renderer *renderer = nullptr;
  rt_device_t tp_device = nullptr;
  uint8_t ui_button_width = 120;
  uint8_t ui_button_height = 34;
  UIAction last_action = NONE;
  
  
public:
  static rt_err_t tp_rx_indicate(rt_device_t dev, rt_size_t size);
  SF32_TouchControls(Renderer *renderer, ActionCallback_t on_action);
  void render(Renderer *renderer) override;
  void renderPressedState(Renderer *renderer, UIAction action, bool state = true) override;
  void powerOnTouch() override;   
  void powerOffTouch() override;
};