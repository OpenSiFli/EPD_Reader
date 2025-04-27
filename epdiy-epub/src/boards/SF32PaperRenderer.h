#pragma once
#include <rtdbg.h>
#include "EpdiyFrameBufferRenderer.h"
#include "miniz.h"

extern "C" {
#include "mem_section.h"
#include "opm060e9_driver.h"
#include "epd_tps.h"
#include "epd_pin_defs.h"

L2_NON_RET_BSS_SECT_BEGIN(frambuf)
// L2_NON_RET_BSS_SECT(frambuf, ALIGN(64) static uint8_t framebuffer1[(EPD_WIDTH * EPD_HEIGHT + 7) / 8]);//1bpp
L2_NON_RET_BSS_SECT(frambuf, ALIGN(64) static uint8_t framebuffer1[EPD_WIDTH * EPD_HEIGHT / 2]);
L2_NON_RET_BSS_SECT_END

}

class SF32PaperRenderer : public EpdiyFrameBufferRenderer
{
private:
  // M5EPD_Driver driver;

public:
  SF32PaperRenderer(
      const EpdFont *regular_font,
      const EpdFont *bold_font,
      const EpdFont *italic_font,
      const EpdFont *bold_italic_font,
      const uint8_t *busy_icon,
      int busy_icon_width,
      int busy_icon_height)
      : EpdiyFrameBufferRenderer(regular_font, bold_font, italic_font, bold_italic_font, busy_icon, busy_icon_width, busy_icon_height)
  {
    // driver.begin();
    // driver.SetColorReverse(true);

    oedtps_init();
    epd_Init();
     m_frame_buffer = (uint8_t *)framebuffer1;
    // clear_screen();
  }
  ~SF32PaperRenderer()
  {
    // TODO: cleanup and shutdown?
  }
  void flush_display()
  {
    // driver.WriteFullGram4bpp(m_frame_buffer);
    // driver.UpdateFull(needs_gray_flush ? UPDATE_MODE_GC16 : UPDATE_MODE_DU);
    // needs_gray_flush = false;

    epd_display_pic(m_frame_buffer);


  }
  void flush_area(int x, int y, int width, int height)
  {
    // there's probably a way of only sending the data we need to send for the area
    // driver.WriteFullGram4bpp(m_frame_buffer);
    // // don't forger we're rotated
    // driver.UpdateArea(y, x, height, width, needs_gray_flush ? UPDATE_MODE_GC16 : UPDATE_MODE_DU);
    // needs_gray_flush = false;
     epd_display_pic(m_frame_buffer);
  }
  virtual bool hydrate()
  {
    ulog_i("M5P", "Hydrating EPD");
    if (EpdiyFrameBufferRenderer::hydrate())
    {
      ulog_i("M5P", "Hydrated EPD");
      // driver.WriteFullGram4bpp(m_frame_buffer);
      // driver.UpdateFull(UPDATE_MODE_GC16);
      return true;
    }
    else
    {
      ulog_i("M5P", "Hydrate EPD failed");
      // reset();
      return false;
    }
  }
  virtual void reset()
  {
    ulog_i("M5P", "Full clear");
    // clear_screen();
    // // flushing to white
    // needs_gray_flush = false;
    // flush_display();
  };
};