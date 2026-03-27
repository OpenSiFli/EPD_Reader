/**
 * @file font_manager.c
 * @brief 字体管理器实现（FreeType 版）
 */

#include <rtthread.h>
#include <dfs_posix.h>
#include <string.h>
#include "font_manager.h"
#include "font_ft.h"

#define LOG_TAG "font_mgr"
#include <rtdbg.h>

/*==========================================================================
 * 字体列表
 *========================================================================*/

#define FONT_MAX_COUNT   64
#define FONT_NAME_MAX    32
#define FONT_PATH_MAX    64

typedef struct {
    char name[FONT_NAME_MAX];
    char path[FONT_PATH_MAX];
    int  is_builtin;
} FontEntry;

static FontEntry g_font_list[FONT_MAX_COUNT];
static int g_font_count = 0;
static int g_current_font = 0;

/*==========================================================================
 * 辅助
 *========================================================================*/

static int is_ttf_file(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if (!dot) return 0;
    return (strcmp(dot, ".ttf") == 0 || strcmp(dot, ".TTF") == 0 ||
            strcmp(dot, ".otf") == 0 || strcmp(dot, ".OTF") == 0);
}

static void extract_display_name(const char *filename, char *name, int max_len)
{
    const char *dot = strrchr(filename, '.');
    int len = dot ? (int)(dot - filename) : (int)strlen(filename);
    if (len >= max_len) len = max_len - 1;
    memcpy(name, filename, len);
    name[len] = '\0';
}

/*==========================================================================
 * 公共 API
 *========================================================================*/

int font_manager_init(const char *font_dir)
{
    strcpy(g_font_list[0].name, "Default");
    g_font_list[0].path[0] = '\0';
    g_font_list[0].is_builtin = 1;
    g_font_count = 1;
    g_current_font = 0;

    DIR *dir = opendir(font_dir);
    if (!dir) {
        rt_kprintf("[font_mgr] No font directory: %s\n", font_dir);
        return g_font_count;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (g_font_count >= FONT_MAX_COUNT) break;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (!is_ttf_file(entry->d_name)) continue;

        FontEntry *fe = &g_font_list[g_font_count];
        snprintf(fe->path, FONT_PATH_MAX, "%s/%s", font_dir, entry->d_name);
        extract_display_name(entry->d_name, fe->name, FONT_NAME_MAX);
        fe->is_builtin = 0;
        g_font_count++;

        rt_kprintf("[font_mgr] Found font: [%d] %s (%s)\n",
                   g_font_count - 1, fe->name, fe->path);
    }
    closedir(dir);

    rt_kprintf("[font_mgr] Total %d fonts available\n", g_font_count);
    return g_font_count;
}

int font_manager_get_count(void)  { return g_font_count; }

const char *font_manager_get_name(int index)
{
    if (index < 0 || index >= g_font_count) return "???";
    return g_font_list[index].name;
}

int font_manager_get_current(void) { return g_current_font; }

int font_manager_select(int index, int pixel_size)
{
    if (index < 0 || index >= g_font_count) return -1;

    rt_kprintf("[font_mgr] Switching to font [%d] %s\n",
               index, g_font_list[index].name);

    /* 清缓存 + 释放旧 FT_Face */
    epd_font_ft_cache_clear();
    epd_font_ft_deinit();

    if (g_font_list[index].is_builtin) {
        /* 内置字体：Flash XIP，通过 FT_New_Memory_Face */
        int ret = epd_font_ft_init_builtin(pixel_size);
        if (ret != 0) {
            rt_kprintf("[font_mgr] ERROR: init builtin failed: %d\n", ret);
            return -2;
        }
    } else {
        /* TF 卡字体：FreeType 直接从文件读取，不占 PSRAM */
        int ret = epd_font_ft_init_from_file(g_font_list[index].path, pixel_size);
        if (ret != 0) {
            rt_kprintf("[font_mgr] ERROR: init from file failed: %s\n",
                       g_font_list[index].path);
            /* 回退到内置字体 */
            epd_font_ft_init_builtin(pixel_size);
            g_current_font = 0;
            return -4;
        }
    }

    g_current_font = index;
    rt_kprintf("[font_mgr] Font switched OK: [%d] %s\n",
               index, g_font_list[index].name);
    return 0;
}

void font_manager_deinit(void)
{
    epd_font_ft_cache_clear();
    epd_font_ft_deinit();
    g_font_count = 0;
    g_current_font = 0;
}