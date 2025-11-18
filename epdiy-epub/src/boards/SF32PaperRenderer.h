#pragma once
#include <rtdbg.h>
#include "EpdiyFrameBufferRenderer.h"
#include "miniz.h"
#include "boards/controls/SF32_TouchControls.h"
extern "C" {
#include "mem_section.h"

#include "epd_pin_defs.h"
L2_NON_RET_BSS_SECT_BEGIN(frambuf)
#ifdef EPDIY_EPUB_1BPP
L2_NON_RET_BSS_SECT(frambuf, ALIGN(64) static uint8_t framebuffer1[(EPD_WIDTH * EPD_HEIGHT + 7) / 8]);//1bpp
#else
L2_NON_RET_BSS_SECT(frambuf, ALIGN(64) static uint8_t framebuffer1[EPD_WIDTH * EPD_HEIGHT / 2]);
#endif

L2_NON_RET_BSS_SECT_END

}
class SF32PaperRenderer : public EpdiyFrameBufferRenderer {
private:
  // M5EPD_Driver driver;
  rt_device_t lcd_device = NULL;
  uint8_t idle_mode_on = 0;  
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
#ifdef EPDIY_EPUB_1BPP
    uint16_t framebuffer_color_format = RTGRAPHIC_PIXEL_FORMAT_MONO;
#else
    uint16_t framebuffer_color_format = RTGRAPHIC_PIXEL_FORMAT_GRAY4;
#endif
    rt_device_control(lcd_device, RTGRAPHIC_CTRL_SET_BUF_FORMAT, &framebuffer_color_format);

     m_frame_buffer = (uint8_t *)framebuffer1;
     clear_screen();
    

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

  void powerOffLcd()
  {    
    // uint8_t idle_mode_on;  
    if (lcd_device)
    {
      idle_mode_on = 1;
        rt_device_control(lcd_device, RTGRAPHIC_CTRL_SET_MODE, (void *)&idle_mode_on);
        rt_kprintf("LCD close\n");

    }
    else
    {
      rt_kprintf("no find lcd device\n");
    }
  }

  void powerOnLcd()
  {

    if (lcd_device)
    {
      idle_mode_on = 0;
      rt_device_control(lcd_device, RTGRAPHIC_CTRL_SET_MODE, (void *)&idle_mode_on);

      rt_kprintf("LCD Open\n");
    }
    else
    {
      rt_kprintf("no find lcd device\n");
    }
  }
 
  void flush_display()
  {
    powerOnLcd();
    rt_graphix_ops(lcd_device)->set_window(0, 0, LCD_HOR_RES_MAX - 1, LCD_VER_RES_MAX - 1);
    rt_graphix_ops(lcd_device)->draw_rect((const char *)m_frame_buffer, 0, 0, LCD_HOR_RES_MAX - 1, LCD_VER_RES_MAX - 1);
    powerOffLcd();
  }

  bool has_gray() 
  {
    return false;
  }



  void flush_area(int x, int y, int width, int height)
  {
    // there's probably a way of only sending the data we need to send for the area
    // driver.WriteFullGram4bpp(m_frame_buffer);
    // // don't forger we're rotated
    // driver.UpdateArea(y, x, height, width, needs_gray_flush ? UPDATE_MODE_GC16 : UPDATE_MODE_DU);
    // needs_gray_flush = false;
    powerOnLcd();
    rt_graphix_ops(lcd_device)->set_window(0, 0, LCD_HOR_RES_MAX - 1, LCD_VER_RES_MAX - 1);
    rt_graphix_ops(lcd_device)->draw_rect((const char *)m_frame_buffer, 0, 0, LCD_HOR_RES_MAX - 1, LCD_VER_RES_MAX - 1);
    powerOffLcd();
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