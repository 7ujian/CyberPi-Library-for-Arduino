#include "cyberpi.h"

CyberPi cyber;

// FPS变量
unsigned long previousMillis = 0;
int frameCount = 0;
float fps = 0.0;

void setup() 
{
    Serial.begin(112500);
    delay(1000);
    cyber.begin();
}

void loop() 
{
    // 计算FPS
    unsigned long currentMillis = millis();
    frameCount++;
    if (currentMillis - previousMillis >= 1000) {
        fps = frameCount / ((currentMillis - previousMillis) / 1000.0);
        previousMillis = currentMillis;
        frameCount = 0;
    }
    
    // 清空屏幕
    cyber.clean_lcd();
    
    // 显示多语言文字（保留原始功能）
    int font_size = 16;
    Bitmap *bitmap1 = cyber.create_text(L"你好", 0xffff, font_size);
    cyber.set_bitmap(4, 4, bitmap1);
    Bitmap *bitmap2 = cyber.create_text(L"簡體", 0xff00, font_size);
    cyber.set_bitmap(4, 24, bitmap2);
    Bitmap *bitmap3 = cyber.create_text(L"hello", 0x00ff, font_size);
    cyber.set_bitmap(4, 44, bitmap3);
    Bitmap *bitmap4 = cyber.create_text(L"こんにちは", 0x0ff0, font_size);
    cyber.set_bitmap(4, 64, bitmap4);
    Bitmap *bitmap5 = cyber.create_text(L"여보세요", 0x0f0f, font_size);
    cyber.set_bitmap(4, 84, bitmap5);
    Bitmap *bitmap6 = cyber.create_text(L"Привет", 0xf0f0, font_size);
    cyber.set_bitmap(4, 104, bitmap6);
    
    // 显示FPS
    char fpsStr[20];
    sprintf(fpsStr, "FPS: %.1f", fps);
    
    // 转换为宽字符
    wchar_t wfpsStr[20];
    mbstowcs(wfpsStr, fpsStr, sizeof(wfpsStr)/sizeof(wchar_t));
    
    // 创建FPS文字位图
    Bitmap *fpsBitmap = cyber.create_text(wfpsStr, 0xf800, 16);
    
    // 绘制FPS文字（右上角）
    cyber.set_bitmap(80, 4, fpsBitmap);
    
    // 释放内存
    if (bitmap1) { free(bitmap1->buffer); free(bitmap1); }
    if (bitmap2) { free(bitmap2->buffer); free(bitmap2); }
    if (bitmap3) { free(bitmap3->buffer); free(bitmap3); }
    if (bitmap4) { free(bitmap4->buffer); free(bitmap4); }
    if (bitmap5) { free(bitmap5->buffer); free(bitmap5); }
    if (bitmap6) { free(bitmap6->buffer); free(bitmap6); }
    if (fpsBitmap) { free(fpsBitmap->buffer); free(fpsBitmap); }
    
    // 渲染LCD
    cyber.render_lcd();
}