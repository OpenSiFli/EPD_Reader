#pragma once
 
#include <functional>
 
typedef enum
{
  NONE,
  UP,
  DOWN,
  SELECT,
  LONG_SELECT,
  LAST_INTERACTION,
  MSG_DRAW_LOW_POWER_PAGE,   
  MSG_DRAW_CHARGE_PAGE,      
  MSG_DRAW_WELCOME_PAGE,
  MSG_UPDATE_CHARGE_STATUS     
} UIAction;
 
typedef std::function<void(UIAction)> ActionCallback_t;
 