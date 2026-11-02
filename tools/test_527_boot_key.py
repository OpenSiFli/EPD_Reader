"""Exercise the actual boot key sampler with held/released and bouncing inputs."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'epdiy-epub/dfu527/boot/boot_entry.c').read_text()
sampler = source[source.index('static int recovery_key(void)'):source.index('static void boot_reader(void)')]
stubs = r'''
#include <assert.h>
#define PAD_PA33 33
#define PAD_PA34 34
#define GPIO_A33 33
#define GPIO_A34 34
#define PIN_PULLUP 1
#define PIN_PULLDOWN 2
#define GPIO_PULLUP 1
#define GPIO_PULLDOWN 2
#define GPIO_MODE_INPUT 0
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
#define hwp_gpio1 0
#ifdef SF32LB57X
#define TEST_PIN 34
#define TEST_PULL GPIO_PULLDOWN
#define TEST_ACTIVE GPIO_PIN_SET
#else
#define TEST_PIN 33
#define TEST_PULL GPIO_PULLUP
#define TEST_ACTIVE GPIO_PIN_RESET
#endif
typedef struct {int Pin,Mode,Pull;} GPIO_InitTypeDef;
static unsigned samples,delays,release_at;
static int pressed;
static void HAL_PIN_Set(int pad,int gpio,int pull,int cpu){assert(pad==TEST_PIN&&gpio==TEST_PIN&&pull==TEST_PULL&&cpu==1);}
static void HAL_GPIO_Init(int gpio,GPIO_InitTypeDef *c){assert(c->Pin==TEST_PIN&&c->Pull==TEST_PULL&&c->Mode==GPIO_MODE_INPUT);}
static int HAL_GPIO_ReadPin(int gpio,int pin){assert(pin==TEST_PIN);return pressed&&samples++<release_at?TEST_ACTIVE:!TEST_ACTIVE;}
static void HAL_Delay_us(unsigned us){assert(us==10000);++delays;}
'''
tests = r'''
int main(void){
  pressed=0;samples=delays=0;assert(!recovery_key()&&!delays);
  pressed=1;release_at=150;samples=delays=0;assert(recovery_key()&&delays==150);
  for(release_at=0;release_at<150;++release_at){samples=delays=0;assert(!recovery_key()&&delays==release_at);}
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path/'test.c').write_text(stubs+sampler+tests)
    for flags in ([], ['-DSF32LB57X']):
        subprocess.run(['gcc','-std=c99',*flags,str(path/'test.c'),'-o',str(path/'test.exe')],check=True)
        subprocess.run([str(path/'test.exe')],check=True)
print('BOOT_PA33_ACTIVE_LOW_HOLD_RELEASE_AND_57_POLICY_PASS')
