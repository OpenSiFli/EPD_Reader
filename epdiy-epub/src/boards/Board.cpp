#include <rtdbg.h>
#include "Board.h"
#include "SF32Paper.h"
#if !defined(SF32LB57X) && !defined(BSP_USING_BOARD_SF32LB527_OED_E0470A03)
#include "battery/ADCBattery.h"
#endif

Board *Board::factory()
{
  return new SF32Paper();
}

void Board::start_filesystem()
{
  // create the EPD renderer
  ulog_i("main", "start_filesystem");
}

void Board::stop_filesystem()
{
   ulog_i("main", "stop_filesystem");
}

Battery *Board::get_battery(rt_mq_t ui_queue)
{
#if !defined(SF32LB57X) && !defined(BSP_USING_BOARD_SF32LB527_OED_E0470A03)
  return new ADCBattery(ui_queue);
#else
  (void)ui_queue;
  return nullptr;
#endif
}

TouchControls *Board::get_touch_controls(Renderer *renderer, rt_mq_t ui_queue)
{
  return new TouchControls();
}
