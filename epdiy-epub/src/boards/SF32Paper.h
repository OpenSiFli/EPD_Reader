#pragma once

#include "Board.h"

class SF32Paper : public Board
{
public:
  virtual void power_up();
  virtual void prepare_to_sleep();
  virtual Renderer *get_renderer();
  virtual void start_filesystem();
  virtual void stop_filesystem();
  virtual ButtonControls *get_button_controls(rt_mq_t ui_queue);
};