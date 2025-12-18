#include <Arduino.h>
#include "cyberpi.h"

CyberPi cyber;

unsigned long previousMillis = 0;
int frameCount = 0;
float fps = 0.0;

void setup() {
  cyber.begin();
  cyber.set_lcd_light(true);
  cyber.clean_lcd();
}

void loop() {
  unsigned long currentMillis = millis();
  frameCount++;

  // 计算FPS
  if (currentMillis - previousMillis >= 1000) {
    fps = frameCount / ((currentMillis - previousMillis) / 1000.0);
    previousMillis = currentMillis;
    frameCount = 0;
  }

  // 清空屏幕
  cyber.clean_lcd();

  // 显示FPS (使用create_text和set_bitmap方法)
  char fpsStr[20];
  sprintf(fpsStr, "FPS: %.1f", fps);
  
  // 转换为宽字符
  wchar_t wfpsStr[20];
  mbstowcs(wfpsStr, fpsStr, sizeof(wfpsStr)/sizeof(wchar_t));
  
  // 创建文字位图
  Bitmap* fpsBitmap = cyber.create_text(wfpsStr, 0xF800, 16);
  
  // 绘制文字
  cyber.set_bitmap(10, 10, fpsBitmap);
  
  // 释放内存
  if (fpsBitmap) {
    free(fpsBitmap->buffer);
    free(fpsBitmap);
  }

  // 渲染LCD
  cyber.render_lcd();
}