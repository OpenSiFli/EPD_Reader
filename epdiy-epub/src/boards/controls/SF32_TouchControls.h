#pragma once

#include "TouchControls.h"

class Renderer;

class SF32_TouchControls : public TouchControls
{
private:
  ActionCallback_t on_action;
  
  // Touch UI buttons (Top left and activated only when USE_TOUCH is defined)
  uint8_t ui_button_width = 120;
  uint8_t ui_button_height = 34;
  UIAction last_action = NONE;
  Renderer *renderer = nullptr;

  friend void touchTask(void *param);

public:
  SF32_TouchControls(Renderer *renderer, int touch_int, int width, int height, int rotation, ActionCallback_t on_action);
  void render(Renderer *renderer);
  void renderPressedState(Renderer *renderer, UIAction action, bool state = true);
  void handleTouch(int x, int y);
};