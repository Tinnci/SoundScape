#define USER_SETUP_INFO "User_Setup"

// 定义显示屏驱动类型（根据实际显示屏型号选择）
#define ST7789_DRIVER  // 或 #define ILI9341_DRIVER

// 定义显示屏分辨率
#define TFT_WIDTH  240  // 根据实际屏幕调整
#define TFT_HEIGHT 320  // 根据实际屏幕调整

// 定义引脚
#define TFT_MISO -1
#define TFT_MOSI 11  // LCD_SDA
#define TFT_SCLK 12  // LCD_SCL
#define TFT_CS   3   // LCD_CS
#define TFT_DC   46  // LCD_DC
#define TFT_RST  9   // LCD_RST
#define TFT_BL   8   // LCD_BL

// 禁用触摸功能
#define TOUCH_CS -1  // 设置为-1以禁用触摸功能

// Force the use of HSPI (SPI3) port on ESP32-S3 (Fix for potential crashes)
#define USE_HSPI_PORT

// 定义颜色位深
#define SPI_FREQUENCY  30000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000

// 其他可选配置
#define LOAD_GLCD   // 字体 1
#define LOAD_FONT2  // 字体 2
#define LOAD_FONT4  // 字体 4
#define LOAD_FONT6  // 字体 6
#define LOAD_FONT7  // 字体 7
#define LOAD_FONT8  // 字体 8
#define LOAD_GFXFF  // FreeFonts

#define SMOOTH_FONT
