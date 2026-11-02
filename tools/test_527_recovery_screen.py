"""Run the real one-shot recovery screen with display failures injected."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
loader=root/'epdiy-epub/dfu527/loader'
source=(loader/'recovery_screen.c').read_text()
function=source[source.index('int reader_recovery_screen(void)'):]
bitmap=(loader/'recovery_screen.bin').read_bytes()
assert len(bitmap)==1216*684//8
assert 0 < sum(8-b.bit_count() for b in bitmap) < 1216*684//3
stubs=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define EPD_LE 0
#define EPD_OE 1
#define EPD_STV 2
#define EPD_CPV 4
#define EPD_GMODE 5
#define EPD_CLK 6
#define EPD_SPH 7
#define EPD_D0 8
#define EPD_D1 9
#define EPD_D2 24
#define EPD_D3 25
#define EPD_D4 28
#define EPD_D5 29
#define EPD_D6 37
#define EPD_D7 39
#define PAD_PA00 0
#define RCC_MOD_LCDC1 1
#define rt_kprintf(...) ((void)0)
static uint8_t reader_recovery_bitmap[1216*684/8],frame[1216*684/2];
static int failure,draws,sleep_calls,reset,parked,clock_disabled;
static unsigned now;
static unsigned rt_tick_get_millisecond(void){return now;}
static int tps_init(int v){assert(v==1100);return failure==1?-1:0;}
static int board527_display_init(void){return failure==2?-1:0;}
static uint8_t *board527_framebuffer(void){return frame;}
static int board527_display_present(uint8_t *p){
 ++draws;assert(p==frame);
 for(unsigned pixel=0;pixel<1216*684;++pixel){
   unsigned white=(reader_recovery_bitmap[pixel/8]>>(7-pixel%8))&1;
   assert(((p[pixel/2]>>(4*(pixel%2)))&15)==(white?15:0));
 }
 return failure==3?-1:0;
}
static void board527_display_off(void){}
static int tps_enter_sleep(void){++sleep_calls;return 0;}
static void rt_thread_mdelay(unsigned ms){assert(ms==450);now+=ms;}
static void HAL_PIN_Set_Analog(int pin,int en){assert(en==1&&pin!=34&&pin!=35&&pin!=36);++parked;}
static void HAL_RCC_DisableModule(int mod){assert(mod==1);clock_disabled=1;}
static void rt_pin_write(int pin,int v){assert(pin==3&&v==0&&now>=450&&parked==17&&clock_disabled);reset=1;}
'''
tests=r'''
int main(int argc,char **argv){
 FILE *f=fopen(argv[1],"rb");assert(f);assert(fread(reader_recovery_bitmap,1,sizeof(reader_recovery_bitmap),f)==sizeof(reader_recovery_bitmap));fclose(f);
 for(failure=0;failure<=3;++failure){
  draws=sleep_calls=reset=parked=clock_disabled=now=0;
  int rc=reader_recovery_screen();assert((rc!=0)==(failure!=0));
  assert(reset&&parked==17&&sleep_calls==(failure!=0));
  assert(draws==(failure==0||failure==3));
 }
 puts("RECOVERY_BITMAP_UNPACK_FAILURE_HV_OFF_USB_PINS_PRESERVED_PASS");
}
'''
with tempfile.TemporaryDirectory() as directory:
    directory=Path(directory)
    (directory/'test.c').write_text(stubs+function+tests)
    subprocess.run(['gcc','-std=c99','-O2',str(directory/'test.c'),'-o',str(directory/'test.exe')],check=True)
    subprocess.run([str(directory/'test.exe'),str(loader/'recovery_screen.bin')],check=True)
