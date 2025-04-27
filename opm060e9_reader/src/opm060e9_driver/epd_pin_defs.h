


#ifdef EPD_DRIVER_LCDC

    #define TPS_SDA   33
    #define TPS_SCL  3
    #define TPS_WAKEUP 27
    #define TPS_PWRCOM 5
    #define TPS_PWRUP 20
    #define EPD_LE 30
    #define EPD_OE 31
    #define EPD_SPH 6
    #define EPD_STV 9
    #define EPD_CPV 29
    #define EPD_GMODE 28
    #define EPD_KEY 11
    #define EPD_CLK 4
    #define EPD_D0  7
    #define EPD_D1  8
    #define EPD_D2  37
    #define EPD_D3  39
    #define EPD_D4  40
    #define EPD_D5  41
    #define EPD_D6  42
    #define EPD_D7  43

#else

    #define TPS_SDA   40
    #define TPS_SCL  39
    #define TPS_WAKEUP 27
    #define TPS_PWRCOM 41
    #define TPS_PWRUP 20
    #define EPD_LE 30
    #define EPD_OE 31
    #define EPD_SPH 28
    #define EPD_STV 9
    #define EPD_CPV 29
    #define EPD_GMODE 42
    #define EPD_KEY 11
    #define EPD_CLK 33
    #define EPD_D0  0
    #define EPD_D1  1
    #define EPD_D2  2
    #define EPD_D3  3
    #define EPD_D4  4
    #define EPD_D5  5
    #define EPD_D6  6
    #define EPD_D7  7

#endif



#define GPIO_PIN_RESET(pin) (((pin) < 32) ? (((GPIO1_TypeDef *)hwp_gpio1)->DOCR0 = 1 << (pin)) : (((GPIO1_TypeDef *)hwp_gpio1)->DOCR1 = 1 << (pin - 32)))
#define GPIO_PIN_SET(pin)   (((pin) < 32) ? (((GPIO1_TypeDef *)hwp_gpio1)->DOSR0 = 1 << (pin)) : (((GPIO1_TypeDef *)hwp_gpio1)->DOSR1 = 1 << (pin - 32)))



#define TPS_WAKEUP_H_hs() GPIO_PIN_SET(TPS_WAKEUP)
#define TPS_WAKEUP_L_hs() GPIO_PIN_RESET(TPS_WAKEUP)

#define TPS_PWRCOM_H_hs() GPIO_PIN_SET(TPS_PWRCOM)
#define TPS_PWRCOM_L_hs() GPIO_PIN_RESET(TPS_PWRCOM)

#define TPS_PWRUP_H_hs() GPIO_PIN_SET(TPS_PWRUP)
#define TPS_PWRUP_L_hs() GPIO_PIN_RESET(TPS_PWRUP)

#define EPD_CLK_H_hs() GPIO_PIN_SET(EPD_CLK)
#define EPD_CLK_L_hs() GPIO_PIN_RESET(EPD_CLK)

#define EPD_LE_H_hs() GPIO_PIN_SET(EPD_LE)
#define EPD_LE_L_hs() GPIO_PIN_RESET(EPD_LE)

#define EPD_OE_H_hs() GPIO_PIN_SET(EPD_OE)
#define EPD_OE_L_hs() GPIO_PIN_RESET(EPD_OE)

#define EPD_SPH_H_hs() GPIO_PIN_SET(EPD_SPH)
#define EPD_SPH_L_hs() GPIO_PIN_RESET(EPD_SPH)

#define EPD_STV_H_hs() GPIO_PIN_SET(EPD_STV)
#define EPD_STV_L_hs() GPIO_PIN_RESET(EPD_STV)

#define EPD_CPV_H_hs() GPIO_PIN_SET(EPD_CPV)
#define EPD_CPV_L_hs() GPIO_PIN_RESET(EPD_CPV)

#define EPD_GMODE_H_hs() GPIO_PIN_SET(EPD_GMODE)
#define EPD_GMODE_L_hs() GPIO_PIN_RESET(EPD_GMODE)