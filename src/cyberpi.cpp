#include "cyberpi.h"

// [重要] 移除旧版驱动引用，防止 ADC 冲突
// #include "driver/i2s.h" 
// #include "driver/dac.h"

// 引用旧的 C 语言驱动
extern "C" { 
#include "io/aw9523b.h"
#include "lcd/lcd.h"
#include "i2c/i2c.h"
#include "gyro/gyro.h"
// [新增] 麦克风模块 (C 链接)
#include "microphone/microphone.h"
}

// [新增] 声音模块 (C++ 链接)
#include "sound/sound.h"

// 全局变量
static uint16_t *_framebuffer;
static uint8_t *_led_data;
static uint8_t *_gyro_data; 
static SemaphoreHandle_t _render_ready;
static long lastTime = 0; // 这里的 lastTime 之前是在类里的，改为静态或使用类成员

CyberPi::CyberPi()
{ 
    _render_ready = xSemaphoreCreateBinary();
    _led_data = this->malloc(15);
    _gyro_data = this->malloc(14); 
    
    memset(_led_data, 0, 15);
}

void CyberPi::begin()
{
    // 1. 基础总线初始化
    i2c_init(); // 使用修复后的 i2c.c (memset清零版)
    aw_init();  // 初始化 IO 扩展芯片
    
    // 2. 硬件初始化
    begin_gpio();
    begin_lcd();  // 内部包含了打开电源的代码
    begin_gyro(); // 内部包含了 math.h 修复
    
    // 3. [重构] 调用新模块初始化
    begin_sound();      // 调用 sound_init (复活的 MySynth)
    //begin_microphone(); // 调用 microphone_init (i2s_std)

    // 4. 启动任务
    // 仅保留 LCD 刷新任务，防止其他空任务导致看门狗死锁
    TaskHandle_t threadTask_lcd;
    xTaskCreatePinnedToCore(this->_on_lcd_thread, "_on_lcd_thread", 4096, NULL, 10, &threadTask_lcd, 1);
}

void CyberPi::_on_lcd_thread(void *p)
{
    while(true)
    {
        if(xSemaphoreTake(_render_ready, 25) == pdTRUE)
        {
            lcd_draw(_framebuffer, 128, 128);
        }
    }
}

void CyberPi::begin_gpio()
{
    pinMode(33, INPUT); // 光线传感器
    // 配置 AW9523B 引脚模式
    aw_pinMode(AW_P1_3, AW_GPIO_MODE_OUTPUT); // 电源控制脚
    aw_pinMode(JOYSTICK_UP_IO, AW_GPIO_MODE_INPUT);
    aw_pinMode(JOYSTICK_DOWN_IO, AW_GPIO_MODE_INPUT);
    aw_pinMode(JOYSTICK_RIGHT_IO, AW_GPIO_MODE_INPUT);
    aw_pinMode(JOYSTICK_LEFT_IO, AW_GPIO_MODE_INPUT);
    aw_pinMode(JOYSTICK_CENTER_IO, AW_GPIO_MODE_INPUT);
    aw_pinMode(BUTTON_A_IO, AW_GPIO_MODE_INPUT);
    aw_pinMode(BUTTON_B_IO, AW_GPIO_MODE_INPUT);
    aw_pinMode(BUTTON_MENU_IO, AW_GPIO_MODE_INPUT);
}

void CyberPi::begin_lcd()
{
    // [关键] 强制打开外设电源（屏幕背光 + 功放）
    aw_pinMode(AW_P1_3, AW_GPIO_MODE_OUTPUT);
    aw_digitalWrite(AW_P1_3, HIGH);

    _framebuffer = (uint16_t*)this->malloc(128 * 128 * 2);
    memset(_framebuffer, 0, 128 * 128 * 2);
    lcd_init();
}

void CyberPi::set_lcd_light(bool on)
{
    if(on) lcd_on();
    else lcd_off();
}

// --- 字体绘制相关 ---
Bitmap* CyberPi::create_text(wchar_t*chars, uint16_t color, uint8_t font_size)
{
    Bitmap *bitmap = new Bitmap();
    int cx=0, cy=0, x=0, y=0;
    int i=0;
    uint32_t c;
    uint8_t font_width = font_size;
    uint8_t font_height = font_size;
    uint8_t font_max_height = font_size;
    uint8_t *buf = (uint8_t*)MALLOC_SPI(400); 
    bool elongate = false;
    uint8_t font = 10;
    
    int temp_i = 0;
    while((c = *(chars + temp_i)) != 0) {
        if(c == 0x08) cx -= font_width;
        else if(c == 0x0A) { cx = 0; cy += font_height; }
        else {
            get_utf8_data(c, c<256?6:font, buf, &elongate, &font_width, &font_height);
            cx += font_width*font_max_height/font_height+0.5f;
        }
        temp_i++;
    }
    bitmap->width = cx > 0 ? cx : 1; 
    bitmap->height = cy + font_max_height + 1;
    bitmap->buffer = (uint16_t*)this->malloc(bitmap->width * bitmap->height * 2);
    memset(bitmap->buffer, 0, bitmap->width * bitmap->height * 2);
    
    cx = 0; cy = 0; i = 0;
    while((c = *(chars + i)) != 0) {
        if(c == 0x08) cx -= font_width;
        else if(c == 0x0A) { cx = 0; cy += font_height; }
        else {
            get_utf8_data(c, c<256?6:font, buf, &elongate, &font_width, &font_height);
            read_char(bitmap, cx, cy, font_size, font_max_height, buf+(elongate?2:0), font_width, font_height, elongate, color);
            cx += font_width*font_max_height/font_height+0.5f;
        }
        i++;
    }
    free(buf);
    return bitmap;
}

void CyberPi::read_char(Bitmap* bitmap, int x, int y, float w, float h, uint8_t* buffer, float font_width, float font_height, bool elongate, uint16_t color)
{
    if(buffer)
    {
        float width = font_width, height = font_height;
        int widthBytes = (int)ceil((double)width / 8);
        float scale = h/height;
        if(scale==1) {
            for(int v=0; v<height; v++) {
                for(int u=0; u<width; u++) {
                    int b = u >> 3; 
                    int bit = ~u & 7;
                    if((buffer[widthBytes * v + b]>>bit)&1) {
                        if(x+u < bitmap->width && y+v < bitmap->height)
                            bitmap->buffer[x+u+(y+v)*bitmap->width] = color;
                    }
                }
            }
        }
        else {
            uint8_t *buf = (uint8_t*)this->malloc(width*height);
            memset(buf,0,width*height);
            for(int v=0; v<(int)height; v++) {
                for(int u=0; u<(int)width; u++) {
                    int b = u >> 3; 
                    int bit = ~u & 7;
                    if((buffer[widthBytes * v + b]>>bit)&1) {
                        buf[u+(v*(int)width)] = 1;
                    }
                }
            }
            for(int i=0; i<(int)(height*scale+0.5f); i++) {
                for(int j=0; j<(int)(width*scale+0.5f); j++) {
                    if(buf[(int)(i/scale)*(int)width+(int)(j/scale)]) {
                         if(x+j < bitmap->width && y+i < bitmap->height)
                            bitmap->buffer[x+j+(y+i)*bitmap->width] = color;
                    }
                }
            }
            free(buf);
        }
    }
}

void CyberPi::clean_lcd()
{
    memset(_framebuffer, 0x0, 128 * 128 * 2);
}

void CyberPi::set_lcd_pixel(uint8_t x, uint8_t y, uint16_t color)
{
    if(y>=0 && y<128 && x>=0 && x<128)
    {
        _framebuffer[y*128+x] = color;
    }
}

void CyberPi::set_bitmap(uint8_t x, uint8_t y, Bitmap* bitmap)
{
    for(int i=0; i<bitmap->height; i++)
    {
        for(int j=0; j<bitmap->width; j++)
        {
            if(x+j < 128 && y+i < 128)
                set_lcd_pixel(x+j, y+i, bitmap->buffer[i*bitmap->width+j]);
        }
    }
}

uint16_t CyberPi::color24_to_16(uint32_t rgb)
{
    return color24to16(rgb);
}

uint16_t CyberPi::swap_color(uint16_t color)
{
    return ((color&0xff)<<8) | (color>>8);
}

void CyberPi::render_lcd()
{
    // 使用静态变量 lastTime，或者在类定义中添加 private 变量
    // 这里假设使用之前的静态变量
    if(millis() - lastTime > 20)
    {
        lastTime = millis();
        xSemaphoreGive(_render_ready);
    }
}

void CyberPi::set_rgb(int idx, uint8_t red, uint8_t greeen, uint8_t blue)
{
    _led_data[idx*3] = red;
    _led_data[idx*3+1] = greeen;
    _led_data[idx*3+2] = blue;
    // 使用扩展芯片写 RGB 数据
    i2c_write_data(0x5B, REG_DIM00, _led_data, 15);
}

uint16_t CyberPi::get_gpio()
{
    return aw_get_gpio();
}

// 模拟读取 (使用 ESP32 3.0 的新驱动，无冲突)
uint16_t CyberPi::get_light()
{
    return analogRead(33);
}

// 摇杆和按键逻辑 (数字式)
int CyberPi::get_joystick_x()
{
    if(aw_digitalRead(JOYSTICK_RIGHT_IO)) return 1;
    if(aw_digitalRead(JOYSTICK_LEFT_IO)) return -1;
    return 0;
}

int CyberPi::get_joystick_y()
{
    if(aw_digitalRead(JOYSTICK_DOWN_IO)) return 1;
    if(aw_digitalRead(JOYSTICK_UP_IO)) return -1;
    return 0;
}

bool CyberPi::get_joystick_pressed() { return aw_digitalRead(JOYSTICK_CENTER_IO); }
bool CyberPi::get_button_a() { return aw_digitalRead(BUTTON_A_IO); }
bool CyberPi::get_button_b() { return aw_digitalRead(BUTTON_B_IO); }
bool CyberPi::get_button_menu() { return aw_digitalRead(BUTTON_MENU_IO); }

// 陀螺仪逻辑 (已修复 math.h)
void CyberPi::begin_gyro()
{
    gyro_init();
}
float CyberPi::get_gyro_x() { return get_gyro_data(0); }
float CyberPi::get_gyro_y() { return get_gyro_data(1); }
float CyberPi::get_gyro_z() { return get_gyro_data(2); }
float CyberPi::get_acc_x() { return get_acc_data(0); }
float CyberPi::get_acc_y() { return get_acc_data(1); }
float CyberPi::get_acc_z() { return get_acc_data(2); }
float CyberPi::get_roll() { return get_gyro_roll(); }
float CyberPi::get_pitch() { return get_gyro_pitch(); }

// --- [重构] 声音部分 (Sound) ---
void CyberPi::begin_sound()
{
    sound_init();
}

void CyberPi::set_pitch(uint8_t channel, uint8_t pitch, uint8_t time)
{
    // 调用新的 sound 模块 (Hardcore MSynth)
    // time 参数单位模糊，原库似乎是 10ms 为单位，这里做个转换
    sound_play_note(channel, pitch, time * 10);
}

// [新增] 暴露乐器设置接口
void CyberPi::set_instrument(uint8_t instrument)
{
    sound_set_instrument(instrument);
}

// --- [重构] 麦克风部分 (Microphone) ---
void CyberPi::begin_microphone()
{
    microphone_init();
}

int CyberPi::get_loudness()
{
    return microphone_get_loudness();
}

// 回调函数暂未实现，清空防止报错
void CyberPi::on_microphone_data(data_callback func)
{
    // _mic_callback = func; 
}

void CyberPi::on_sound_data(data_callback func)
{
    // _sound_callback = func;
}

// 内存分配 (保持不变)
uint8_t* CyberPi::malloc(uint32_t len)
{
    if (heap_caps_get_free_size( MALLOC_CAP_SPIRAM ) > 0)
    {
        return (uint8_t*)MALLOC_SPI(len);
    }
    return (uint8_t*)MALLOC_INTERNAL(len);
}

// --- 占位符函数 (保持类结构完整) ---

void CyberPi::_on_sound_thread(void*p)
{
    // 线程已废弃，任务不再创建，此函数不会被调用
    while(1) { delay(1000); }
}

void CyberPi::_on_sensor_thread(void* p) 
{
    // 线程已废弃
    while(1) { delay(1000); }
}

void CyberPi::_render_audio(uint8_t *audio_buf, uint16_t audio_buf_len)
{
    // 已废弃，sound.cpp 有自己的回调
}