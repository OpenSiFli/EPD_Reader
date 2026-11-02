/* SPDX-License-Identifier: Apache-2.0
 * Retain the pinned SF32LB52 silicon/Flash bring-up. Do not execute its generic
 * OTA selector: the immutable first stage dispatches only to fixed partitions.
 */
#define entry sdk_unused_entry
#define boot_images_help sdk_unused_boot_images_help
#include "main.c" /* pinned SDK butterflmicro/board/main.c, via SConscript */
#undef entry
#undef boot_images_help
#include "reader_dfu.h"

static int read_nor(uint32_t address,void *data,uint32_t size)
{return g_flash_read(address,(const int8_t *)data,size)==(int)size?0:-1;}
static const r527_io reader_io={read_nor,0,0};
static int recovery_key(void)
{
#ifdef SF32LB57X
    HAL_PIN_Set(PAD_PA34,GPIO_A34,PIN_PULLDOWN,1);
    GPIO_InitTypeDef cfg={.Pin=34,.Mode=GPIO_MODE_INPUT,.Pull=GPIO_PULLDOWN};
#else
    /* Original btn1 is PA33, active LOW. Boot only needs a digital
     * pressed/not-pressed signal, not its application ADC key identity.
     * PA34 is the hardware long-press reset and must never select recovery. */
    HAL_PIN_Set(PAD_PA33,GPIO_A33,PIN_PULLUP,1);
    GPIO_InitTypeDef cfg={.Pin=33,.Mode=GPIO_MODE_INPUT,.Pull=GPIO_PULLUP};
#endif
    HAL_GPIO_Init(hwp_gpio1,&cfg);
    /* Require a sustained press; return immediately on a normal boot. */
    for(unsigned n=0;n<150;++n) {
#ifdef SF32LB57X
        if(HAL_GPIO_ReadPin(hwp_gpio1,34)!=GPIO_PIN_SET)return 0;
#else
        if(HAL_GPIO_ReadPin(hwp_gpio1,33)!=GPIO_PIN_RESET)return 0;
#endif
        HAL_Delay_us(10000);
    }
    return 1;
}
static void boot_reader(void)
{
    int requested=HAL_Get_backup(RTC_BAKCUP_OTA_FORCE_MODE)==R527_COOKIE;
    HAL_Set_backup(RTC_BAKCUP_OTA_FORCE_MODE,0);
    int held=recovery_key();r527_record record;
    int valid=r527_latest(&reader_io,&record,0)==0&&r527_verify(&reader_io,&record)==0;
    uint32_t dest=(!requested&&!held&&valid)?r527_image_base(0):R527_LOADER_BASE;
    uint32_t vectors[2];
    if(read_nor(dest,vectors,sizeof(vectors)))return;
    uint32_t limit=dest==R527_LOADER_BASE?R527_LOADER_SIZE:r527_image_limit(0);
    if((vectors[0]&7)||vectors[0]<0x20000000u||vectors[0]>0x2007fc00u||
       !(vectors[1]&1)||vectors[1]<dest||vectors[1]>=dest+limit)return;
    board_init_psram();
    HAL_FLASH_ALIAS_CFG(boot_handle,dest,limit,0);
    run_img(dest);
}
int entry(void)
{
    HAL_Delay_us(0);
    if(__HAL_SYSCFG_GET_REVID()>=HAL_CHIP_REV_ID_A4) {
        board_flash_power_on();HAL_MspInit();board_boot_src=board_boot_from();
    } else {
        board_boot_src=board_boot_from();board_flash_power_on();HAL_MspInit();
    }
    __HAL_SYSCFG_CLEAR_SECURITY();dfu_flash_init();boot_reader();
    /* Corrupt recovery vectors require factory reinstallation, never a jump
     * into unchecked application data. No erase/write primitive is used here. */
    while(1){};
}
