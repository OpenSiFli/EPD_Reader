/* SPDX-License-Identifier: Apache-2.0
 * SF32LB527 / OED v1.2 / E0470A03-AF-S. DBI shifts data; GPIO scans gates.
 * Updates track old/new gray levels and leave panel HV off after scanning.
 */
#include <rtthread.h>
#include <rtdevice.h>
#ifndef READER527_RECOVERY_DISPLAY
#include <finsh.h>
#else
#define MSH_CMD_EXPORT(name,description)
#endif
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "drv_flash.h"
#include "mem_section.h"
#include "mem_map.h"
#include "epd_tps.h"
#include "epd_waveform_bin_reader.h"
#define WAVE_BYTES 223240u
#define WAVE_FNV 0x1a2e9bf7u
#include "board527.h"
#include "frontlight.h"
#include "tf_card.h"
#include "epub_mem.h"
#ifdef READER527_RECOVERY_DISPLAY
/* Recovery must work even while the application waveform partition is torn. */
extern const uint8_t reader_recovery_wave[WAVE_BYTES];
#endif

#define W 1216
#define H 684
#define DATA_BYTES (W / 4)
#define ROW_BYTES (DATA_BYTES + 12)
#define CACHE_BYTES (3u * 1024 * 1024)
#define MAX_PHASES 64
#define MAX_ROWS 32768
#define HASH_SLOTS 65536
/* Preparation and scanning never overlap. Reuse the expansion buffer for
 * exact per-row waveform equivalence checks, without increasing PSRAM use. */
typedef union {
    uint8_t frame[H * ROW_BYTES];
    struct {
        uint32_t masks[H][8];
    } prep;
} PhaseWorkspace;
L2_NON_RET_BSS_SECT_BEGIN(epd_test)
L2_NON_RET_BSS_SECT(epd_test, ALIGN(64) static uint8_t mixed[W * H]);
L2_NON_RET_BSS_SECT(epd_test, ALIGN(64) static uint8_t reader_frame[W * H / 2]);
L2_NON_RET_BSS_SECT(epd_test, ALIGN(64) static uint8_t scan_cache[CACHE_BYTES]);
L2_NON_RET_BSS_SECT(epd_test, ALIGN(64) static PhaseWorkspace workspace);
L2_NON_RET_BSS_SECT(epd_test, static uint16_t phase_rows[MAX_PHASES][H]);
L2_NON_RET_BSS_SECT(epd_test, static uint32_t row_offsets[MAX_ROWS]);
L2_NON_RET_BSS_SECT(epd_test, static uint16_t row_slots[HASH_SLOTS]);
L2_NON_RET_BSS_SECT_END
/* Small hot tables belong in internal SRAM, not the external row store. */
static uint8_t phase_luts[MAX_PHASES][256];
ALIGN(32) static uint32_t pair_words[65536/4];
static unsigned dictionary_mode,dictionary_count;
#define DICT_INDEX_BYTES (W*H/2)
#define full_frame workspace.frame
static LCDC_HandleTypeDef lcd;
static uint32_t lut[256];
static struct rt_mutex lock;
static struct rt_timer cutoff;
static volatile int aborted;
/* COM34 optical comparison: gatepol=0 displays normally; gatepol=1 failed. */
static int ready, previous_known, gatepol = 0, temperature = 28;
static unsigned full_period=10,refresh_index,cached_rows,cache_used,active_phase,raw_cache;
static unsigned packed_rows,reused_rows;
static void *extra_allocation;
static uint8_t *extra_cache;
static unsigned extra_bytes;
static uint8_t *raw_row(unsigned row);
static int lcd_ready;
static unsigned draws, frames_total, errors, last_frames;
static uint32_t last_ms;
static unsigned source_mhz=48, gate_us=1, unique_rows;
static unsigned packed_dummy=1;
static unsigned pipeline=1;
ALIGN(32) static uint8_t neutral_row[ROW_BYTES];
static uint16_t row_map[H], unique_y[H];
static uint32_t row_hash[H];
static uint32_t last_scan_us;
static int prepared;
static unsigned prepared_mode,prepared_count,prepared_index,prepared_ms;
static int (*prepare_cancel)(void *);
static void *prepare_context;
static int prepare_cancelled;
static int preparation_cancelled(void)
{
    // A short contact can end between checks. Once cancelled, never resume a
    // partially generated row map/cache during the same preparation attempt.
    if(prepare_cancel&&prepare_cancel(prepare_context))prepare_cancelled=1;
    return prepare_cancelled;
}

L1_RET_CODE_SECT(epd527, static void pin(unsigned n, unsigned v))
{
    GPIO1_TypeDef *gpio = (GPIO1_TypeDef *)hwp_gpio1;
    if (n < 32) {
        if (v) gpio->DOSR0 = 1u << n;
        else gpio->DOCR0 = 1u << n;
    } else {
        if (v) gpio->DOSR1 = 1u << (n - 32);
        else gpio->DOCR1 = 1u << (n - 32);
    }
}
L1_RET_CODE_SECT(epd527, static void gate(int active)) { pin(EPD_CPV, active ^ gatepol); }
static void idle(void)
{
    pin(EPD_OE, 0);
    pin(EPD_LE, 0);
    pin(EPD_GMODE, 0);
    pin(EPD_STV, 1);
    gate(0);
}
static void power_cutoff(void *arg)
{
    (void)arg;
    aborted = 1;
    pin(EPD_OE, 0);
    HAL_Delay_us(20);
    pin(3, 0); /* AW reset releases GE6 to SY7636 EN pulldown. No I2C in ISR. */
    pin(EPD_GMODE, 0);
}
void *waveform_bin_reader_malloc(size_t n) { return rt_malloc(n); }
void waveform_bin_reader_free(void *p) { rt_free(p); }
int waveform_bin_reader_read_data(uint32_t off, uint8_t *buf, uint32_t size)
{
    if (off > WAVE_BYTES || size > WAVE_BYTES - off) return -1;
#ifdef READER527_RECOVERY_DISPLAY
    memcpy(buf,reader_recovery_wave+off,size);return size;
#else
    return rt_flash_read(CUSTOM_EPD_WAVE_TABLE_START_ADDR + off, buf, size);
#endif
}

static int setup_lcd(void)
{
    unsigned transfer_bytes=DATA_BYTES+(packed_dummy?12:0);
    memset(&lcd, 0, sizeof(lcd));
    lcd.Instance = hwp_lcdc1;
    lcd.Init.lcd_itf = LCDC_INTF_DBI_8BIT_B;
    lcd.Init.freq = source_mhz * 1000000;
    lcd.Init.color_mode = LCDC_PIXEL_FORMAT_RGB332;
    lcd.Init.cfg.dbi.syn_mode = HAL_LCDC_SYNC_DISABLE;
    HAL_RCC_EnableModule(RCC_MOD_LCDC1);
    if (HAL_LCDC_Init(&lcd) != HAL_OK) return -1;
    /* Same raw RGB332 byte transport as the original OED v1.2 driver. */
    lcd.Instance->LAYER0_CONFIG =
        (4u << LCD_IF_LAYER0_CONFIG_FORMAT_Pos) |
        (1u << LCD_IF_LAYER0_CONFIG_ALPHA_SEL_Pos) |
        (255u << LCD_IF_LAYER0_CONFIG_ALPHA_Pos) |
        (transfer_bytes << LCD_IF_LAYER0_CONFIG_WIDTH_Pos) |
        (1u << LCD_IF_LAYER0_CONFIG_ACTIVE_Pos);
    lcd.Instance->LAYER0_TL_POS = 0;
    lcd.Instance->LAYER0_BR_POS = (transfer_bytes - 1) << LCD_IF_LAYER0_BR_POS_X1_Pos;
    lcd.Instance->CANVAS_TL_POS = 0;
    lcd.Instance->CANVAS_BR_POS = (transfer_bytes - 1) << LCD_IF_CANVAS_BR_POS_X1_Pos;
    lcd.Instance->LCD_WR = 0;
    return 0;
}

L1_RET_CODE_SECT(epd527, static int wait_idle(int single))
{
    uint32_t start = HAL_DBG_DWT_GetCycles();
    while (single ? (lcd.Instance->LCD_SINGLE & LCD_IF_LCD_SINGLE_LCD_BUSY) :
                    (lcd.Instance->STATUS & LCD_IF_STATUS_LCD_BUSY)) {
        if (aborted || HAL_DBG_DWT_GetCycles() - start > 2400000u) return -1;
    }
    return aborted ? -1 : 0;
}
L1_RET_CODE_SECT(epd527, static int clocks(unsigned count, int keep_stl_high))
{
    while (count--) {
        lcd.Instance->LCD_SINGLE = LCD_IF_LCD_SINGLE_WR_TRIG |
            (keep_stl_high ? LCD_IF_LCD_SINGLE_TYPE_Msk : 0);
        if (wait_idle(1)) return -1;
    }
    return 0;
}
L1_RET_CODE_SECT(epd527, static void gate_pulse(void))
{
    gate(1); HAL_Delay_us(gate_us);
    gate(0); HAL_Delay_us(gate_us);
}
L1_RET_CODE_SECT(epd527, static const uint8_t *row_source(unsigned y))
{
    return raw_cache?raw_row(phase_rows[active_phase][y]):full_frame+y*ROW_BYTES;
}
L1_RET_CODE_SECT(epd527, __attribute__((noinline)) static int scan_frame(void))
{
    pin(EPD_OE, 0);
    pin(EPD_GMODE, 0); HAL_Delay_us(6);
    pin(EPD_GMODE, 1); HAL_Delay_us(6);
    pin(EPD_STV, 0); HAL_Delay_us(6);
    gate(1); HAL_Delay_us(6);
    pin(EPD_STV, 1);
    gate(0); HAL_Delay_us(6);
    for (unsigned i = 0; i < 3; ++i) gate_pulse();

    if(pipeline) {
        /* Preload first source row while OE is low, then latch it. Keep each
         * gate selected while shifting the following row, as the 52X DBI
         * reference does. The final neutral transfer gives the last real row
         * the same selected duration as every other row. */
        if(clocks(1,0))return -1;
        lcd.Instance->LAYER0_SRC=(uint32_t)(row_source(0));
        lcd.Instance->COMMAND=1;
        if(wait_idle(0)||(!packed_dummy&&clocks(12,1)))return -1;
        for(unsigned y=0;y<H;++y) {
            pin(EPD_LE,1);HAL_Delay_us(1);
            pin(EPD_LE,0);HAL_Delay_us(1);
            pin(EPD_OE,1);
            if(clocks(1,0))return -1;
            gate(1);
            lcd.Instance->LAYER0_SRC=(uint32_t)(y+1<H?
                row_source(y+1):neutral_row);
            lcd.Instance->COMMAND=1;
            if(wait_idle(0)||(!packed_dummy&&clocks(12,1)))return -1;
            gate(0);HAL_Delay_us(gate_us);
        }
    } else {
    for (unsigned y = 0; y < H; ++y) {
        if (aborted || clocks(1, 0)) return -1;
        lcd.Instance->LAYER0_SRC = (uint32_t)(row_source(y));
        lcd.Instance->COMMAND = 1;
        if (wait_idle(0) || (!packed_dummy && clocks(12, 1))) return -1;
        /* >=10 dummy source clocks after data, LE >=300 ns,
         * and >=800 ns before next STL, per panel specification. */
        pin(EPD_LE, 1); HAL_Delay_us(1);
        pin(EPD_LE, 0); HAL_Delay_us(1);
        pin(EPD_OE, 1);
        gate_pulse();
    }
    }
    pin(EPD_OE, 0);
    for (unsigned i = 0; i < 5; ++i) gate_pulse();
    return 0;
}


/* Every waveform phase is prepared before HV. Identical packed rows share
 * storage across phases; RLE compresses uniform areas. Each full frame is
 * expanded once and scanned over the entire panel, never in visible tiles. */
static uint32_t prep_cycles[7];
static void report_preparation(void)
{
#if BOARD527_VERBOSE_LOGS
    rt_kprintf("EPD527_PROFILE map_us=%u lut_us=%u reuse_us=%u pack_us=%u encode_us=%u lookup_us=%u promote_us=%u\n",
        prep_cycles[0]/240,prep_cycles[1]/240,prep_cycles[2]/240,prep_cycles[3]/240,
        prep_cycles[4]/240,prep_cycles[5]/240,prep_cycles[6]/240);
#endif
}
static int extend_cache(unsigned needed)
{
    if(extra_cache)return needed<=extra_bytes?0:-1;
    /* Borrow only while presenting an already-rendered page. Keep at least
     * 512KiB free for background layout and fonts; never resize live data. */
    unsigned available=epub_mem_available_size();
    const unsigned reserve=512u*1024+63;
    unsigned budget=available>reserve?available-reserve:0;
    if(budget>2u*1024*1024)budget=2u*1024*1024;
    budget&=~(64u*1024-1);
    for(unsigned size=budget;size>=64u*1024;size-=64u*1024) {
        if(size<needed)break;
        extra_allocation=epub_mem_malloc(size+63);
        if(!extra_allocation)continue;
        extra_cache=(uint8_t *)(((uintptr_t)extra_allocation+63)&~(uintptr_t)63);
        extra_bytes=size;return 0;
    }
    return -1;
}
static void release_extra_cache(void)
{
    if(extra_allocation)epub_mem_free(extra_allocation);
    extra_allocation=RT_NULL;extra_cache=RT_NULL;extra_bytes=0;
}
static uint8_t *cache_address(unsigned offset)
{
    return offset<CACHE_BYTES?scan_cache+offset:extra_cache+offset-CACHE_BYTES;
}
static uint8_t *raw_row(unsigned row)
{
    unsigned base_rows=CACHE_BYTES/ROW_BYTES;
    return row<base_rows?scan_cache+row*ROW_BYTES:extra_cache+(row-base_rows)*ROW_BYTES;
}
static void map_rows(void)
{
    unique_rows=0;
    memset(workspace.prep.masks,0,sizeof(workspace.prep.masks));
    for(unsigned y=0;y<H;++y) {
        if(preparation_cancelled())return;
        const uint8_t *row=mixed+y*W;uint32_t hash=2166136261u;
        const uint32_t *words=(const uint32_t *)row;
        for(unsigned x=0;x<W/4;++x)hash=(hash^words[x])*16777619u;
        unsigned n;
        for(n=0;n<unique_rows;++n)
            if(row_hash[n]==hash&&!memcmp(row,mixed+unique_y[n]*W,W))break;
        if(n==unique_rows) {
            row_hash[n]=hash;unique_y[n]=y;++unique_rows;
            uint32_t mask[8]={0};
            for(unsigned x=0;x<W;++x)
                mask[row[x]>>5]|=1u<<(row[x]&31);
            memcpy(workspace.prep.masks[n],mask,sizeof(mask));
        }
        row_map[y]=n;
    }
}

#if defined(__GNUC__) && !defined(__clang__)
#define PREP_FAST __attribute__((optimize("O3"),noinline))
#else
#define PREP_FAST
#endif
/* Separate hot loops avoid the size-optimized caller's register spills.
 * GPIO/LCDC scan code retains its electrically verified compiler settings. */
static PREP_FAST void make_pair_table(const uint8_t *table)
{
    uint32_t low[64];
    for(unsigned i=0;i<64;++i)
        low[i]=(table[4*i]<<2)|(table[4*i+1]<<10)|
               (table[4*i+2]<<18)|(table[4*i+3]<<26);
    for(unsigned high=0;high<256;++high) {
        uint32_t repeated=table[high]*0x01010101u;
        for(unsigned i=0;i<64;++i)pair_words[high*64+i]=low[i]|repeated;
    }
}
static PREP_FAST void pack_row(const uint8_t *source,uint8_t *out)
{
    const uint16_t *pairs=(const uint16_t *)source;
    const uint8_t *table=(const uint8_t *)pair_words;
    for(unsigned x=0;x<DATA_BYTES;++x)
        out[x]=(table[pairs[2*x]]<<4)|table[pairs[2*x+1]];
}
static inline uint32_t row_word(const uint8_t *p)
{
    uint32_t word;memcpy(&word,p,sizeof(word));return word;
}
static PREP_FAST unsigned encode_row(const uint8_t *raw,uint8_t *encoded)
{
    unsigned size=0;
    for(unsigned i=0;i<DATA_BYTES;) {
        unsigned run=1;
        while(i+run<DATA_BYTES&&raw[i+run]==raw[i]&&run<128)++run;
        if(run>=3) {
            encoded[size++]=0x80|(run-1);encoded[size++]=raw[i];i+=run;
        } else {
            unsigned first=i;
            do {
                // Test four possible three-byte runs at once. Word loads use
                // memcpy so this is valid for unaligned literals on host too.
                if(i+5<DATA_BYTES&&i-first<=124) {
                    uint32_t a=row_word(raw+i),b=row_word(raw+i+1),c=row_word(raw+i+2);
                    uint32_t unequal=(a^b)|(b^c);
                    if(!((unequal-0x01010101u)&~unequal&0x80808080u)) {
                        i+=4;
                        if(i-first==128)break;
                        continue;
                    }
                }
                ++i;
                if(i>=DATA_BYTES||i-first==128)break;
            } while(!(i+2<DATA_BYTES&&raw[i]==raw[i+1]&&raw[i]==raw[i+2]));
            encoded[size++]=i-first-1;
            memcpy(encoded+size,raw+first,i-first);size+=i-first;
        }
    }
    return size;
}
static int cache_row(const uint8_t *raw)
{
    uint32_t begin=HAL_DBG_DWT_GetCycles();
    uint8_t encoded[DATA_BYTES*2];unsigned size=encode_row(raw,encoded);
    const uint8_t *data=encoded;unsigned tag=size;
    if(size>=DATA_BYTES){data=raw;size=DATA_BYTES;tag=size|0x8000;}
    prep_cycles[4]+=HAL_DBG_DWT_GetCycles()-begin;begin=HAL_DBG_DWT_GetCycles();
    uint32_t hash=2166136261u;
    for(unsigned i=0;i<size;++i)hash=(hash^data[i])*16777619u;
    unsigned slot=hash&(HASH_SLOTS-1);
    while(row_slots[slot]) {
        unsigned id=row_slots[slot]-1;
        const uint8_t *stored=cache_address(row_offsets[id]);
        unsigned stored_tag=stored[0]|stored[1]<<8;
        if(stored_tag==tag&&!memcmp(stored+2,data,size)) {
            prep_cycles[5]+=HAL_DBG_DWT_GetCycles()-begin;return id;
        }
        slot=(slot+1)&(HASH_SLOTS-1);
    }
    if(cached_rows>=MAX_ROWS-1)return -1;
    /* Encoded records must not straddle the two independently allocated banks. */
    if(cache_used<CACHE_BYTES&&size+2>CACHE_BYTES-cache_used)cache_used=CACHE_BYTES;
    if(cache_used+size+2>CACHE_BYTES+extra_bytes &&
       extend_cache(cache_used+size+2-CACHE_BYTES))return -1;
    unsigned id=cached_rows++;row_offsets[id]=cache_used;row_slots[slot]=id+1;
    uint8_t *out=cache_address(cache_used);out[0]=tag;out[1]=tag>>8;
    memcpy(out+2,data,size);cache_used+=size+2;
    prep_cycles[5]+=HAL_DBG_DWT_GetCycles()-begin;return id;
}
/* A display byte represents four old/new transitions (one 32-bit input).
 * Cache the waveform for each distinct word once, not once per row/phase.
 * row_offsets/row_slots are temporary key/hash storage until scanning. */
static PREP_FAST int prepare_dictionary(unsigned count)
{
    uint32_t start=HAL_DBG_DWT_GetCycles();
    const uint32_t *input=(const uint32_t *)mixed;
    uint16_t *indices=(uint16_t *)scan_cache;
    dictionary_mode=dictionary_count=0;
    memset(row_slots,0,sizeof(row_slots));
    for(unsigned y=0;y<H;++y) {
        if(preparation_cancelled())return -1;
        for(unsigned x=0;x<DATA_BYTES;++x) {
            unsigned at=y*DATA_BYTES+x;uint32_t word=input[at];
            uint32_t hash=word;hash^=hash>>16;hash*=0x7feb352du;hash^=hash>>15;
            unsigned slot=hash&(HASH_SLOTS-1),id;
            while(row_slots[slot]&&row_offsets[row_slots[slot]-1]!=word)slot=(slot+1)&(HASH_SLOTS-1);
            if(row_slots[slot])id=row_slots[slot]-1;
            else {
                if(dictionary_count>=MAX_ROWS-1)return -1;
                id=dictionary_count++;row_offsets[id]=word;row_slots[slot]=id+1;
            }
            indices[at]=id;
        }
    }
    prep_cycles[0]+=HAL_DBG_DWT_GetCycles()-start;
    unsigned stride=(dictionary_count+3)&~3u;
    if(stride>(CACHE_BYTES-DICT_INDEX_BYTES)/count)return -1;
    for(unsigned f=0;f<count;++f) {
        if(preparation_cancelled())return -1;
        start=HAL_DBG_DWT_GetCycles();
        waveform_bin_reader_fill_lut(lut,f);
        for(unsigned i=0;i<256;++i)phase_luts[f][i]=(lut[i]>>3)&3;
        prep_cycles[1]+=HAL_DBG_DWT_GetCycles()-start;
        start=HAL_DBG_DWT_GetCycles();
        uint8_t *dest=scan_cache+DICT_INDEX_BYTES+stride*f;
        const uint8_t *table=phase_luts[f];
        for(unsigned i=0;i<dictionary_count;++i) {
            uint32_t word=row_offsets[i];
            dest[i]=(table[word&255]<<6)|(table[(word>>8)&255]<<4)|
                    (table[(word>>16)&255]<<2)|table[word>>24];
        }
        prep_cycles[3]+=HAL_DBG_DWT_GetCycles()-start;
    }
    dictionary_mode=1;raw_cache=0;cached_rows=0;
    cache_used=DICT_INDEX_BYTES+stride*count;
    packed_rows=dictionary_count;reused_rows=0;unique_rows=H;
    return 0;
}
static int prepare_row_phases(unsigned count)
{
    cached_rows=cache_used=packed_rows=reused_rows=0;
    memset(prep_cycles,0,sizeof(prep_cycles));uint32_t tick=HAL_DBG_DWT_GetCycles();
    memset(row_slots,0,sizeof(row_slots));map_rows();
    prep_cycles[0]+=HAL_DBG_DWT_GetCycles()-tick;
    if(preparation_cancelled())return -1;
    uint32_t planes[MAX_PHASES][16];
    tick=HAL_DBG_DWT_GetCycles();
    for(unsigned f=0;f<count;++f) {
        waveform_bin_reader_fill_lut(lut,f);
        memset(planes[f],0,sizeof(planes[f]));
        for(unsigned i=0;i<256;++i) {
            unsigned value=(lut[i]>>3)&3;phase_luts[f][i]=value;
            if(value&1)planes[f][i>>5]|=1u<<(i&31);
            if(value&2)planes[f][8+(i>>5)]|=1u<<(i&31);
        }
    }
    prep_cycles[1]+=HAL_DBG_DWT_GetCycles()-tick;
    for(unsigned r=0;r<unique_rows;++r) {
        uint32_t mask[8],hashes[MAX_PHASES];
        memcpy(mask,workspace.prep.masks[r],sizeof(mask));
        for(unsigned f=0;f<count;++f) {
            if(preparation_cancelled())return -1;
            tick=HAL_DBG_DWT_GetCycles();
            uint32_t hash=2166136261u;
            for(unsigned i=0;i<16;++i)hash=(hash^(planes[f][i]&mask[i&7]))*16777619u;
            hashes[f]=hash;unsigned prior;
            for(prior=0;prior<f;++prior) {
                if(hashes[prior]!=hash)continue;
                unsigned i;
                for(i=0;i<8;++i)
                    if(((planes[f][i]^planes[prior][i])|
                        (planes[f][i+8]^planes[prior][i+8]))&mask[i])break;
                if(i==8)break;
            }
            prep_cycles[2]+=HAL_DBG_DWT_GetCycles()-tick;
            if(prior<f) {
                phase_rows[f][unique_y[r]]=prior;
                ++reused_rows;continue;
            }
            phase_rows[f][unique_y[r]]=0xffff;
        }
    }
    // Phase-major conversion uses a 64KiB SRAM LUT for two transitions at a
    // time. Equivalence was already decided using each row's exact bit masks.
    for(unsigned f=0;f<count;++f) {
        tick=HAL_DBG_DWT_GetCycles();make_pair_table(phase_luts[f]);
        prep_cycles[1]+=HAL_DBG_DWT_GetCycles()-tick;
        for(unsigned r=0;r<unique_rows;++r) {
            if(preparation_cancelled())return -1;
            unsigned y=unique_y[r],prior=phase_rows[f][y];
            if(prior!=0xffff){phase_rows[f][y]=phase_rows[prior][y];continue;}
            ALIGN(4) uint8_t row[DATA_BYTES];
            tick=HAL_DBG_DWT_GetCycles();pack_row(mixed+y*W,row);
            prep_cycles[3]+=HAL_DBG_DWT_GetCycles()-tick;
            int id=cache_row(row);if(id<0)return -1;
            phase_rows[f][y]=id;++packed_rows;
        }
    }
    // unique_y selects the first occurrence, so filling duplicates cannot
    // overwrite a source entry. Every waveform phase still scans all 684 rows.
    for(unsigned f=0;f<count;++f)for(unsigned y=0;y<H;++y)
        phase_rows[f][y]=phase_rows[f][unique_y[row_map[y]]];
    return 0;
}
static int prepare_phases(unsigned count)
{
    memset(prep_cycles,0,sizeof(prep_cycles));
    if(!prepare_dictionary(count))return 0;
    dictionary_mode=0;
    if(preparation_cancelled())return -1;
    return prepare_row_phases(count);
}
static void expand_row(const uint8_t *in,uint8_t *out)
{
    unsigned tag=in[0]|in[1]<<8;in+=2;
    if(tag&0x8000)memcpy(out,in,DATA_BYTES);
    else {
        const uint8_t *end=in+tag;
        while(in<end) {
            unsigned control=*in++,n=(control&127)+1;
            if(control&128){memset(out,*in++,n);}
            else {memcpy(out,in,n);in+=n;}
            out+=n;
        }
    }
}
static void promote_raw_cache(void)
{
    if(dictionary_mode){raw_cache=0;return;}
    uint32_t begin=HAL_DBG_DWT_GetCycles();
    unsigned base_rows=CACHE_BYTES/ROW_BYTES;
    if(cached_rows>base_rows)extend_cache((cached_rows-base_rows)*ROW_BYTES);
    raw_cache=cached_rows<=base_rows+extra_bytes/ROW_BYTES;
    if(!raw_cache)return;
    uint8_t encoded[DATA_BYTES+2];
    /* Every encoded record is <=306 bytes, smaller than a 316-byte raw row.
     * Expand backwards; earlier compressed records can never be overwritten. */
    for(unsigned n=cached_rows;n>0;--n) {
        if(preparation_cancelled()){raw_cache=0;return;}
        unsigned row=n-1;const uint8_t *in=cache_address(row_offsets[row]);
        unsigned tag=in[0]|in[1]<<8;
        memcpy(encoded,in,(tag&0x7fff)+2);
        expand_row(encoded,raw_row(row));
        memset(raw_row(row)+DATA_BYTES,0,12);
    }
    mpu_dcache_clean(scan_cache,(cached_rows<base_rows?cached_rows:base_rows)*ROW_BYTES);
    if(cached_rows>base_rows)mpu_dcache_clean(extra_cache,(cached_rows-base_rows)*ROW_BYTES);
    prep_cycles[6]+=HAL_DBG_DWT_GetCycles()-begin;
}
static PREP_FAST void expand_dictionary_phase(unsigned phase)
{
    unsigned stride=(dictionary_count+3)&~3u;
    /* The per-phase byte table fits in internal SRAM. External memory traffic
     * is then sequential index reads and complete-frame writes. */
    uint8_t *table=(uint8_t *)pair_words;
    memcpy(table,scan_cache+DICT_INDEX_BYTES+stride*phase,dictionary_count);
    const uint16_t *indices=(const uint16_t *)scan_cache;
    for(unsigned y=0;y<H;++y) {
        uint32_t *out=(uint32_t *)(full_frame+y*ROW_BYTES);
        for(unsigned x=0;x<DATA_BYTES;x+=4) {
            *out++=table[indices[0]]|((uint32_t)table[indices[1]]<<8)|
                ((uint32_t)table[indices[2]]<<16)|((uint32_t)table[indices[3]]<<24);
            indices+=4;
        }
        out[0]=out[1]=out[2]=0;
    }
    mpu_dcache_clean(full_frame,sizeof(full_frame));
}
static void expand_phase(unsigned phase)
{
    if(dictionary_mode){expand_dictionary_phase(phase);return;}
    for(unsigned y=0;y<H;++y) {
        const uint8_t *in=cache_address(row_offsets[phase_rows[phase][y]]);
        expand_row(in,full_frame+y*ROW_BYTES);
    }
    mpu_dcache_clean(full_frame,sizeof(full_frame));
}
static int drive(unsigned mode)
{
    int hit=prepared&&prepared_mode==mode&&prepared_index==refresh_index;
    unsigned saved_ms=hit?prepared_ms:0;
    unsigned count=hit?prepared_count:waveform_bin_reader_get_frames(temperature,(WAVE_TABLE_MODE_T)mode);
    prepared=0;
    if(!count||count>MAX_PHASES)return -1;
    uint32_t begin=rt_tick_get(),scan_cycles=0;unsigned done=0;
#if BOARD527_VERBOSE_LOGS
    uint32_t expand_cycles=0,visible_ms=0,first_scan_ms=0;
#endif
    if(!hit&&prepare_phases(count)) {
        ++errors;rt_kprintf("EPD527_CACHE_FULL HV=OFF rows=%u bytes=%u\n",cached_rows,cache_used);
        release_extra_cache();return -RT_ENOMEM;
    }
    if(!hit){promote_raw_cache();report_preparation();}
#if BOARD527_VERBOSE_LOGS
    unsigned prep_ms=rt_tick_get()-begin;
#endif
    memset(full_frame,0,sizeof(full_frame));aborted=0;
    rt_timer_start(&cutoff);
    if(tps_exit_sleep()!=RT_EOK){aborted=1;goto cleanup;}
    for(unsigned f=0;f<count;++f) {
        active_phase=f;
#if BOARD527_VERBOSE_LOGS
        uint32_t expand_tick=HAL_DBG_DWT_GetCycles();
#endif
        if(!raw_cache)expand_phase(f);
#if BOARD527_VERBOSE_LOGS
        expand_cycles+=HAL_DBG_DWT_GetCycles()-expand_tick;
        if(f==0)first_scan_ms=rt_tick_get()-begin;
#endif
        uint32_t tick=HAL_DBG_DWT_GetCycles();
        rt_enter_critical();int rc=scan_frame();rt_exit_critical();
        scan_cycles+=HAL_DBG_DWT_GetCycles()-tick;
        if(rc){aborted=1;break;}++done;
    }
cleanup:
#if BOARD527_VERBOSE_LOGS
    visible_ms=rt_tick_get()-begin;
#endif
    idle();HAL_Delay_us(20);
    int power_rc=tps_enter_sleep();rt_timer_stop(&cutoff);
#if BOARD527_VERBOSE_LOGS
    unsigned borrowed=extra_bytes;
#endif
    release_extra_cache();
    if(aborted||power_rc){++errors;previous_known=0;rt_kprintf("EPD527_REFRESH_FAIL mode=%u frames=%u power_rc=%d aborted=%d\n",mode,done,power_rc,aborted);return -1;}
    ++draws;frames_total+=done;last_frames=done;last_ms=rt_tick_get()-begin;
    last_scan_us=done?scan_cycles/240/done:0;
#if BOARD527_VERBOSE_LOGS
    rt_kprintf("EPD527_REFRESH mode=%u frames=%u ms=%u prep_ms=%u scan_us=%u expand_us=%u errors=%u HV=OFF\n",
        mode,done,last_ms,prep_ms,last_scan_us,done?expand_cycles/240/done:0,errors);
    rt_kprintf("EPD527_CACHE input_rows=%u stored_rows=%u bytes=%u raw=%u full_screen=1\n",unique_rows,cached_rows,cache_used,raw_cache);
    rt_kprintf("EPD527_REUSE packed_rows=%u reused_rows=%u\n",packed_rows,reused_rows);
    rt_kprintf("EPD527_TIMING visible_ms=%u discharge_guard_ms=450\n",visible_ms);
    rt_kprintf("EPD527_MEMORY borrowed=%u released=1\n",borrowed);
    rt_kprintf("EPD527_DICTIONARY enabled=%u words=%u\n",dictionary_mode,dictionary_count);
    rt_kprintf("EPD527_PREFETCH hit=%u saved_ms=%u first_scan_ms=%u\n",hit,saved_ms,first_scan_ms);
#endif
    return 0;
}
static void lock_devices(void)
{
    /* Shared IO lock already excludes TF transfers; never wait for a
     * filesystem command lock while holding that lower-level lock. */
    frontlight_display_lock();board527_touch_lock();
}
static void unlock_devices(void)
{
    board527_touch_unlock();frontlight_display_unlock();
}
static unsigned refresh_mode(unsigned period,unsigned index,int has_gray,int old_gray)
{
    if(!period||index%period==0)return 0;
    return has_gray||old_gray?5:1;
}
/* While prepared, mixed retains displayed old levels in the upper nibble and
 * the expected next page in the lower nibble. It is never panel state yet. */
static void discard_prepared(void)
{
    if(prepared)for(unsigned i=0;i<W*H;++i)mixed[i]>>=4;
    prepared=0;release_extra_cache();
}
void board527_display_cancel_prepare(void)
{
    if(!ready)return;
    rt_mutex_take(&lock,RT_WAITING_FOREVER);discard_prepared();rt_mutex_release(&lock);
}
int board527_display_prepare(const uint8_t *gray4,int (*cancel)(void *),void *context)
{
    if(!ready||!gray4)return -1;
    rt_mutex_take(&lock,RT_WAITING_FOREVER);discard_prepared();
    if(!previous_known){rt_mutex_release(&lock);return -1;}
    uint32_t begin=rt_tick_get();int gray=0,old_gray=0,changed=0,rc=-1;
    for(unsigned i=0;i<W*H;++i) {
        unsigned old=mixed[i]&15,next=(gray4[i/2]>>((i&1)*4))&15;
        changed|=old!=next;gray|=next!=0&&next!=15;old_gray|=old!=0&&old!=15;
        mixed[i]=(old<<4)|next;
    }
    prepared=1; /* Also enables rollback on cancellation/allocation failure. */
    prepare_cancel=cancel;prepare_context=context;prepare_cancelled=0;
    if(changed&&!preparation_cancelled()) {
        prepared_mode=refresh_mode(full_period,refresh_index,gray,old_gray);
        prepared_count=waveform_bin_reader_get_frames(temperature,(WAVE_TABLE_MODE_T)prepared_mode);
        if(prepared_count&&prepared_count<=MAX_PHASES&&!prepare_phases(prepared_count)) {
            promote_raw_cache();
            if(!preparation_cancelled())rc=0;
        }
    }
    prepare_cancel=RT_NULL;prepare_context=RT_NULL;prepare_cancelled=0;
    prepared_ms=rt_tick_get()-begin;prepared_index=refresh_index;
    report_preparation();
    if(rc)discard_prepared();
#if BOARD527_VERBOSE_LOGS
    rt_kprintf("EPD527_PREPARED ready=%u ms=%u raw=%u borrowed=%u HV=OFF\n",!rc,prepared_ms,raw_cache,extra_bytes);
#endif
    rt_mutex_release(&lock);return rc;
}
int board527_display_present(const uint8_t *gray4)
{
    if(!ready||!gray4)return -1;
    rt_mutex_take(&lock,RT_WAITING_FOREVER);
    if(board527_io_enter()){rt_mutex_release(&lock);return -RT_EIO;}
    lock_devices();
    int rc=0,has_gray=0,old_gray=0,changed=0;
    if(prepared) {
        int match=previous_known&&prepared_index==refresh_index;
        for(unsigned i=0;match&&i<W*H;++i)
            if((mixed[i]&15)!=((gray4[i/2]>>((i&1)*4))&15))match=0;
        if(match) {
            rc=drive(prepared_mode);
            if(!rc){previous_known=1;++refresh_index;}
            goto finish;
        }
        discard_prepared();
    }
    if(!previous_known) {
        memset(mixed,0xff,sizeof(mixed));
        rc=drive(3);if(rc)goto finish;
        previous_known=1;
    }
    for(unsigned i=0;i<W*H;++i) {
        uint8_t old=mixed[i]&15,new_value=(gray4[i/2]>>((i&1)*4))&15;
        changed|=old!=new_value;
        has_gray|=new_value!=0&&new_value!=15;old_gray|=old!=0&&old!=15;
        mixed[i]=(old<<4)|new_value;
    }
    if(!changed)goto finish;
    /* Antialiased text also contains grays. GL16 handles these ordinary
     * transitions; GC16 is reserved for the configured cleaning interval. */
    unsigned mode=refresh_mode(full_period,refresh_index,has_gray,old_gray);
    rc=drive(mode);
    if(!rc){previous_known=1;++refresh_index;}
    else if(rc==-RT_ENOMEM) {
        /* Preparation failed before HV, so the displayed old image is intact. */
        for(unsigned i=0;i<W*H;++i)mixed[i]>>=4;
    }
finish:
    unlock_devices();board527_io_exit();rt_mutex_release(&lock);return rc;
}
int board527_display_init(void)
{
    uint32_t hash=2166136261u;
#ifdef READER527_RECOVERY_DISPLAY
    const uint8_t *wave=reader_recovery_wave;
#else
    const uint8_t *wave=(const uint8_t *)CUSTOM_EPD_WAVE_TABLE_START_ADDR;
#endif
    HAL_DBG_DWT_Init();rt_mutex_init(&lock,"epd527",RT_IPC_FLAG_PRIO);
    rt_timer_init(&cutoff,"epdlimit",power_cutoff,RT_NULL,rt_tick_from_millisecond(8000),RT_TIMER_FLAG_ONE_SHOT);
    idle();
    for(unsigned i=0;i<WAVE_BYTES;++i)hash=(hash^wave[i])*16777619u;
    if(hash!=WAVE_FNV){rt_kprintf("EPD527_WAVE_MISMATCH %08x\n",hash);return -1;}
    lcd_ready=waveform_bin_reader_init(CUSTOM_EPD_WAVE_TABLE_SIZE)==0&&setup_lcd()==0;
    ready=lcd_ready;previous_known=0;memset(mixed,0xff,sizeof(mixed));
    rt_kprintf("EPD527_READY=%d native=1216x684 VCOM=-1100mV source=48MHz row_cache=3MiB compressed\n",ready);
    return ready?0:-1;
}
void board527_display_off(void){idle();}
void board527_clock_seed(const uint8_t *gray4)
{
    rt_mutex_take(&lock,RT_WAITING_FOREVER);discard_prepared();
    for(unsigned i=0;i<W*H;++i)mixed[i]=(gray4[i/2]>>((i&1)*4))&15;
    previous_known=1;rt_mutex_release(&lock);
}
int board527_clock_present(const uint8_t *gray4,int full)
{
    unsigned saved_period=full_period,saved_index=refresh_index;
    full_period=full?0:11;refresh_index=full?0:1;
    int rc=board527_display_present(gray4);
    full_period=saved_period;refresh_index=saved_index;
    return rc;
}
uint8_t *board527_framebuffer(void){return reader_frame;}
void set_part_disp_times(int count)
{
    board527_display_cancel_prepare();full_period=count<0?0:(unsigned)count;refresh_index=0;
}
static int epd(int argc,char **argv)
{
    (void)argc;(void)argv;
    rt_kprintf("EPD527_STATUS ready=%d known=%d draws=%u frames=%u errors=%u last_ms=%u full_screen=1\n",ready,previous_known,draws,frames_total,errors,last_ms);
    rt_kprintf("EPD527_BUFFER prepared=%u bytes=%u rows=%u borrowed=%u app_free=%u\n",
        prepared,cache_used,cached_rows,extra_bytes,(unsigned)epub_mem_available_size());
    return 0;
}
MSH_CMD_EXPORT(epd, E0470A03 status);
