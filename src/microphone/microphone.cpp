#include "microphone.h"

// [修复] 告诉 C++ 编译器，这是一个 C 语言头文件
extern "C" {
#include "es8218e.h" 
}

// 静态句柄
static i2s_chan_handle_t rx_handle = NULL;
static bool is_initialized = false;

// CyberPi 引脚定义
#define MIC_BCLK  13
#define MIC_WS    14
#define MIC_DIN   35

void microphone_init(void) {
    if (is_initialized) return;

    // 1. 创建 I2S 通道
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    if (i2s_new_channel(&chan_cfg, NULL, &rx_handle) != ESP_OK) return;

    // 2. 配置标准模式
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000), 
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)MIC_BCLK,
            .ws = (gpio_num_t)MIC_WS,
            .dout = I2S_GPIO_UNUSED,
            .din = (gpio_num_t)MIC_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    // 3. 初始化并启用
    i2s_channel_init_std_mode(rx_handle, &std_cfg);
    i2s_channel_enable(rx_handle);

    // 4. MCLK 时钟输出 (ES8218E 必须)
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
    
    // 5. 启动解码芯片 (调用 C 函数)
    es8218e_start();
    
    is_initialized = true;
}

int microphone_get_loudness(void) {
    if (!rx_handle || !is_initialized) return 0;

    size_t bytes_read = 0;
    int16_t buffer[128]; 
    
    // 非阻塞读取
    esp_err_t res = i2s_channel_read(rx_handle, buffer, sizeof(buffer), &bytes_read, 10);
    
    if (res == ESP_OK && bytes_read > 0) {
        long sum = 0;
        int samples = bytes_read / 2;
        for (int i = 0; i < samples; i++) {
            sum += abs(buffer[i]);
        }
        
        int val = sum / samples;
        return val * 5; 
    }
    return 0;
}