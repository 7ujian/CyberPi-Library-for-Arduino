#include "i2c.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// 确保引脚定义存在
#ifndef I2C_MASTER_SDA_IO
#define I2C_MASTER_SDA_IO 19
#endif
#ifndef I2C_MASTER_SCL_IO
#define I2C_MASTER_SCL_IO 18
#endif

void i2c_init()
{
    int i2c_master_port = I2C_NUM_1;
    i2c_config_t conf;
    
    // [关键修复1] 彻底清零结构体，防止垃圾数据导致 Panic
    memset(&conf, 0, sizeof(i2c_config_t)); 

    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    
    // [关键修复2] 降速到 100kHz 提高兼容性
    conf.master.clk_speed = 100000; 

    // [关键修复3] 只有旧版 ESP-IDF 才需要这个 flags，3.0 直接注释掉
    // conf.clk_flags = 0; 

    // [关键修复4] 防止重复初始化导致的死锁，先卸载驱动
    i2c_driver_delete(i2c_master_port);

    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
}

void i2c_write(uint8_t addr, uint8_t reg, uint8_t val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, 1);
    i2c_master_write_byte(cmd, reg, 1);
    i2c_master_write_byte(cmd, val, 1);
    i2c_master_stop(cmd);
    // [关键修复5] 使用正确的超时宏
    i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

uint8_t i2c_read(uint8_t addr, uint8_t reg)
{
    uint8_t data = 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, 1);
    i2c_master_write_byte(cmd, reg, 1);
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, 1);
    i2c_master_read_byte(cmd, &data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return data;
}

void i2c_write_data(uint8_t slaver_addr, uint8_t reg_addr, uint8_t *data, uint16_t size)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();  
    i2c_master_start(cmd);                         
    i2c_master_write_byte(cmd, (slaver_addr << 1) | I2C_MASTER_WRITE, 1);
    i2c_master_write(cmd, &reg_addr, 1, 1);
    if(size > 0) {
        i2c_master_write(cmd, data, size, 1);
    }
    
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

void i2c_read_data(uint8_t slaver_addr, uint8_t start, uint8_t *buffer, uint16_t size)
{
    if (size == 0) return;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (slaver_addr << 1) | I2C_MASTER_WRITE, 1);
    i2c_master_write(cmd, &start, 1, 1);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (slaver_addr << 1) | I2C_MASTER_READ, 1);
    if(size > 1) {
        i2c_master_read(cmd, buffer, size - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, buffer + size - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}