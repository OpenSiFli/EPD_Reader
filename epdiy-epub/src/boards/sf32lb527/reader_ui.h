#pragma once
#include "Renderer/Renderer.h"
#include "controls/Actions.h"
void reader527_home(Renderer *renderer,int selected);
void reader527_settings(Renderer *renderer);
bool reader527_frontlight(Renderer *renderer,UIAction action);
bool reader527_dfu_page(Renderer *renderer,UIAction action);
extern volatile int reader527_dfu_hit;
extern volatile int reader527_light_hit;
bool reader527_debug_message(uint32_t message,Renderer *renderer);
