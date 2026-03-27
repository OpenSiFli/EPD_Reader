/**
 * @file reading_settings.cpp
 * @brief 阅读设置页面 — 字体、字号、粗细、行距、边距调节
 *
 * 操作逻辑（与通用设置页面一致）：
 *   UP/DOWN: 在设置项之间上下移动
 *   SELECT: 在当前选项上循环切换值；在"保存退出"项上按 SELECT 保存并退出
 *
 * 持久化方案：
 *   保存设置到 TF 卡 /settings.cfg（简单 key=value 文本文件）
 *   启动时加载，若字体不可用则 fallback 到 Default
 */

#include "reading_settings.h"
#include <rtthread.h>
#include <string.h>
#include <stdio.h>
#include <dfs_posix.h>

#include "UIRegionsManager.h"

extern "C" {
#include "font_ft.h"
#include "font_manager.h"
}

/*==========================================================================
 * 配置文件路径
 *========================================================================*/

#define SETTINGS_FILE_PATH  "/settings.cfg"

/*==========================================================================
 * 设置项定义
 *========================================================================*/

/* 字号档位（像素） */
static const int FONT_SIZES[] = {24, 28, 32, 36, 40, 44, 48};
static const int FONT_SIZES_COUNT = sizeof(FONT_SIZES) / sizeof(FONT_SIZES[0]);

/* 加粗等级 */
static const int BOLD_LEVELS[] = {0, 1, 2};
static const int BOLD_LEVELS_COUNT = sizeof(BOLD_LEVELS) / sizeof(BOLD_LEVELS[0]);
static const char *BOLD_NAMES[] = {
    "\xe6\xad\xa3\xe5\xb8\xb8",    /* 正常 */
    "\xe4\xb8\xad\xe7\xb2\x97",    /* 中粗 */
    "\xe7\xb2\x97\xe4\xbd\x93"     /* 粗体 */
};

/* 行距档位（x10，避免浮点，10 = 1.0x, 12 = 1.2x） */
static const int LINE_SPACINGS[] = {10, 12, 14, 16, 18, 20};
static const int LINE_SPACINGS_COUNT = sizeof(LINE_SPACINGS) / sizeof(LINE_SPACINGS[0]);

/* 边距档位（像素） */
static const int MARGINS[] = {5, 8, 11, 14, 17, 20};
static const int MARGINS_COUNT = sizeof(MARGINS) / sizeof(MARGINS[0]);

/* 设置项枚举 */
typedef enum {
    SETTING_FONT_TYPE = 0,
    SETTING_FONT_SIZE,
    SETTING_BOLD,
    SETTING_LINE_SPACING,
    SETTING_MARGIN,
    SETTING_SAVE_EXIT,
    SETTING_COUNT
} SettingItem;

/* 当前状态 */
static SettingItem g_current_item = SETTING_FONT_TYPE;
static int g_font_type_idx = 0;
static int g_font_size_idx = 0;
static int g_bold_idx = 0;
static int g_line_spacing_idx = 1;
static int g_margin_idx = 1;
static bool g_settings_changed = false;
static bool g_font_changed = false;

/*==========================================================================
 * 辅助
 *========================================================================*/

static int find_index(const int *arr, int count, int value, int def = 0)
{
    for (int i = 0; i < count; i++) {
        if (arr[i] == value) return i;
    }
    return def;
}

static int find_font_by_name(const char *name)
{
    int count = font_manager_get_count();
    for (int i = 0; i < count; i++) {
        if (strcmp(font_manager_get_name(i), name) == 0)
            return i;
    }
    return -1;
}

static void utf8_truncate(char *dst, const char *src, int max_bytes)
{
    int len = (int)strlen(src);
    if (len <= max_bytes) {
        strcpy(dst, src);
        return;
    }
    int pos = max_bytes;
    while (pos > 0 && ((unsigned char)src[pos] & 0xC0) == 0x80) {
        pos--;
    }
    memcpy(dst, src, pos);
    dst[pos]     = '.';
    dst[pos + 1] = '.';
    dst[pos + 2] = '\0';
}

/*==========================================================================
 * 持久化
 *========================================================================*/

void reading_settings_save(void)
{
    int fd = open(SETTINGS_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        rt_kprintf("[settings] Cannot save to %s\n", SETTINGS_FILE_PATH);
        return;
    }

    char buf[256];
    int len = snprintf(buf, sizeof(buf),
                       "font_name=%s\n"
                       "font_size=%d\n"
                       "bold=%d\n"
                       "line_spacing=%d\n"
                       "margin=%d\n",
                       font_manager_get_name(g_font_type_idx),
                       FONT_SIZES[g_font_size_idx],
                       BOLD_LEVELS[g_bold_idx],
                       LINE_SPACINGS[g_line_spacing_idx],
                       MARGINS[g_margin_idx]);

    write(fd, buf, len);
    close(fd);

    rt_kprintf("[settings] Saved: font=%s size=%d bold=%d spacing=%d margin=%d\n",
               font_manager_get_name(g_font_type_idx),
               FONT_SIZES[g_font_size_idx],
               BOLD_LEVELS[g_bold_idx],
               LINE_SPACINGS[g_line_spacing_idx],
               MARGINS[g_margin_idx]);
}

void reading_settings_load(Renderer *renderer)
{
    char font_name[48] = {0};
    int font_size = 0, bold = 0, line_spacing = 0, margin = 0;
    bool file_loaded = false;

    int fd = open(SETTINGS_FILE_PATH, O_RDONLY);
    if (fd >= 0) {
        char buf[256] = {0};
        int n = read(fd, buf, sizeof(buf) - 1);
        close(fd);

        if (n > 0) {
            buf[n] = '\0';
            char *line = strtok(buf, "\n");
            while (line) {
                char *cr = strchr(line, '\r');
                if (cr) *cr = '\0';
                if (strncmp(line, "font_name=", 10) == 0) {
                    strncpy(font_name, line + 10, sizeof(font_name) - 1);
                } else if (strncmp(line, "font_size=", 10) == 0) {
                    font_size = atoi(line + 10);
                } else if (strncmp(line, "bold=", 5) == 0) {
                    bold = atoi(line + 5);
                } else if (strncmp(line, "line_spacing=", 13) == 0) {
                    line_spacing = atoi(line + 13);
                } else if (strncmp(line, "margin=", 7) == 0) {
                    margin = atoi(line + 7);
                }
                line = strtok(NULL, "\n");
            }
            file_loaded = true;
        }
    }

    if (!file_loaded) {
        rt_kprintf("[settings] No config file, using defaults\n");
        return;
    }

    rt_kprintf("[settings] Loaded: font=%s size=%d bold=%d spacing=%d margin=%d\n",
               font_name, font_size, bold, line_spacing, margin);

    g_font_size_idx = find_index(FONT_SIZES, FONT_SIZES_COUNT, font_size, 0);
    g_bold_idx = find_index(BOLD_LEVELS, BOLD_LEVELS_COUNT, bold, 0);
    g_line_spacing_idx = find_index(LINE_SPACINGS, LINE_SPACINGS_COUNT, line_spacing, 1);
    g_margin_idx = find_index(MARGINS, MARGINS_COUNT, margin, 1);

    int font_idx = find_font_by_name(font_name);
    if (font_idx < 0) {
        rt_kprintf("[settings] Font '%s' not found, fallback to Default\n", font_name);
        g_font_type_idx = 0;
    } else if (font_idx == 0) {
        g_font_type_idx = 0;
    } else {
        rt_kprintf("[settings] Restoring font [%d] %s...\n", font_idx, font_name);
        renderer->show_busy();
        int new_size = FONT_SIZES[g_font_size_idx];
        int ret = font_manager_select(font_idx, new_size);
        if (ret != 0) {
            rt_kprintf("[settings] Font load failed (%d), fallback to Default\n", ret);
            g_font_type_idx = 0;
        } else {
            g_font_type_idx = font_idx;
            rt_kprintf("[settings] Font restored OK\n");
        }
    }

    int target_size = FONT_SIZES[g_font_size_idx];
    if (epd_font_ft_get_size() != target_size) {
        epd_font_ft_set_size(target_size);
    }
    epd_font_ft_set_bold(BOLD_LEVELS[g_bold_idx]);
    float spacing = LINE_SPACINGS[g_line_spacing_idx] / 10.0f;
    epd_font_ft_set_line_spacing(spacing);
    int m = MARGINS[g_margin_idx];
    renderer->set_margin_left(m);
    renderer->set_margin_right(m);

    rt_kprintf("[settings] Applied: font=%s size=%d bold=%d spacing=%.1f margin=%d\n",
               font_manager_get_name(g_font_type_idx), target_size,
               BOLD_LEVELS[g_bold_idx], spacing, m);
}

/*==========================================================================
 * 绘制设置页面
 *========================================================================*/

void reading_settings_draw(Renderer *renderer)
{
    renderer->clear_screen();

    clear_areas();  // 清除旧的触控区域

    int page_w = renderer->get_page_width();
    int page_h = renderer->get_page_height();
    int line_h = renderer->get_line_height();
    int y = 20;
    int x_label = 30;
    int x_value = page_w * 2 / 5;
    int max_name_bytes = 20;

    /* 标题居中 */
    const char *title = "\xe9\x98\x85\xe8\xaf\xbb\xe8\xae\xbe\xe7\xbd\xae";  /* 阅读设置 */
    int title_w = epd_font_ft_get_size() * 4;
    int title_x = (page_w - title_w) / 2;
    if (title_x < 0) title_x = 0;
    renderer->draw_text(title_x, y, title, false, false);
    y += line_h * 2;

    renderer->draw_rect(20, y - 5, page_w - 40, 1, 0);
    y += 10;

    /* 设置项标签 */
    const char *item_names[] = {
        "\xe5\xad\x97\xe4\xbd\x93",    /* 字体 */
        "\xe5\xad\x97\xe5\x8f\xb7",    /* 字号 */
        "\xe5\xad\x97\xe9\x87\x8d",    /* 字重 */
        "\xe8\xa1\x8c\xe8\xb7\x9d",    /* 行距 */
        "\xe8\xbe\xb9\xe8\xb7\x9d",    /* 边距 */
        "\xe4\xbf\x9d\xe5\xad\x98\xe9\x80\x80\xe5\x87\xba"  /* 保存退出 */
    };
    char val_buf[64];

    for (int i = 0; i < SETTING_COUNT; i++) {
        if (y + line_h > page_h - line_h * 3) break;

        /* 注册整行触控区域：g_area_array[i] 对应 SettingItem i */
        add_area(0, y - 2, page_w, line_h * 2);

        if (i == g_current_item) {
            renderer->fill_triangle(x_label - 20, y + line_h / 2,
                                    x_label - 10, y + line_h / 2 - 6,
                                    x_label - 10, y + line_h / 2 + 6, 0);
            renderer->draw_rect(x_label - 25, y - 2, page_w - 40, line_h + 4, 0);
        }

        renderer->draw_text(x_label, y, item_names[i], false, false);

        if (i != SETTING_SAVE_EXIT) {
            switch (i) {
            case SETTING_FONT_TYPE: {
                char name_buf[32];
                utf8_truncate(name_buf, font_manager_get_name(g_font_type_idx), max_name_bytes);
                snprintf(val_buf, sizeof(val_buf), "< %s >", name_buf);
                break;
            }
            case SETTING_FONT_SIZE:
                snprintf(val_buf, sizeof(val_buf), "< %d px >", FONT_SIZES[g_font_size_idx]);
                break;
            case SETTING_BOLD:
                if (epd_font_ft_is_variable_weight()) {
                    snprintf(val_buf, sizeof(val_buf), "< %s (VF) >", BOLD_NAMES[g_bold_idx]);
                } else {
                    snprintf(val_buf, sizeof(val_buf), "< %s >", BOLD_NAMES[g_bold_idx]);
                }
                break;
            case SETTING_LINE_SPACING:
                snprintf(val_buf, sizeof(val_buf), "< %d.%dx >",
                         LINE_SPACINGS[g_line_spacing_idx] / 10,
                         LINE_SPACINGS[g_line_spacing_idx] % 10);
                break;
            case SETTING_MARGIN:
                snprintf(val_buf, sizeof(val_buf), "< %d px >", MARGINS[g_margin_idx]);
                break;
            }
            renderer->draw_text(x_value, y, val_buf, false, false);
        }

        y += line_h * 2;
    }

    /* 操作提示 */
    y = page_h - line_h * 3;
    if (y < page_h / 2) y = page_h / 2;

    renderer->draw_rect(20, y - 5, page_w - 40, 1, 128);
    y += 10;

    renderer->draw_text(x_label, y,
        "\xe4\xb8\x8a/\xe4\xb8\x8b: \xe5\x88\x87\xe6\x8d\xa2\xe9\x80\x89\xe9\xa1\xb9",
        /* 上/下: 切换选项 */
        false, false);
    y += line_h;

    if (y + line_h <= page_h) {
        renderer->draw_text(x_label, y,
            "\xe7\xa1\xae\xe8\xae\xa4: \xe8\xb0\x83\xe6\x95\xb4\xe5\x80\xbc / \xe4\xbf\x9d\xe5\xad\x98\xe9\x80\x80\xe5\x87\xba",
            /* 确认: 调整值 / 保存退出 */
            false, false);
    }

    renderer->flush_display();
}

/*==========================================================================
 * 处理按键
 *
 * UP/DOWN: 在设置项之间上下移动（循环）
 * SELECT: 在当前选项上循环切换值；在"保存退出"项上保存并退出
 *========================================================================*/

bool reading_settings_handle_action(Renderer *renderer, UIAction action)
{
    int font_count = font_manager_get_count();

    switch (action) {
    case UP:
        if (g_current_item > 0)
            g_current_item = (SettingItem)(g_current_item - 1);
        else
            g_current_item = (SettingItem)(SETTING_COUNT - 1);
        reading_settings_draw(renderer);
        return true;

    case DOWN:
        if (g_current_item < SETTING_COUNT - 1)
            g_current_item = (SettingItem)(g_current_item + 1);
        else
            g_current_item = (SettingItem)0;
        reading_settings_draw(renderer);
        return true;

    case SELECT:
        switch (g_current_item) {
        case SETTING_FONT_TYPE:
            g_font_type_idx = (g_font_type_idx + 1) % font_count;
            g_settings_changed = true;
            g_font_changed = true;
            break;
        case SETTING_FONT_SIZE:
            g_font_size_idx = (g_font_size_idx + 1) % FONT_SIZES_COUNT;
            g_settings_changed = true;
            break;
        case SETTING_BOLD:
            g_bold_idx = (g_bold_idx + 1) % BOLD_LEVELS_COUNT;
            g_settings_changed = true;
            break;
        case SETTING_LINE_SPACING:
            g_line_spacing_idx = (g_line_spacing_idx + 1) % LINE_SPACINGS_COUNT;
            g_settings_changed = true;
            break;
        case SETTING_MARGIN:
            g_margin_idx = (g_margin_idx + 1) % MARGINS_COUNT;
            g_settings_changed = true;
            break;
        case SETTING_SAVE_EXIT:
            if (g_settings_changed) {
                reading_settings_apply(renderer);
                reading_settings_save();
                g_settings_changed = false;
                g_font_changed = false;
            }
            g_current_item = SETTING_FONT_TYPE;
            return false;  /* 退出设置页面 */
        default:
            break;
        }
        reading_settings_draw(renderer);
        return true;

    default:
        return true;
    }
}

/*==========================================================================
 * 应用设置
 *========================================================================*/

void reading_settings_apply(Renderer *renderer)
{
    int new_size = FONT_SIZES[g_font_size_idx];
    float new_spacing = LINE_SPACINGS[g_line_spacing_idx] / 10.0f;
    int new_margin = MARGINS[g_margin_idx];
    int new_bold = BOLD_LEVELS[g_bold_idx];

    if (g_font_changed) {
        rt_kprintf("Switching font to [%d] %s...\n",
                   g_font_type_idx, font_manager_get_name(g_font_type_idx));
        renderer->show_busy();
        int ret = font_manager_select(g_font_type_idx, new_size);
        if (ret != 0) {
            rt_kprintf("Font switch failed: %d, keeping current font\n", ret);
            g_font_type_idx = font_manager_get_current();
        }
    } else {
        epd_font_ft_set_size(new_size);
    }

    rt_kprintf("Applying settings: font=%s, size=%d, bold=%d, spacing=%.1f, margin=%d\n",
               font_manager_get_name(g_font_type_idx), new_size, new_bold,
               new_spacing, new_margin);

    epd_font_ft_set_bold(new_bold);
    epd_font_ft_set_line_spacing(new_spacing);
    renderer->set_margin_left(new_margin);
    renderer->set_margin_right(new_margin);
}

/*==========================================================================
 * 触控辅助：设置当前选中项
 *========================================================================*/

void reading_settings_set_current_item(int index)
{
    if (index >= 0 && index < SETTING_COUNT) {
        g_current_item = (SettingItem)index;
    }
}