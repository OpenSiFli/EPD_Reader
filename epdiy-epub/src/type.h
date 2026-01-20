#ifndef TYPES_H
#define TYPES_H

// typedef enum {
//   MAIN_PAGE,
//   SELECTING_EPUB,
//   SELECTING_TABLE_CONTENTS,
//   READING_EPUB,
//   SETTINGS_PAGE
// } UIState;

// typedef enum {
//   MAIN_MENU,
//   WELCOME_PAGE,
//   LOW_POWER_PAGE,
//   CHARGING_PAGE
// } UIState2;

// 设置页列表项
typedef enum { 
  SET_TOUCH = 0, 
  SET_TIMEOUT = 1, 
  SET_FULL_REFRESH = 2, 
  SET_CONFIRM = 3 
} SettingsItem;

typedef enum {
  MAIN_PAGE,           // 新主页面
  SELECTING_EPUB,      // 电子书列表页面(书库)
  SELECTING_TABLE_CONTENTS, // 电子书目录页面
  READING_EPUB,        // 阅读页面
  SETTINGS_PAGE,       // 通用功能设置页面
  WELCOME_PAGE,        // 欢迎页面
  LOW_POWER_PAGE,      // 低电量页面
  CHARGING_PAGE,       // 充电页面
  SHUTDOWN_PAGE        // 关机页面
} AppUIState;
#endif