#pragma once

#include <rtthread.h>
#include "boards/SF32PaperRenderer.h"
#include "boards/controls/Actions.h"
#include "boards/controls/TouchControls.h"
#include "boards/controls/SF32_TouchControls.h"

typedef enum
{
	OPTION_OPEN_LIBRARY = 0,
	OPTION_ENTER_SETTINGS,
	OPTION_WEATHER,
	OPTION_CONTINUE_READING,
	OPTION_BLANK_PAGE,
	OPTION_COUNT
} MainOption;

// 初始化屏幕模块（设置默认的关机超时小时数，0 表示不关机）
void screen_init(int default_timeout_hours);

// 获取当前关机超时设置（小时；0 表示不关机）
int screen_get_timeout_shutdown_minutes();

// 获取当前主页面选中的选项(0:打开书库 1:进入设置 2:查看天气 3:继续阅读)
MainOption screen_get_main_selected_option();

// 主页面交互与渲染
void handleMainPage(Renderer *renderer, UIAction action, bool needs_redraw);

// 设置页面交互与渲染
// 返回值：0=继续停留在设置页，1=确认退出到主页面，2=进入阅读设置页面
int handleSettingsPage(Renderer *renderer, UIAction action, bool needs_redraw);

// Weather page handling
// 返回值：0=继续停留在天气页，1=返回主页面，2=进入城市选择页
int handleWeatherPage(Renderer *renderer, UIAction action, bool needs_redraw);

// Weather city page handling
// 返回值：0=继续停留在城市选择页，1=返回天气页，2=确认切换城市并返回天气页
int handleWeatherCityPage(Renderer *renderer, UIAction action, bool needs_redraw);

// 空白页面处理（测试用，绘制45°斜线）
// 返回值：0=继续停留在空白页，1=返回主页面
int handleBlankPage(Renderer *renderer, UIAction action, bool needs_redraw);

// 切换全刷周期（循环）
void screen_cycle_full_refresh_period(bool refresh);
// 获取当前全刷周期值
int screen_get_full_refresh_period();

// 蓝牙 UI 状态（由设置页控制）
extern bool bluetooth_ui_enabled;
// 蓝牙待切换标志（设置页点击时设置，主循环执行完操作后清除）
extern bool bluetooth_pending_toggle;
// 蓝牙连接状态：0=断开，1=已连接
extern volatile int g_bluetooth_connected;