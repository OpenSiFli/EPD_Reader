/* SPDX-License-Identifier: Apache-2.0
 * One synchronous refresh before USB starts. No app fonts, TF or touch tasks.
 * The panel scan and SY7636 shutdown protections are shared with the reader.
 */
#include <rtthread.h>
#include "board.h"
#include "board527.h"
#include "frontlight.h"
#include "epub_mem.h"
#include <string.h>

extern const uint8_t reader_recovery_bitmap[1216*684/8];

/* Only this main thread accesses display/PMIC; no other peripheral clients. */
int board527_io_enter(void){return 0;}
void board527_io_exit(void){}
void frontlight_display_lock(void){}
void frontlight_display_unlock(void){}
void board527_touch_lock(void){}
void board527_touch_unlock(void){}
/* The binary screen uses the fixed cache; never borrow the internal heap. */
size_t epub_mem_available_size(void){return 0;}
void *epub_mem_malloc(size_t size){(void)size;return RT_NULL;}
void epub_mem_free(void *p){(void)p;}

int reader_recovery_screen(void)
{
    uint32_t begin=rt_tick_get_millisecond();
    int rc=tps_init(1100);
    if(!rc)rc=board527_display_init();
    if(!rc) {
        uint8_t *frame=board527_framebuffer();
        for(unsigned i=0;i<1216*684/2;++i) {
            unsigned bit=i*2;
            unsigned a=(reader_recovery_bitmap[bit/8]>>(7-bit%8))&1;
            unsigned b=(reader_recovery_bitmap[(bit+1)/8]>>(7-(bit+1)%8))&1;
            frame[i]=(a?15:0)|(b?240:0);
        }
        rc=board527_display_present(frame);
    }
    board527_display_off();
    /* Let EN-low discharge finish before reset releases the AW outputs. */
    if(rc)(void)tps_enter_sleep();
    rt_thread_mdelay(450);
    /* Avoid driving panel pins into the now-unpowered shared logic rail. */
    const uint8_t pins[]={EPD_LE,EPD_OE,EPD_STV,EPD_CPV,EPD_GMODE,
        EPD_CLK,EPD_SPH,EPD_D0,EPD_D1,EPD_D2,EPD_D3,EPD_D4,EPD_D5,EPD_D6,EPD_D7,10,11};
    for(unsigned i=0;i<sizeof(pins);++i)HAL_PIN_Set_Analog(PAD_PA00+pins[i],1);
    HAL_RCC_DisableModule(RCC_MOD_LCDC1);
    rt_pin_write(3,0);
    rt_kprintf("DFU527_SCREEN rc=%d ms=%u HV=OFF\n",rc,rt_tick_get_millisecond()-begin);
    return rc;
}
