#ifndef __MICROPHONE_H__
#define __MICROPHONE_H__

#include <Arduino.h>
#include "driver/i2s_std.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化麦克风 (配置 I2S 和 ES8218E)
void microphone_init(void);

// 获取环境音量 (0-100+)
int microphone_get_loudness(void);

#ifdef __cplusplus
}
#endif

#endif