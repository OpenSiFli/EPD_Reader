#ifndef TYPES_H
#define TYPES_H
#include <rtconfig.h>

// 设置页列表项
typedef enum { 
  SET_TOUCH = 0, 
  SET_TIMEOUT = 1, 
  SET_FULL_REFRESH = 2, 
  SET_BLUETOOTH = 3,
  SET_READING_SETTINGS = 4,  // 阅读设置（字体/字号/行距/边距）
#ifdef BSP_USING_BOARD_SF32LB527_OED_E0470A03
  SET_FRONTLIGHT = 5,
  SET_USB_DFU = 6,
  SET_AUTO_TURN = 7,
  SET_STANDBY = 8,
  SET_CONFIRM = 9
#else
  SET_CONFIRM = 5
#endif
} SettingsItem;

typedef enum {
  MAIN_PAGE,           // 新主页面
  SELECTING_EPUB,      // 电子书列表页面(书库)
  SELECTING_TABLE_CONTENTS, // 电子书目录页面
  READING_EPUB,        // 阅读页面
  READING_SETTINGS,    // 阅读设置页面（字体/字号/行距/边距）
  SETTINGS_PAGE,       // 通用功能设置页面
  WEATHER_PAGE,        // 查看天气页面
  WEATHER_CITY_PAGE,   // 天气城市选择页面
  WELCOME_PAGE,        // 欢迎页面
  LOW_POWER_PAGE,      // 低电量页面
  CHARGING_PAGE,       // 充电页面
  SHUTDOWN_PAGE,       // 关机页面
  FRONTLIGHT_PAGE,
  USB_DFU_PAGE
} AppUIState;
#endif
