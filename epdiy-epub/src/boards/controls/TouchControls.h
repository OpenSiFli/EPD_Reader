#pragma once

#include "Actions.h"

class Renderer;

// TODO - we should move the rendering out of this class so that it's only doing the touch detection
class TouchControls
{
protected:
  bool touch_enable = false;

public:
  TouchControls(){};
  // draw the controls on the screen
  virtual void render(Renderer *renderer) {}
  // show the touched state
  virtual void renderPressedState(Renderer *renderer, UIAction action, bool state = true) {}

  virtual void powerOnTouch() {}
  virtual void powerOffTouch() {}

  bool isTouchEnabled() const { return touch_enable; }
  void setTouchEnable(bool enable) { touch_enable = enable; }
};