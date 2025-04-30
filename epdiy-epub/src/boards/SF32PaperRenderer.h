#pragma once
#include <rtdbg.h>
#include "EpdiyFrameBufferRenderer.h"
#include "miniz.h"

extern "C" {
#include "mem_section.h"
#include "epd_tps.h"
#include "epd_pin_defs.h"
L2_NON_RET_BSS_SECT_BEGIN(frambuf)
// L2_NON_RET_BSS_SECT(frambuf, ALIGN(64) static uint8_t framebuffer1[(EPD_WIDTH * EPD_HEIGHT + 7) / 8]);//1bpp
L2_NON_RET_BSS_SECT(frambuf, ALIGN(64) static uint8_t framebuffer1[EPD_WIDTH * EPD_HEIGHT / 2]);
L2_NON_RET_BSS_SECT_END

}
static rt_device_t lcd_device = NULL;

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
 
    lcd_device = rt_device_find("lcd");
    if (rt_device_open(lcd_device, RT_DEVICE_OFLAG_RDWR) == RT_EOK)
    {
        struct rt_device_graphic_info info;
        if (rt_device_control(lcd_device, RTGRAPHIC_CTRL_GET_INFO, &info) == RT_EOK)
        {
            rt_kprintf("Lcd info w:%d, h%d, bits_per_pixel %d, draw_align:%d\r\n",
                       info.width, info.height, info.bits_per_pixel, info.draw_align);
        }
    }
    else
    {
        rt_kprintf("Lcd open error!\n");
        return;
    }
    uint16_t framebuffer_color_format = RTGRAPHIC_PIXEL_FORMAT_GRAY4;
    rt_device_control(lcd_device, RTGRAPHIC_CTRL_SET_BUF_FORMAT, &framebuffer_color_format);

     m_frame_buffer = (uint8_t *)framebuffer1;
    // clear_screen();
  }
  ~SF32PaperRenderer()
  {
    // TODO: cleanup and shutdown?
  }
  static rt_err_t lcd_flush_done(rt_device_t dev, void *buffer)
  {
      rt_kprintf("lcd_flush_done!\n");
      return RT_EOK;
  }
  void flush_display()
  {
    rt_graphix_ops(lcd_device)->set_window(0, 0, LCD_HOR_RES_MAX - 1, LCD_VER_RES_MAX - 1);

    rt_graphix_ops(lcd_device)->draw_rect((const char *)m_frame_buffer, 0, 0, LCD_HOR_RES_MAX - 1, LCD_VER_RES_MAX - 1);
  }
  

  void flush_area(int x, int y, int width, int height)
  {
    // there's probably a way of only sending the data we need to send for the area
    // driver.WriteFullGram4bpp(m_frame_buffer);
    // // don't forger we're rotated
    // driver.UpdateArea(y, x, height, width, needs_gray_flush ? UPDATE_MODE_GC16 : UPDATE_MODE_DU);
    // needs_gray_flush = false;
    rt_graphix_ops(lcd_device)->set_window(0, 0, LCD_HOR_RES_MAX - 1, LCD_VER_RES_MAX - 1);

    rt_graphix_ops(lcd_device)->draw_rect((const char *)m_frame_buffer, 0, 0, LCD_HOR_RES_MAX - 1, LCD_VER_RES_MAX - 1);
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