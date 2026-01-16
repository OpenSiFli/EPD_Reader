#pragma once

#include <rtthread.h>
#include "boards/SF32PaperRenderer.h"
#include "boards/controls/Actions.h"
#include "boards/controls/TouchControls.h"
#include "boards/controls/SF32_TouchControls.h"


// 初始化屏幕模块（设置默认的关机超时小时数，0 表示不关机）
void screen_init(int default_timeout_hours);

// 获取当前关机超时设置（小时；0 表示不关机）
int screen_get_timeout_shutdown_minutes();

// 获取当前主页面选中的选项（0: 打开书库, 1: 继续阅读, 2: 进入设置）
int screen_get_main_selected_option();

// 主页面交互与渲染
void handleMainPage(Renderer *renderer, UIAction action, bool needs_redraw);

// 设置页面交互与渲染；返回 true 表示确认并退出到主页面
bool handleSettingsPage(Renderer *renderer, UIAction action, bool needs_redraw);
// 切换全刷周期（循环）
void screen_cycle_full_refresh_period();
// 获取当前全刷周期值
int screen_get_full_refresh_period();