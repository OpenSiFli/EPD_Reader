
#define GPIO_PIN_RESET(pin) ((GPIO1_TypeDef *)hwp_gpio1)->DOCR0 = 1 << (pin)
#define GPIO_PIN_SET(pin)   ((GPIO1_TypeDef *)hwp_gpio1)->DOSR0 = 1 << (pin)

#define SDA_H() GPIO_PIN_SET(EPD_SDA)
#define SDA_L() GPIO_PIN_RESET(EPD_SDA)

#define SCLK_H() GPIO_PIN_SET(EPD_SCK)
#define SCLK_L() GPIO_PIN_RESET(EPD_SCK)

#define nCS_H() GPIO_PIN_SET(EPD_CS)
#define nCS_L() GPIO_PIN_RESET(EPD_CS)

#define nDC_H() GPIO_PIN_SET(EPD_DC)
#define nDC_L() GPIO_PIN_RESET(EPD_DC)

#define nRST_H() GPIO_PIN_SET(EPD_RES)
#define nRST_L() GPIO_PIN_RESET(EPD_RES)

