# 墨水屏，电纸书demo

## 概述
* 基于奥翼opm060e9墨水屏，em-lb525开发板，通过按键控制实现以下功能
1. 打开不同的电子书
2. 打开电子书对内容进行前后翻页
3. 在电子书内容界面通过按键选择字体和大小

* 提供2种刷新方式（注意 2种方式的接线不同， 在menuconfig中切换）
 1. 纯软件刷新方式 (宏 `EPD_DRIVER_LCDC`)
 1. 硬件加速方案（宏 `EPD_DRIVER_LCDC`）

## 阅读电子书操作指南
1. 主界面文件选择
* 上下移动选择：按K2键上移光标，按K1键下移光标，浏览电子书列表。
2. 进入/退出电子书的阅读界面
* 进入书籍：光标选中目标电子书后，长按K1键2秒即可打开。
* 退出返回：阅读中长按K2键，直接退回主界面。
3. 内容翻阅与设置
* 翻页操作：进入电子书后，短按K1键向下翻页，短按K2键向上翻页。
* 字体调整：\
	a. 阅读时长按K1键，进入字体与字号设置界面；\
	b. 通过K1/K2键选择目标字体或字号，再次长按K1键确认。\
	c. 界面将自动刷新成新格式。
4. 操作逻辑总结
* 短按：用于基础功能（移动、翻页）；
* 长按K1键：层级递进操作（确认/设置）；
* 长按K2键：返回主界面。

## 软件逻辑
1. 窗口管理:分为目录窗口(`dir_window`),阅读窗口(`reader_window`)和字体设置窗口(`sitting_window`),通过按键来控制每个界面
2. 通过文件系统存储.txt格式文档到 \disk 目录下
3. 通过文件系统读取一页数据缓存在 \src\reader_window.c中的`readbuffer`中
4. 仿照\Book_Reader\BSP\book_read\bsp_book_read.c`void BSP_DispPage_EPD( uint16_t usX, uint16_t usY, uint8_t f_size, uint8_t myFont, uint8_t wordspace, uint8_t usRowL)`函数，实现翻页后，缓存数组中缓存新数据
5. 送屏数据数组\src\e-ink\bsp_e-ink.c`unsigned char  BlackImage[EPD_ARRAY];`
6. 通过\src\e-ink\bsp_e-ink.c`Paint_NewImage(BlackImage, EPD_HEIGHT, EPD_WIDTH, ROTATE_90, WHITE);` 将`Paint.Image`地址指向`BlackImage[EPD_ARRAY]`，在接下来文本转换点阵数据中，接收点阵数据 （可在`void BSP_DispPage_EPD( uint16_t usX, uint16_t usY, uint8_t f_size, uint8_t myFont, uint8_t wordspace, uint8_t usRowL)` 中实现）
7. 通过\src\e-ink\bsp_e-ink.c`SSD1677_Display_Page( 16, 32, Fsize_Control, Font_Control, Z_Space_Control, H_Space_Control, (char *)ReadBuffer)`函数将特定编码格式的文本，转换为点阵buffer数据，存放在`BlackImage[EPD_ARRAY]`中
8. 最后通过\src\opm060e9_driver\opm060e9_driver.c`epd_display_pic(BlackImage);`函数送屏，实现显示。

## 编码转点阵实现逻辑

### 显示整页函数
1. 编码格式：已经存放的点整转换字库，转换的格式为GBK2312。GBK2312中，每个汉字占两个字节16位，英文占八字节，使用和ASCII(英文字符只读取ASCII值前126的	)码相同的编码格式。
2. 判断汉，英文：传入数据后先通过`if( (* pStr <= 126)&&(* pStr != '\r') )`判断是汉字还是英文字符。
3. 自动换行：检测到换行符，或超出屏幕边界。
```c
void SSD1677_Display_Page( uint16_t usX, uint16_t usY, uint8_t f_size, uint8_t myFont, uint8_t wordspace, uint8_t rowLedge, char * pStr)
{
	uint32_t EN_Counter=0;  // 存放当前页显示的英文字符总数
	uint32_t CH_Counter=0;   // 存放当前页显示的汉字总数
	uint32_t Space_Counter=0;  // 存放当前页显示的空格总数		

	uint16_t usCh=0;
	bool Page_Display_Limit = false; // 页面显示限制，防止超出屏幕宽度

	while( * pStr != '\0' )
	{
		if( * pStr == '\r' )  // 遇到换行时 y轴坐标增加一个单位，另起一行
		{   
			usY += (f_size + rowLedge);
			usX = 16;			
//			printf("\r\n>>>>>>>>遇到了换行符\r\n");
		}

		if( (* pStr <= 126)&&(* pStr != '\r') )	 //英文字符只读取ASCII值前126的			
		{
			if ( ( usX + f_size/2 ) > LCD_X_LENGTH )  // x轴坐标超出屏幕宽度
			{
				usX = 16; //左边距
				usY += (f_size + rowLedge); // Y换行
			}
			if ( ( usY + f_size ) > (LCD_Y_LENGTH-32) ) // y轴坐标超出屏幕宽度
			{
				usX = 0; // X = 0;
				usY = 0; // Y = 0;
				Page_Display_Limit = true;
			}					

			if(Page_Display_Limit == false)	// 不超出范围时显示		
			{
				SSD1677_DispChar_EN( usX, usY, BLACK, f_size, * pStr);   // 显示英文字符
				EN_Counter++; // 统计显示的英文字符数
			}
			pStr ++;	
			usX +=  (f_size/2+wordspace);	// X轴坐标增加		
		}	
		else  // 显示汉字字符
		{
			if ( ( usX + f_size ) > LCD_X_LENGTH )  // x轴坐标超出屏幕宽度
			{
				usX = 16;//左边距
				usY += (f_size + rowLedge);
			}			
			if ( ( usY + f_size ) > (LCD_Y_LENGTH-32) ) // y轴坐标超出屏幕宽度
			{
				usX = 0;
				usY = 0;
				Page_Display_Limit = true;
			}
					
			usCh = * ( uint16_t * ) pStr;			     // 码值转换				
			usCh = ( usCh << 8 ) + ( usCh >> 8 );		
			// usCh = (uint8_t)*pStr ;   // 将pStr的第一个字节赋值给usCh的高八位
			// usCh |= (uint8_t)*(pStr + 1)<< 8; // 将pStr的第二个字节赋值给usCh的低八位
			if(Page_Display_Limit == false)	// 超出范围不显示		
			{  
				if(usCh==3338)  // ' '空格符号的国标码值
				{	  									    
					Space_Counter++;  // 统计显示的空格符总数
				}	 
				else
				{	
                    if(* pStr != '\r')
					{						
						SSD1677_DispChar_CH( usX, usY, BLACK, f_size, myFont, usCh);	  // 显示一个汉字				
						CH_Counter++; // 统计显示的汉字总数	
						usX += (f_size+wordspace);
					}						
				}									 
			}					
			pStr += 2;           //一个汉字两个字节 				
		}
	}
	
//	printf("输出的空格数量：%03d \r\n",Space_Counter);			
//	printf("输出的英文数量：%03d \r\n",EN_Counter);				
//	printf("输出的汉字数量：%03d \r\n",CH_Counter);

	Page_Counter_EN_CH_Space = CH_Counter*2 + EN_Counter + Space_Counter*2; // 显示的总字节数
//	printf("显示的总字节数：%03d \r\n",Page_Counter_EN_CH_Space);

	Space_Counter = 0;
	EN_Counter = 0;
	CH_Counter = 0;

	Page_Display_Limit = false; // 页面显示限制变量清零，防止下次判断失误
}
```

### 显示单个中文或英文字符
```c
void SSD1677_DispChar_EN( uint16_t usX, uint16_t usY, uint16_t Color, uint8_t f_size, const char cChar)
{
	uint32_t   byteCount, bitCount,fontLength;	
	uint32_t   ucRelativePositon;
	uint32_t   usTemp; 
	uint8_t    *Pfont;	
    uint16_t      Forecolor,BackColor; 
	// ---------------------------------------------		
		
	if(Color == BLACK)
	{
		Forecolor = BLACK; // 前景色
		BackColor = WHITE; // 背景色
	}
	else
	{
		Forecolor = WHITE; // 前景色
		BackColor = BLACK; // 背景色		
	}
	
	ucRelativePositon = cChar - ' '; //对ascii码表偏移（字模表不包含ASCII表的前32个非图形符号）	
//----------------------------------------------------------------------------------------------------------	
	// //按字节读取字模数据
	// if(f_size==16)
	// {
	// 	fontLength = f_size*f_size/2/8;
	// 	Pfont = (uint8_t *)&ASCII8x16_Table[ucRelativePositon * fontLength];  /*ascii码表偏移值乘以每个字模的字节数，求出字模的偏移位置*/
		
	// 	for ( byteCount = 0; byteCount < fontLength; byteCount++ )  // 控制字节数循环
	// 	{			
	// 		for ( bitCount = 0; bitCount < 8; bitCount++ )  //一位一位处理要显示的颜色
	// 		{
	// 			if ( Pfont[byteCount] & (0x80>>bitCount) )
	// 				Paint_DrawPoint( usX+bitCount, usY+byteCount, Forecolor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
	// 			else
	// 				Paint_DrawPoint( usX+bitCount, usY+byteCount, BackColor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
	// 		}	
	// 	}	
	// }
//----------------------------------------------------------------------------------------------------------
	if(f_size==24)
	{
		Pfont = (uint8_t *)&ASCII12x24_Table[ucRelativePositon * 48]; /*ascii码表偏移值乘以每个字模的字节数，求出字模的偏移位置*/
		
		for ( byteCount = 0; byteCount < 24; byteCount++ )
		{
			/* 取出两个字节的数据，在lcd上即是一个汉字的一行 */
			usTemp = Pfont [ byteCount * 2 + 0 ];
			usTemp = ( usTemp << 8 );		
			usTemp |= Pfont [ byteCount * 2 + 1 ];        		

			for ( bitCount = 0; bitCount < 12; bitCount ++ )
			{			
				if ( usTemp & ( 0x8000 >> bitCount ) )  //高位在前 
					Paint_DrawPoint( usX+bitCount, usY+byteCount, Forecolor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
				else
					Paint_DrawPoint( usX+bitCount, usY+byteCount, BackColor, DOT_PIXEL_DFT, DOT_STYLE_DFT);		
			}			
		}	
	}	
//----------------------------------------------------------------------------------------------------------
	if(f_size==32)
	{
		fontLength = f_size*f_size/2/8;
		Pfont = (uint8_t *)&ASCII16x32_Table[ucRelativePositon * fontLength];  /*ascii码表偏移值乘以每个字模的字节数，求出字模的偏移位置*/
		
		for ( byteCount = 0; byteCount < (fontLength/2); byteCount++ )  // 控制字节数循环
		{			
			for ( bitCount = 0; bitCount < 8; bitCount++ )  //一位一位处理要显示的颜色
			{
				if ( Pfont[byteCount*2] & (0x80>>bitCount) )
					Paint_DrawPoint( usX+bitCount, usY+byteCount, Forecolor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
				else
					Paint_DrawPoint( usX+bitCount, usY+byteCount, BackColor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
				//--------------------------------------------------
				if ( Pfont[byteCount*2+1] & (0x80>>bitCount) )
					Paint_DrawPoint( usX+bitCount+8, usY+byteCount, Forecolor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
				else
					Paint_DrawPoint( usX+bitCount+8, usY+byteCount, BackColor, DOT_PIXEL_DFT, DOT_STYLE_DFT);			
			}	
		}
    }	
}

void SSD1677_DispChar_CH( uint16_t usX, uint16_t usY, uint16_t Color, uint8_t f_size, uint8_t myFont, uint16_t usChar)
{
	uint8_t   rowCount, bitCount;
//	uint8_t   ucBuffer [ f_size*f_size/8 ];  // 使用此条会导致不定时触发HardFault_Handler
	uint8_t   ucBuffer[128];		
	uint32_t  usTemp1,usTemp2; 	
    uint16_t     Forecolor,BackColor; 
	
	if(Color == BLACK)
	{
		Forecolor = BLACK; // 前景色
		BackColor = WHITE; // 背景色
	}
	else
	{
		Forecolor = WHITE; // 前景色
		BackColor = BLACK; // 背景色		
	}	
	
	//取字模数据  
	GetGBKCode_from_EXFlash ( ucBuffer, usChar, f_size, myFont);	

	for ( rowCount = 0; rowCount < f_size; rowCount++ )
	{
		/* 取出两个字节的数据，在lcd上即是一个汉字的一行 */
		usTemp1 = ucBuffer  [ rowCount * (f_size/8) + 0 ];
		usTemp1 = ( usTemp1 << 8 );
		usTemp1 |= ucBuffer [ rowCount * (f_size/8) + 1 ];
		
		if(f_size==24) // 24字号独有代码
		{
			usTemp2 = ucBuffer  [ rowCount * (f_size/8) + 2 ];
		}
		if(f_size==32) // 32字号独有代码
		{
			usTemp2 = ucBuffer  [ rowCount * (f_size/8) + 2 ];
			usTemp2 = ( usTemp2 << 8 );		
			usTemp2 |= ucBuffer [ rowCount * (f_size/8) + 3 ];			
		}
		
        //数据逐行处理		
		for ( bitCount = 0; bitCount < 16; bitCount ++ )
		{			
			if ( usTemp1 & ( 0x8000 >> bitCount ) )  // 16/24/32字号代码通用部分
				Paint_DrawPoint( usX+bitCount, usY+rowCount, Forecolor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
            else
				Paint_DrawPoint( usX+bitCount, usY+rowCount, BackColor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
			
			if(f_size==32)  // 32字号独有代码
			{
				if ( usTemp2 & ( 0x8000 >> bitCount ) )  
					Paint_DrawPoint( usX+bitCount+16, usY+rowCount, Forecolor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
				else
					Paint_DrawPoint( usX+bitCount+16, usY+rowCount, BackColor, DOT_PIXEL_DFT, DOT_STYLE_DFT);			
			}
		}
		
		if(f_size==24)  // 24字号独有代码
		{
			for ( bitCount = 0; bitCount < 8; bitCount ++ )
			{					
				if ( usTemp2 & ( 0x80 >> bitCount ) )  //高位在前 
					Paint_DrawPoint( usX+bitCount+16, usY+rowCount, Forecolor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
				else
					Paint_DrawPoint( usX+bitCount+16, usY+rowCount, BackColor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
			}			
		}		
	}		
}
```

### 取字模数据
1. 先计算特定汉字在字模中的偏移量
2. 通过`read_buf(pBuffer, (uint8_t*)&gbk16 + pos, f_size*f_size/8);` 通过偏移量找到汉字在字模中数据的首地址，再根据汉字像素大小，读取汉字对应的显示点阵数据，存入pBuffer
```c
uint8_t GetGBKCode_from_EXFlash( uint8_t * pBuffer, uint16_t c, uint8_t f_size, uint8_t myFont)
{ 
	uint8_t         High8bit,Low8bit;
	uint32_t        pos;
	static uint8_t  everRead=0;
	
	High8bit= c >> 8;     /* 取高8位数据 */
	Low8bit= c & 0x00FF;  /* 取低8位数据 */		
	

	if(High8bit<=0xA9)  // 显示01~09区特殊符号
	{
		pos = ((High8bit-0xa1)*94+(Low8bit-0xa1))*f_size*f_size/8; //计算特定汉字在字模中的偏移量
	}
	else // 显示16~87区汉字
	{
		pos = ((High8bit-0xa7)*94+(Low8bit-0xa1))*f_size*f_size/8; //计算特定汉字在字模中的偏移量	
	}

	read_buf(pBuffer, (uint8_t*)&gbk16 + pos, f_size*f_size/8);
	// SPI_FLASH_BufferRead(pBuffer,GBK_START_ADDRESS+pos,f_size*f_size/8); //读取字库数据
	return 0;
}
```

## 按键的控制

* 通过短按与长按结合,分别对K1与K2按键操作,实现翻页,确认,与返回的功能,下面是阅读电子书内容的按键设置
```c
static void on_btn(wnd_evt_btn_t *evt_btn)
{
	// int rd_size;
	uint32_t prev_pos = -1;
	reader_window_ctx_t *ctx = &reader_wnd_ctx;
	rt_kprintf("%s, %d, key=%d, action=%d\r\n",__FILE__, __LINE__, evt_btn->pin, evt_btn->action);
	if (BSP_KEY1_PIN == evt_btn->pin)
	{
		if (ctx->next_pos == ctx->file_size)  // 读取到了结尾
		{

		}else{
			if (BUTTON_CLICKED == evt_btn->action) /* next page */
			{
				ctx->curr_pos = ctx->next_pos;
				window_send_evt_refresh();
			}
			else if (BUTTON_LONG_PRESSED == evt_btn->action) /* load setting window */
			{
				window_load(SETTING_WND_ID, NULL);
			}
		}
	}
	else if (BSP_KEY2_PIN == evt_btn->pin)
	{
		if (BUTTON_CLICKED == evt_btn->action) /* prev page */
		{
			if (ctx->curr_pos > 0)
			{
				// ctx->curr_pos -= sizeof(read_buffer);
				ctx->curr_pos = ctx->curr_pos - (ctx->next_pos - ctx->curr_pos); /* 假设当前页的长度和上一页相同 */
				if (ctx->curr_pos < 0)
				{
					ctx->curr_pos = 0;
				}

				window_send_evt_refresh();
			}
			
		}
		else if (BUTTON_LONG_PRESSED == evt_btn->action) /* load dir window */
		{
			window_load(DIR_WND_ID, NULL);
		}

	}
}
```
## 窗口设置
* 设置了三个窗口:分为目录窗口(`dir_window`),阅读窗口(`reader_window`)和字体设置窗口(`sitting_window`)
1. 目录窗口
	主界面展示 \disk 目录下的.txt格式文档,通过按键选择想要查看的电子书籍.
```c
	 /* list directory */
    if (dfs_file_open(&fd, path, O_DIRECTORY) == 0)
    {
        rt_kprintf("Directory %s:\n", path);
        do
        {
            memset(&dirent, 0, sizeof(struct dirent));
            length = dfs_file_getdents(&fd, &dirent, sizeof(struct dirent));
            if (length > 0)
            {
                memset(&stat, 0, sizeof(struct stat));

                /* build full path for each file */
                fullpath = dfs_normalize_path(path, dirent.d_name);
                if (fullpath == NULL)
                    break;

                if (dfs_file_stat(fullpath, &stat) == 0)
                {
                    if (!S_ISDIR(stat.st_mode))
                    {
						if (strstr(dirent.d_name, ".txt"))
						{
							strncpy(ctx->book_name_list[ctx->book_num], dirent.d_name, BOOK_NAME_MAX - 1);
							ctx->book_name_list[ctx->book_num][BOOK_NAME_MAX - 1] = 0;
							ctx->book_num++;
							

							rt_kprintf("1\n");

							rt_kprintf("%s\n",dirent.d_name);

						}
                    }
                }
                rt_free(fullpath);
            }
        }
        while (length > 0);

        dfs_file_close(&fd);
    }
```
2. 阅读窗口
	进入阅读窗口查看电子书内容,从文件中读取一页的数据`FILE_READ_SIZE`存在`read_buffer`中最终显示在屏幕上,并且通过按键控制实现上下翻页的功能
```c
static void BSP_DispPage_EPD( uint16_t usX, uint16_t usY, uint8_t f_size, uint8_t myFont, uint8_t wordspace, uint8_t usRowL)
{
	uint32_t    TXT_SIZE;
	double      Reading_Percentage; // 阅读进度百分比计算结果
	char        Reading_Percentage_Save[20]; // 阅读进度最终显示结果
    bool        F_READ_END = false;	
	
	char        BDE_pBuffer[50];
	uint8_t     file_name_length;
	reader_window_ctx_t *ctx = &reader_wnd_ctx;
	int rd_size;

    //----------------------------------------------------------------------------------------
	Paint_Clear(WHITE);
	if ((ctx->fid > 0) && (ctx->curr_pos < ctx->file_size))
	{	
		lseek(ctx->fid, ctx->curr_pos, SEEK_SET);  // 移动到要读取的位置

		rt_kprintf("Now Reading Address = %d\r\n", ctx->curr_pos);							
		
		memset((void *)read_buffer, 0, sizeof(read_buffer));
		rd_size = read(ctx->fid, read_buffer, FILE_READ_SIZE);  //从文件中取一页的数据			
		SSD1677_Display_Page(usX, usY, f_size, myFont, wordspace, usRowL, (char *)read_buffer);	//在屏幕上显示	
		
		ctx->next_pos = ctx->curr_pos + Page_Counter_EN_CH_Space; // 记录位置变量更新				
		if (ctx->next_pos >= ctx->file_size)  // 读取到了结尾
		{
			ctx->next_pos = ctx->file_size;
		}			

		Display_down_handle();
	}
}
```
3. 字体设置窗口
	字体设置分为:`H24_SongTi`,`H32_KaiTi`两种模式,调用字库和字体大小,可以通过按键进行切换确认,实现字体和大小的变更,最终显示在屏幕上
```c
static const font_desc_t font_db[] = 
{
	{
		.name = "H24_SongTi",
		.id = SongTi,
		.size = 24,
	},
	{
		.name = "H32_KaiTi",
		.id = KaiTi,
		.size = 32,
	}
};
```

## 参考开源项目
* 上层具体应用可参考以下链接
```notice
https://oshwhub.com/hmgs271828/book_reader
```
