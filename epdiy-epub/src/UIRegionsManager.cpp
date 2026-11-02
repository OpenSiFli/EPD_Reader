#include "UIRegionsManager.h"
#include <cstring>

// 2. 定义全局数组和相关变量
AreaRect g_area_array[MAX_AREAS];
int g_area_count = 0;

// 3. 清空区域数组的函数
void clear_areas() 
{
    g_area_count = 0;
    memset(g_area_array, 0, sizeof(g_area_array));
}

// 4. 添加区域到数组的函数
bool add_area(int x, int y, int width, int height) 
{
    if (g_area_count >= MAX_AREAS) {
        return false; // 数组已满
    }
    
    g_area_array[g_area_count].start_x = x;
    g_area_array[g_area_count].start_y = y;
    g_area_array[g_area_count].end_x = x + width;
    g_area_array[g_area_count].end_y = y + height;
    g_area_count++;
    
    return true;
}

bool static_add_area(int x, int y, int width, int height ,int index) 
{
    
    if(index < 0 || index >= MAX_AREAS) {
        return false; // 索引超出范围
    }
    g_area_array[index].start_x = x;
    g_area_array[index].start_y = y;
    g_area_array[index].end_x = x + width;
    g_area_array[index].end_y = y + height;
    if(index>=g_area_count)g_area_count=index+1;
    
    return true;
}

