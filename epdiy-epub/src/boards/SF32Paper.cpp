
#include "SF32Paper.h"
#include "SF32PaperRenderer.h"
#include <regular_font.h>
#include <bold_font.h>
#include <italic_font.h>
#include <bold_italic_font.h>
#include <hourglass.h>
#include "SF32_ButtonControls.h"

extern "C" {
#include "dfs_fs.h"
#include "mem_map.h"
#include "rtdevice.h"
#ifndef _WIN32
    #include "drv_flash.h"
#endif /* _WIN32 */


}


void SF32Paper::power_up()
{
  // nothing to do for this board - should probably move the power up from
  // the driver to here?
}
void SF32Paper::prepare_to_sleep()
{
  
}
Renderer *SF32Paper::get_renderer()
{
#if 1 //
  return new SF32PaperRenderer(
      &regular_font,
      &regular_font, //&bold_font,
      &regular_font, //&italic_font,
      &regular_font, //&bold_italic_font,
      hourglass_data,
      hourglass_width,
      hourglass_height);
#else
  return new SF32PaperRenderer(
      &regular_font,
      &bold_font,
      &italic_font,
      &bold_italic_font,
      hourglass_data,
      hourglass_width,
      hourglass_height);
#endif
}
void SF32Paper::start_filesystem()
{
    LOG_I("SF32Paper::start_filesystem");
#ifndef _WIN32
#ifndef FS_REGION_START_ADDR
    LOG_E("Need to define file system start address!");
#endif

    char *name[2];

    LOG_I("===auto_mnt_init===\n");

    memset(name, 0, sizeof(name));

#ifdef RT_USING_SDIO
    //Waitting for SD Card detection done.
    int sd_state = mmcsd_wait_cd_changed(3000);
    if (MMCSD_HOST_PLUGED == sd_state)
    {
        LOG_I("SD-Card plug in\n");
        name[0] = (char *)"sd0";
    }
    else
    {
        LOG_E("No SD-Card detected, state: %d\n", sd_state);
    }
#endif /* RT_USING_SDIO */

#if defined(RT_USING_SPI_MSD)
    uint16_t time_out = 100;
    LOG_I("Waitting for SD Card detection done...");
    while (time_out --)
    {
        rt_thread_mdelay(30);
        if (rt_device_find("sd0"))
        {
            LOG_I("Found SD-Card");
            name[0] = (char *)"sd0";
            break;
        }
    }
#endif

    name[1] = (char *)"flash0";
    register_mtd_device(FS_REGION_START_ADDR, FS_REGION_SIZE, name[1]);


    for (uint32_t i = 0; i < sizeof(name) / sizeof(name[0]); i++)
    {
        if (NULL == name[i]) continue;

        if (dfs_mount(name[i], "/", "elm", 0, 0) == 0) // fs exist
        {
            LOG_I("mount fs on %s to root success\n", name[i]);
            break;
        }
        else
        {
            LOG_E("mount fs on %s to root fail\n", name[i]);
        }
    }

#endif /* _WIN32 */
}
void SF32Paper::stop_filesystem()
{

}
ButtonControls *SF32Paper::get_button_controls(rt_mq_t ui_queue)
{
  return new SF32_ButtonControls(
    [ui_queue](UIAction action)
    {
      rt_mq_send(ui_queue, &action, sizeof(UIAction));
    }
  );
}
