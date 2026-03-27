/**
 * @file reading_settings.h
 * @brief 阅读设置页面 — 字体、字号、行距、边距调节（含持久化）
 */
#ifndef READING_SETTINGS_H
#define READING_SETTINGS_H

#include "../Renderer/Renderer.h"
#include "boards/controls/Actions.h"

/* 绘制设置页面 */
void reading_settings_draw(Renderer *renderer);

/* 处理按键，返回 true=继续在设置页面，false=退出设置 */
bool reading_settings_handle_action(Renderer *renderer, UIAction action);

/* 应用当前设置（字体、字号、行距、边距） */
void reading_settings_apply(Renderer *renderer);

/* 持久化：保存当前设置到 TF 卡 */
void reading_settings_save(void);

/* 持久化：从 TF 卡加载设置并应用（启动时调用，含 fallback） */
void reading_settings_load(Renderer *renderer);

/* 设置当前选中项索引（供触控回调使用） */
void reading_settings_set_current_item(int index);

#endif /* READING_SETTINGS_H */