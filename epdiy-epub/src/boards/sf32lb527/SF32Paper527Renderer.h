#pragma once
#include "EpdiyFrameBufferRenderer.h"
extern "C" {
#include "board527.h"
}
class SF32PaperRenderer : public EpdiyFrameBufferRenderer {
    uint8_t *preview_frame=nullptr;
    bool preview_gray=false;
public:
    SF32PaperRenderer(const EpdFont *regular,const EpdFont *bold,const EpdFont *italic,
                      const EpdFont *bold_italic,const uint8_t *busy,int w,int h)
        : EpdiyFrameBufferRenderer(regular,bold,italic,bold_italic,busy,w,h)
    {
        m_frame_buffer=board527_framebuffer();clear_screen();
    }
    void powerOnLcd() {}
    void powerOffLcd(){board527_display_off();}
    /* A transient hourglass would require another complete waveform before
     * the requested page. Keep the last page visible until its replacement. */
    void show_busy() override {}
    void flush_display() override
    {
        if(preview_frame)return;
        int rc=board527_display_present(m_frame_buffer);
        if(rc)rt_kprintf("READER_DISPLAY_ERROR rc=%d\n",rc);
    }
    bool begin_page_preview() override
    {
        board527_display_cancel_prepare();
        const size_t size=1216*684/2;
        if(preview_frame||epub_mem_available_size()<size+768*1024)return false;
        preview_frame=static_cast<uint8_t *>(epub_mem_malloc(size));
        if(!preview_frame)return false;
        preview_gray=needs_gray_flush;m_frame_buffer=preview_frame;return true;
    }
    void end_page_preview(int (*cancel)(void *),void *context) override
    {
        if(!preview_frame)return;
        if(!cancel||!cancel(context))board527_display_prepare(preview_frame,cancel,context);
        m_frame_buffer=board527_framebuffer();needs_gray_flush=preview_gray;
        epub_mem_free(preview_frame);preview_frame=nullptr;
    }
    void cancel_page_preview() override
    {
        if(preview_frame) {
            m_frame_buffer=board527_framebuffer();needs_gray_flush=preview_gray;
            epub_mem_free(preview_frame);preview_frame=nullptr;
        }
        board527_display_cancel_prepare();
    }
    void flush_area(int x,int y,int w,int h) override
    {
        (void)x;(void)y;(void)w;(void)h;flush_display();
    }
    /* In the EPUB parser this legacy hook requests a separate white-page
     * flush before drawing. The driver already tracks all old/new gray levels
     * and its cleaning interval, so no intermediate white image is needed. */
    bool has_gray() override {return false;}
    void reset() override {}
};
