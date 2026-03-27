/**
 * @file font_manager.h
 * @brief 字体管理器 — 扫描 TF 卡字体、加载到 PSRAM、切换字体
 *
 * 字体列表 index 0 固定是 ROM2 内置字体（兜底），
 * index 1+ 是 TF 卡 /fonts/ 目录下扫描到的 TTF 文件。
 */
#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 扫描字体目录，建立字体列表。
 * index 0 固定为内置字体。
 * @param font_dir TF 卡字体目录路径，如 "/fonts"
 * @return 可用字体总数（含内置）
 */
int font_manager_init(const char *font_dir);

/**
 * 获取可用字体数量（含内置）。
 */
int font_manager_get_count(void);

/**
 * 获取第 i 个字体的显示名。
 */
const char *font_manager_get_name(int index);

/**
 * 切换到第 i 个字体。
 * index=0 切回内置字体，index>0 从 TF 卡加载。
 * @param index 字体索引
 * @param pixel_size 字号（像素）
 * @return 0 成功，<0 失败
 */
int font_manager_select(int index, int pixel_size);

/**
 * 获取当前选中的字体索引。
 */
int font_manager_get_current(void);

/**
 * 释放从 TF 卡加载的字体数据。
 */
void font_manager_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* FONT_MANAGER_H */
