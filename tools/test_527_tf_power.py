"""Exercise actual shared-rail policy and retained-card identity gates."""
from pathlib import Path
import argparse,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
root=Path(__file__).resolve().parents[1]/'epdiy-epub/src/boards/sf32lb527'
power=(root/'tf_power.c').read_text(encoding='utf8')
msd=(root/'spi_msd.c').read_text(encoding='utf8')
shared=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#define RT_EOK 0
#define RT_EIO 5
#define RT_EBUSY 16
#define RT_ENOSYS 38
#define RT_IPC_FLAG_PRIO 0
#define RT_WAITING_FOREVER -1
#define RT_TICK_PER_SECOND 1000
#define rt_kprintf(...) ((void)0)
using rt_tick_t=uint32_t;
static unsigned now;
static unsigned rt_tick_get(){return now;}
static unsigned rt_tick_get_millisecond(){return now;}
static unsigned rt_tick_from_millisecond(unsigned n){return n;}
static void rt_thread_mdelay(unsigned n){now+=n;}
'''
rail=shared+r'''
struct rt_mutex{};
static rt_mutex shared_lock;
static int initialized,automatic,rail_off;
static rt_tick_t last_use;
static unsigned sleeps,wakes,errors;
static int depth,votes,other_thread,touch,fail_off,fail_on,fail_sync,hv_busy;
static int parks,restores,cold,gate_calls;
static void rt_mutex_init(rt_mutex *,const char *,int){}
static int rt_mutex_take(rt_mutex *,int timeout){if(other_thread)return -RT_EBUSY;++depth;return 0;}
static void rt_mutex_release(rt_mutex *){assert(depth>0);--depth;}
static void reader_power_busy(){++votes;}
static void reader_power_idle(){assert(votes>0);--votes;}
static int board527_touch_active(){return touch;}
static int board527_logic_sleep_ready(){return !hv_busy;}
static int board527_logic_power(int on){++gate_calls;return (on?fail_on:fail_off)?-RT_EIO:0;}
static void park(){++parks;}
static void restore(){++restores;}
static int tf_msd_prepare_off(){assert(votes&&depth);return fail_sync?-RT_EIO:0;}
static void tf_msd_power_lost(){cold=1;}
'''+power[power.index('static int wake_locked'):power.index('static int tfpm')]+r'''
int main(){
    assert(board527_storage_idle(1)==-RT_ENOSYS);
    assert(board527_storage_wait_ms(5000)==5000);
    board527_io_init();board527_storage_pm_enable(1);assert(votes==0&&depth==0);
    assert(board527_storage_wait_ms(5000)==500&&board527_storage_wait_ms(100)==100);
    now=499;assert(board527_storage_idle(0)==0&&parks==0&&board527_storage_wait_ms(5000)==1);
    now=500;touch=1;assert(board527_storage_idle(0)==-RT_EBUSY&&parks==0);touch=0;
    assert(board527_storage_wait_ms(5000)==50);
    hv_busy=1;assert(board527_storage_idle(1)==-RT_EBUSY&&parks==0&&!cold&&automatic&&errors==0);hv_busy=0;
    other_thread=1;assert(board527_storage_idle(1)==-RT_EBUSY);other_thread=0;
    assert(board527_storage_idle(0)==0&&rail_off&&cold&&sleeps==1);
    assert(votes==0&&depth==0);
    assert(board527_storage_wait_ms(5000)==5000);
    int calls=gate_calls;assert(board527_storage_idle(1)==0&&gate_calls==calls);
    assert(board527_io_enter()==0&&!rail_off&&restores==1&&wakes==1&&votes==1);
    assert(board527_io_enter()==0&&votes==2&&depth==2);
    board527_io_exit();board527_io_exit();assert(votes==0&&depth==0);
    assert(board527_io_diagnostic_begin()==0&&votes==0&&depth==1);
    board527_io_diagnostic_end();assert(votes==0&&depth==0);
    fail_sync=1;assert(board527_storage_idle(1)!=0&&!automatic&&!rail_off);fail_sync=0;
    board527_storage_pm_enable(1);fail_off=1;
    assert(board527_storage_idle(1)!=0&&!automatic&&!rail_off&&cold);fail_off=0;
    board527_storage_pm_enable(1);assert(!board527_storage_idle(1));fail_on=1;
    assert(board527_io_enter()!=0&&rail_off&&votes==0&&depth==0);fail_on=0;
    assert(!board527_io_enter()&&!rail_off);board527_io_exit();
    board527_storage_pm_enable(0);calls=gate_calls;assert(!board527_storage_idle(1)&&gate_calls==calls);
    assert(board527_storage_wait_ms(5000)==5000);
    board527_storage_pm_enable(1);last_use=0xffffff00u;now=0x0000002cu;
    assert(board527_storage_wait_ms(5000)==200); // tick wrap, 300ms elapsed
    other_thread=1;assert(board527_storage_wait_ms(5000)==50&&board527_storage_wait_ms(20)==20);other_thread=0;
    assert(votes==0&&depth==0);
    puts("TF_POWER_IDLE_LOCK_WAKE_FAILURE_DIAGNOSTIC_VOTES_PASS");
}
'''
card=shared+r'''
#define BOARD527_VERBOSE_LOGS 0
#define ALIGN(n)
#define SEND_CID 10
#define SEND_STATUS 13
#define response_r1 1
#define response_r2 2
using rt_device_t=void *;using rt_size_t=unsigned;using rt_off_t=int;
struct mutex{};
struct bus_t {mutex lock;};struct rt_spi_device {bus_t *bus;};
struct rt_device_blk_geometry {unsigned sector_count,bytes_per_sector,block_size;};
using msd_card_type=int;
struct msd_device {rt_spi_device *spi_device;rt_device_blk_geometry geometry;int card_type;long offset;};
static bus_t spi_bus;static rt_spi_device spi={&spi_bus};
static msd_device _msd_device={&spi,{1024,512,512},4,0};
static unsigned pm_restoring,pm_cold,pm_identity_valid,pm_fault,pm_resumes,io_errors;
static uint8_t pm_cid[16],current_cid[16]={1,2,3};
static rt_device_blk_geometry pm_geometry;
static msd_card_type pm_card_type;static long pm_offset;
static int present=1,init_error,read_error,cmd_error,status_error,ready_error,io_enter_error;
static int spi_depth,io_depth,raw_ios,init_calls;
static int board_tf_present(){return present;}
static int MSD_take_owner(rt_spi_device *){++spi_depth;return 0;}
static void rt_spi_take(rt_spi_device *){}
static void rt_spi_release(rt_spi_device *){}
static void rt_mutex_release(mutex *){assert(spi_depth>0);--spi_depth;}
static int _send_cmd(rt_spi_device *,int cmd,int,int,int,uint8_t *r){r[0]=0;r[1]=cmd==SEND_STATUS?status_error:0;return cmd_error?-1:0;}
static int _read_block(rt_spi_device *,void *out,unsigned n){memcpy(out,current_cid,n);return read_error?-1:0;}
static int _wait_ready(rt_spi_device *){return ready_error?-1:0;}
static int rt_msd_init(rt_device_t){++init_calls;return init_error?-1:0;}
static int board527_io_enter(){if(io_enter_error)return -1;++io_depth;return 0;}
static void board527_io_exit(){assert(io_depth>0);--io_depth;}
static rt_size_t sector_io_locked(rt_device_t,rt_off_t,void *,rt_size_t count,int){++raw_ios;return count;}
'''
start=msd.index('static int pm_identify(int remember)\n{')
end=msd.index('void tf_msd_status',start)
card+=msd[start:end]
start=msd.index('static rt_size_t sector_io(rt_device_t')
card+=msd[start:msd.index('static rt_size_t rt_msd_read',start)]
card+=r'''
static void reset_case(){
    pm_fault=pm_cold=0;present=1;init_error=read_error=cmd_error=status_error=ready_error=0;
    current_cid[0]=1;_msd_device.geometry={1024,512,512};_msd_device.card_type=4;_msd_device.offset=0;
    assert(!pm_identify(1));raw_ios=0;
}
int main(){
    reset_case();assert(!tf_msd_prepare_off());tf_msd_power_lost();assert(pm_cold);
    uint8_t b[512];assert(sector_io(&_msd_device,0,b,1,1)==1&&raw_ios==1&&!pm_cold&&pm_resumes==1);
    for(int fail=0;fail<7;++fail){
        reset_case();tf_msd_power_lost();
        if(fail==0)current_cid[0]=9;
        if(fail==1)_msd_device.geometry.sector_count=2048;
        if(fail==2)_msd_device.card_type=3;
        if(fail==3)present=0;
        if(fail==4)init_error=1;
        if(fail==5)read_error=1;
        if(fail==6)_msd_device.offset=1;
        assert(sector_io(&_msd_device,0,b,1,1)==0&&raw_ios==0&&pm_fault);
        present=1;read_error=init_error=0;current_cid[0]=1;
        assert(sector_io(&_msd_device,0,b,1,1)==0&&raw_ios==0);
        assert(io_depth==0&&spi_depth==0);
    }
    reset_case();ready_error=1;assert(tf_msd_prepare_off()!=0);ready_error=0;
    status_error=1;assert(tf_msd_prepare_off()!=0);status_error=0;
    cmd_error=1;assert(tf_msd_prepare_off()!=0);cmd_error=0;
    io_enter_error=1;assert(!sector_io(&_msd_device,0,b,1,1)&&!raw_ios&&!io_depth);
    assert(spi_depth==0);
    puts("TF_POWER_CID_GEOMETRY_REPLACEMENT_FAIL_CLOSED_WRITE_GATE_PASS");
}
'''
# Prevent the filesystem -> block-device -> shared lock order from being
# inverted by display or whole filesystem shell commands.
display=(root/'epd_e0470a03.c').read_text(encoding='utf8')
locks=display[display.index('static void lock_devices'):display.index('static unsigned refresh_mode')]
assert 'tf_card_display_lock();' not in locks and 'tf_card_display_unlock();' not in locks
tf=(root/'tf_card.c').read_text(encoding='utf8')
command=tf[tf.index('static int tf(int'):tf.index('MSH_CMD_EXPORT(tf,')]
assert 'board527_io_enter' not in command
pmtest=tf[tf.index('static int power_test'):tf.index('static int tf(int')]
assert 'unmount_card(' not in pmtest
with tempfile.TemporaryDirectory(prefix='tf-power-') as d:
    d=Path(d)
    for name,source in [('rail',rail),('card',card)]:
        cpp=d/(name+'.cpp');exe=d/(name+'.exe');cpp.write_text(source,encoding='utf8')
        subprocess.run([a.cxx,'-O2',str(cpp),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True,timeout=10)
