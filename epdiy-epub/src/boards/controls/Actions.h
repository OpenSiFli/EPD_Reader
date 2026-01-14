#pragma once

#include <functional>

typedef enum
{
  NONE,
  UP,
  DOWN,
  SELECT,
  UPGLIDE, // 长按触发的上滑操作，用于阅读页半屏操作覆盖
  LAST_INTERACTION,
  MSG_DRAW_LOW_POWER_PAGE,   
  MSG_DRAW_CHARGE_PAGE,      
  MSG_DRAW_WELCOME_PAGE,
  MSG_UPDATE_CHARGE_STATUS     
} UIAction;

typedef std::function<void(UIAction)> ActionCallback_t;
