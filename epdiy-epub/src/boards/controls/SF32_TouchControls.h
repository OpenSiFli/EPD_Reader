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
  // 添加手势检测状态
  bool is_touch_down = false;        // 是否正在触摸
  int touch_start_x = 0;             // 按下时的 X 坐标
  int touch_start_y = 0;             // 按下时的 Y 坐标
  int touch_current_x = 0;           // 当前触摸 X 坐标
  int touch_current_y = 0;           // 当前触摸 Y 坐标
  // 滑动检测阈值
  static const int SWIPE_THRESHOLD = 50;  // 最小滑动距离阈值
  
public:
  static rt_err_t tp_rx_indicate(rt_device_t dev, rt_size_t size);
  SF32_TouchControls(Renderer *renderer, ActionCallback_t on_action);
  void render(Renderer *renderer) override;
  void renderPressedState(Renderer *renderer, UIAction action, bool state = true) override;
  void powerOnTouch() override;   
  void powerOffTouch() override;
};

// 最近一次设置页左右箭头触控标记
// 行号：0=触控开关，1=超时关机，2=全刷周期；-1=无
extern volatile int g_touch_last_settings_row;
// 方向：-1=左(减)，+1=右(加)，0=无
extern volatile int g_touch_last_settings_dir;