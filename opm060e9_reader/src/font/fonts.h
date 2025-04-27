#ifndef __FONT_H
#define __FONT_H

#include <board.h>


// const uint8_t ASCII8x16_Table[ ];
// const uint8_t ASCII12x24_Table[ ];
// const uint8_t ASCII16x32_Table[ ];

/** @defgroup FONTS_Exported_Types
  * @{
  */
typedef struct _tFont
{
    const uint8_t *table;
    uint16_t Width;
    uint16_t Height;

} sFONT;

extern sFONT Font24x32;
extern sFONT Font16x24;
extern sFONT Font8x16;


/*******************����********** ����ʾ������ʾ���ַ���С ***************************/
#define      WIDTH_CH_CHAR                      16      //�����ַ����� 
#define      HEIGHT_CH_CHAR                     16        //�����ַ��߶� 

/* �����������ʹ�ñ���32*32��ģ ���� ILI9341_DispChar_CH ���������ȡ�ֽ����ݹ���Ϊ��Ӧ�ֽڸ�����

    //ȡ��4���ֽڵ����ݣ���lcd�ϼ���һ�����ֵ�һ��
        usTemp = ucBuffer [ rowCount * 4 ];
        usTemp = ( usTemp << 8 );
        usTemp |= ucBuffer [ rowCount * 4 + 1 ];
        usTemp = ( usTemp << 8 );
        usTemp |= ucBuffer [ rowCount * 4 + 2 ];
        usTemp = ( usTemp << 8 );
        usTemp |= ucBuffer [ rowCount * 4 + 3 ];

*/

#define LINE(x) ((x) * (((sFONT *)LCD_GetFont())->Height))

//LINEYͳһʹ�ú�����ģ�ĸ߶�
#define LINEY(x) ((x) * (WIDTH_CH_CHAR))




//0��ʾʹ��SD����ģ�������ʾFLASH��ģ,����SD����ģ���ļ�ϵͳ���ٶ����ܶࡣ

/*ʹ��FLASH��ģ*/
/*�����ֿ�洢��FLASH����ʼ��ַ*/
/*FLASH*/
#define GBKCODE_START_ADDRESS   1*4096


/*��ȡ�ֿ�ĺ���*/
//�����ȡ�����ַ���ģ����ĺ�������ucBufferΪ�����ģ��������usCharΪ�����ַ��������룩
// #define      GetGBKCode( ucBuffer, usChar )  GetGBKCode_from_EXFlash( ucBuffer, usChar )
// int GetGBKCode_from_EXFlash( uint8_t * pBuffer, uint16_t c);




#endif /*end of __FONT_H    */
