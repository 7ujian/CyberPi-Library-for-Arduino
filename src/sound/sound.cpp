#include "sound.h"
#include <Arduino.h>

// 必须加 extern "C"，因为 aw9523b 是 C 文件
extern "C" {
#include "../io/aw9523b.h"
}

#include "synth.h" 

// --- 硬件参数 ---
#define SPEAKER_PIN 25 

// --- PWM 参数 (Class D 模拟) ---
// 载波频率: 64kHz (远超人耳范围，听感细腻)
// 分辨率: 8-bit (0-255)，完美匹配 MSynth 的输出
#define PWM_FREQ 64000 
#define PWM_RES  8      

// --- 全局对象 ---
static MSynth _synth; 
static TaskHandle_t audio_task_handle = NULL;
static bool is_init = false;

// --- 核心输出函数 ---
// 这是 MSynth 计算完一帧数据后调用的
void audio_callback_blocking(uint8_t *audio_buf, uint16_t audio_buf_len) {
    if (!is_init) return;
    
    for(int i=0; i<audio_buf_len; i++) {
        uint8_t sample = audio_buf[i]; 
        
        // [核心逻辑] PWM 模拟 DAC
        // ESP32 3.0 API: 直接写引脚，底层自动映射到通道
        ledcWrite(SPEAKER_PIN, sample);

        // [软件定时] 控制播放采样率 (20kHz)
        // 1秒 / 20000 = 50微秒
        // 虽然这里是阻塞忙等待，但在 Core 1 上运行不影响主程序
        esp_rom_delay_us(50); 
    }
}

// --- 音频生产线程 ---
void audio_task(void *pvParameter) {
    while (1) {
        if (is_init) {
            // 计算波形 -> 填满缓冲区 -> 调用 callback -> 输出 PWM
            _synth.render(); 
            
            // 极短的让步，防止看门狗 (WDT) 认为死锁
            vTaskDelay(1); 
        } else {
            vTaskDelay(100);
        }
    }
}

// --- 初始化 ---
void sound_init(void) {
    if (is_init) return;

    // 1. 打开功放电源 (AW9523B P1_3)
    aw_pinMode(AW_P1_3, AW_GPIO_MODE_OUTPUT);
    aw_digitalWrite(AW_P1_3, HIGH);

    // 2. 初始化 PWM (ESP32 3.0 新 API)
    // ledcAttach(pin, freq, resolution)
    // 成功返回 true
    if (!ledcAttach(SPEAKER_PIN, PWM_FREQ, PWM_RES)) {
        return; // 初始化失败
    }

    // 3. 初始化合成器
    _synth.begin(audio_callback_blocking);

    // 4. 创建高优先级任务 (Core 1)
    // 堆栈给大一点 (8192) 比较安全
    xTaskCreatePinnedToCore(audio_task, "AudioTask", 8192, NULL, 10, &audio_task_handle, 1);

    is_init = true;
}

// --- 接口封装 ---

void sound_play_note(uint8_t channel, uint8_t pitch, uint8_t duration) {
    if (!is_init) sound_init(); // 懒加载保护
    if (!is_init) return;
    _synth.addNote(channel, pitch, duration);
}

void sound_set_instrument(uint8_t instrument) {
    if (!is_init) return;
    _synth.setInstrument(instrument);
}

void sound_set_volume(uint8_t channel, uint8_t volume) {
    if (!is_init) return;
    _synth.setVolume(channel, volume);
}

void sound_stop(void) {
    if (is_init) {
        ledcWrite(SPEAKER_PIN, 0); // 占空比 0 = 静音
    }
}