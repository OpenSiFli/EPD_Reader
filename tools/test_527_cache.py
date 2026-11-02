"""Replay packed-row cache against the waveform LUT, including dense gray data.

Uses the actual C routines from the driver, compiled with the host gcc. The test
checks every expanded byte, raw/RLE records, hash reuse and bounded overflow.
"""
from pathlib import Path
import argparse,struct,subprocess,tempfile

p=argparse.ArgumentParser();p.add_argument('--gcc',default='gcc')
p.add_argument('--mixed',type=Path,help='Optional 1216x684 old<<4|new transition bytes from a real page pair')
a=p.parse_args()
root=Path(__file__).resolve().parents[1]
driver=(root/'epdiy-epub/src/boards/sf32lb527/epd_e0470a03.c').read_text()
section=driver[driver.index('/* Every waveform phase'):driver.index('static int drive')]
policy=driver[driver.index('static unsigned refresh_mode'):driver.index('int board527_display_present')]
present=driver[driver.index('int board527_display_present'):driver.index('int board527_display_init')]
header=r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#define W 1216
#define H 684
#define DATA_BYTES (W/4)
#define ROW_BYTES (DATA_BYTES+12)
#define CACHE_BYTES (3u*1024*1024)
#define MAX_ROWS 32768
#define HASH_SLOTS 65536
#define MAX_PHASES 64
#define DICT_INDEX_BYTES (W*H/2)
#define ALIGN(n)
#define RT_NULL NULL
static uint8_t mixed[W*H],scan_cache[CACHE_BYTES];
static uint16_t phase_rows[64][H],row_slots[HASH_SLOTS],row_map[H],unique_y[H];
static uint32_t row_offsets[MAX_ROWS],row_hash[H],lut[256];
static unsigned unique_rows,cached_rows,cache_used;
static unsigned raw_cache;
static unsigned dictionary_mode,dictionary_count;
static unsigned packed_rows,reused_rows;
static void *extra_allocation;
static uint8_t *extra_cache;
static unsigned extra_bytes;
static size_t available_heap=3u*1024*1024;
static size_t epub_mem_available_size(void){return available_heap;}
static void *epub_mem_malloc(size_t n){return malloc(n);}
static void epub_mem_free(void *p){free(p);}
static uint8_t phase_luts[64][256];
static uint32_t pair_words[65536/4];
static uint8_t phases[MAX_PHASES][64];
static int prepared;
static unsigned prepared_mode,prepared_count,prepared_index,prepared_ms;
static int (*prepare_cancel)(void *);
static void *prepare_context;
static int prepare_cancelled;
static void mpu_dcache_clean(void *p,unsigned n){(void)p;(void)n;}
static void waveform_bin_reader_fill_lut(uint32_t *out,unsigned f)
{
    for(unsigned i=0;i<256;++i)out[i]=((phases[f][i/4]>>(6-2*(i%4)))&3)<<3;
}
'''
workspace=driver[driver.index('typedef union {'):driver.index('L2_NON_RET_BSS_SECT_BEGIN')]
header+=workspace+'\nstatic PhaseWorkspace workspace;\n#define full_frame workspace.frame\n'
header+=driver[driver.index('static int preparation_cancelled'):driver.index('L1_RET_CODE_SECT(epd527, static void pin')]
present_stubs=r'''
#define RT_WAITING_FOREVER -1
#define RT_ENOMEM 5
#define RT_EIO 6
static int io_failure,io_votes;
static int board527_io_enter(void){if(io_failure)return -1;++io_votes;return 0;}
static void board527_io_exit(void){assert(io_votes>0);--io_votes;}
typedef unsigned WAVE_TABLE_MODE_T;
static unsigned loaded_count,temperature=28,ticks;
static unsigned waveform_bin_reader_get_frames(unsigned t,unsigned m){(void)t;(void)m;return loaded_count;}
static unsigned rt_tick_get(void){return ++ticks;}
static unsigned HAL_DBG_DWT_GetCycles(void){return ++ticks;}
#define rt_kprintf(...) ((void)0)
static int ready=1,previous_known,lock,drive_result;
static unsigned full_period=10,refresh_index,drive_calls,drive_mode;
static void rt_mutex_take(int *p,int wait){(void)wait;++*p;}
static void rt_mutex_release(int *p){--*p;}
static void lock_devices(void){}
static void unlock_devices(void){}
static void release_extra_cache(void);
static int drive(unsigned mode){++drive_calls;drive_mode=mode;prepared=0;release_extra_cache();return drive_result;}
static uint8_t requested[W*H/2];
static int cancellation_checks;
static int cancel_after(void *arg){return ++cancellation_checks>*(int *)arg;}
static int cancel_pulse(void *arg){return ++cancellation_checks==*(int *)arg;}
'''
main=r'''
int main(int argc,char **argv)
{
    /* All 65,536 transition pairs, not only pairs in the sample images. */
    for(unsigned i=0;i<256;++i)phase_luts[0][i]=(i^(i>>3))&3;
    make_pair_table(phase_luts[0]);
    for(unsigned i=0;i<65536;++i)
        assert(((uint8_t *)pair_words)[i]==(phase_luts[0][i&255]<<2|phase_luts[0][i>>8]));
    unsigned random_codec=0x527e0470u;
    for(unsigned test=0;test<5000;++test) {
        uint8_t raw[DATA_BYTES],encoded[DATA_BYTES*2+2],decoded[DATA_BYTES+2];
        for(unsigned i=0;i<DATA_BYTES;++i) {
            random_codec^=random_codec<<13;random_codec^=random_codec>>17;random_codec^=random_codec<<5;
            raw[i]=(test%3&&i%17<test%17)?0:(random_codec&255);
        }
        unsigned size=encode_row(raw,encoded+2);encoded[0]=size;encoded[1]=size>>8;
        memset(decoded,0xa5,sizeof(decoded));expand_row(encoded,decoded+1);
        assert(!memcmp(raw,decoded+1,DATA_BYTES)&&decoded[0]==0xa5&&decoded[DATA_BYTES+1]==0xa5);
    }
    puts("PAIR_LUT_EXHAUSTIVE_AND_CODEC_FUZZ_PASS");
    memset(mixed,255,sizeof(mixed));memset(requested,255,sizeof(requested));
    io_failure=1;assert(board527_display_present(requested)==-RT_EIO&&lock==0&&drive_calls==0&&io_votes==0);io_failure=0;
    previous_known=1;refresh_index=1;requested[0]=0xf7;drive_result=-RT_ENOMEM;
    assert(board527_display_present(requested)==-RT_ENOMEM);
    assert(previous_known&&refresh_index==1&&lock==0&&drive_calls==1&&drive_mode==5);
    for(unsigned i=0;i<W*H;++i)assert((mixed[i]&15)==15);
    drive_result=0;
    assert(board527_display_present(requested)==0&&drive_calls==2&&refresh_index==2);
    assert((mixed[0]&15)==7);
    assert(board527_display_present(requested)==0&&drive_calls==2);
    previous_known=0;refresh_index=0;memset(requested,255,sizeof(requested));
    assert(board527_display_present(requested)==0&&drive_calls==3&&drive_mode==3);
    assert(previous_known&&lock==0);
    puts("PRESENT_STATE_AND_SKIP_PASS");
    for(unsigned period=5;period<=20;period*=2)for(unsigned index=0;index<=2*period;++index) {
        for(int gray=0;gray<4;++gray)
            assert(refresh_mode(period,index,gray&1,gray&2)==
                (index%period==0?0:gray?5:1));
        assert(refresh_mode(0,index,1,1)==0);
    }
    puts("REFRESH_POLICY_PASS");
    assert(argc==2||argc==3);FILE *wave=fopen(argv[1],"rb");assert(wave);
    unsigned count;assert(fread(&count,1,4,wave)==4&&count<=MAX_PHASES);
    assert(fread(phases,64,count,wave)==count);fclose(wave);
    loaded_count=count;
    memset(mixed,255,sizeof(mixed));memset(requested,0x77,sizeof(requested));
    previous_known=1;refresh_index=1;unsigned calls=drive_calls;
    assert(board527_display_prepare(requested,NULL,NULL)==0&&prepared&&drive_calls==calls);
    for(unsigned i=0;i<W*H;++i)assert(mixed[i]==0xf7);
    assert(board527_display_present(requested)==0&&!prepared&&drive_calls==calls+1);
    for(unsigned i=0;i<W*H;++i)assert((mixed[i]&15)==7);
    memset(requested,0x88,sizeof(requested));assert(board527_display_prepare(requested,NULL,NULL)==0);
    /* Backward/menu render differs from candidate: old pixel must remain 7. */
    memset(requested,0x33,sizeof(requested));assert(board527_display_present(requested)==0&&!prepared);
    for(unsigned i=0;i<W*H;++i)assert(mixed[i]==0x73);
    memset(requested,0xaa,sizeof(requested));cancellation_checks=0;int cancel_limit=50;
    assert(board527_display_prepare(requested,cancel_after,&cancel_limit)!=0&&!prepared);
    for(unsigned i=0;i<W*H;++i)assert((mixed[i]&15)==3);
    assert(!extra_cache&&!extra_allocation&&!prepare_cancel);
    cancellation_checks=0;
    assert(board527_display_prepare(requested,cancel_pulse,&cancel_limit)!=0&&!prepared);
    for(unsigned i=0;i<W*H;++i)assert((mixed[i]&15)==3);
    assert(board527_display_prepare(requested,NULL,NULL)==0);
    board527_display_cancel_prepare();
    for(unsigned i=0;i<W*H;++i)assert((mixed[i]&15)==3);
    assert(!prepared&&!extra_cache&&lock==0);
    puts("PREFETCH_HIT_MISS_CANCEL_STATE_PASS");
    for(unsigned test=0;test<(argc==3?7u:6u);++test) {
        unsigned random=0x527e0470u;
        for(unsigned y=0;y<H;++y)for(unsigned x=0;x<W;++x) {
            unsigned value=test==0?255:test==1?((x/76)<<4)|((y/43)&15):
                (((x/16+y/20)&15)<<4)|((x/23+y/31)&15);
            if(test==3) {
                unsigned old=(x*13+y*7)%W<60?(x+y*31)&15:15;
                unsigned next=(x*17+y*11)%W<80?(x/3+y*41)&15:15;
                value=(old<<4)|next;
            }
            if(test==4||test==5) {
                random^=random<<13;random^=random>>17;random^=random<<5;
                value=test==5&&x>=500?255:random&255;
            }
            mixed[y*W+x]=value;
        }
        if(test==6) {
            FILE *capture=fopen(argv[2],"rb");assert(capture);
            assert(fread(mixed,1,sizeof(mixed),capture)==sizeof(mixed));fclose(capture);
        }
        int rc=prepare_phases(count);
        if(rc){assert(test==4&&cache_used<=CACHE_BYTES+extra_bytes);printf("CACHE_BOUNDED_REJECT case=%u bytes=%u\n",test,cache_used);release_extra_cache();continue;}
        memset(full_frame,0,sizeof(full_frame));
        for(unsigned f=0;f<count;++f) {
            expand_phase(f);waveform_bin_reader_fill_lut(lut,f);
            for(unsigned y=0;y<H;++y) {
                for(unsigned x=0;x<DATA_BYTES;++x) {
                    uint8_t expected=0;
                    for(unsigned k=0;k<4;++k)expected|=((lut[mixed[y*W+4*x+k]]>>3)&3)<<(6-2*k);
                    assert(full_frame[y*ROW_BYTES+x]==expected);
                }
                for(unsigned x=DATA_BYTES;x<ROW_BYTES;++x)assert(full_frame[y*ROW_BYTES+x]==0);
            }
        }
        promote_raw_cache();
        if(raw_cache)for(unsigned f=0;f<count;++f) {
            waveform_bin_reader_fill_lut(lut,f);
            for(unsigned y=0;y<H;++y)for(unsigned x=0;x<DATA_BYTES;++x) {
                uint8_t expected=0;
                for(unsigned k=0;k<4;++k)expected|=((lut[mixed[y*W+4*x+k]]>>3)&3)<<(6-2*k);
                assert(raw_row(phase_rows[f][y])[x]==expected);
            }
        }
        printf("CACHE_REPLAY_PASS case=%u rows=%u bytes=%u raw=%u dictionary=%u words=%u packed=%u reused=%u\n",test,cached_rows,cache_used,raw_cache,dictionary_mode,dictionary_count,packed_rows,reused_rows);
        release_extra_cache();assert(!extra_cache&&!extra_allocation&&!extra_bytes);
    }
    /* Old 512KiB allocation steps discarded nearly half a MiB in this case. */
    available_heap=2u*1024*1024-4096;
    assert(extend_cache(1200u*1024)==0&&extra_bytes>=1200u*1024);
    assert(extra_bytes+512u*1024+63<=available_heap);
    release_extra_cache();
    available_heap=512u*1024; /* Reserved application heap must not be borrowed. */
    cached_rows=0;cache_used=CACHE_BYTES;memset(row_slots,0,sizeof(row_slots));uint8_t row[DATA_BYTES];memset(row,0xa5,sizeof(row));
    assert(cache_row(row)==-1&&cache_used==CACHE_BYTES);
    puts("CACHE_OVERFLOW_PASS");return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='reader527-cache-') as temp:
    path=Path(temp);c=path/'cache_test.c';exe=path/'cache_test.exe';c.write_text(header+present_stubs+section+policy+present+main)
    subprocess.run([a.gcc,'-O2','-Wall','-Wextra',str(c),'-o',str(exe)],check=True)
    wave=(root/'epdiy-epub/waveform/e0470a03/epd_waveform.bin').read_bytes()
    for mode in (0,1,3,5):
        zones,offset=struct.unpack_from('<II',wave,8+8*mode)
        for zone in range(zones):
            low,high,count,table=struct.unpack_from('<iiII',wave,offset+16*zone)
            if low<=28<high:break
        else:raise AssertionError(f'Missing temperature zone for mode {mode}')
        data=path/f'mode{mode}.bin';data.write_bytes(struct.pack('<I',count)+wave[table:table+64*count])
        if mode==5:
            assert all(wave[table+64*f+63]&3==0 for f in range(count)), 'GL16 must leave white-to-white undriven'
        print(f'MODE_REPLAY mode={mode} frames={count}',flush=True)
        subprocess.run([str(exe),str(data)]+([str(a.mixed.resolve())] if a.mixed else []),check=True)
