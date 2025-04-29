#pragma once

#include "Actions.h"
#include "ButtonControls.h"
#include <rtthread.h>
#include <rtdevice.h>
#include "button.h"

class SF32_ButtonControls : public ButtonControls
{
private:

  ActionCallback_t on_action;
public:
  SF32_ButtonControls(
  ActionCallback_t on_action);
  bool did_wake_from_deep_sleep();
  UIAction get_deep_sleep_action();
  // void setup_deep_sleep();
};

