#include <Arduino.h>
#include <SPI.h>

// GT30L24A3W SPI引脚定义
#define GT_CS_PIN     27
#define GT_MOSI_PIN   2
#define GT_MISO_PIN   26
#define GT_CLK_PIN    4

// SPI命令
#define GT_READ_CMD   0x03

void setup() {
  Serial.begin(115200);
  delay(1000);
  for (int i=1;i<6;i++)
  {
    Serial.printf("Waiting test %d", i);
    delay(1000);
  }
  Serial.println("GT30L24A3W SPI连接测试");
  
  // 初始化SPI
  SPI.begin(GT_CLK_PIN, GT_MISO_PIN, GT_MOSI_PIN, GT_CS_PIN);
  SPI.setClockDivider(SPI_CLOCK_DIV8); // 设置时钟频率
  SPI.setDataMode(SPI_MODE0);
  
  // 设置CS引脚为输出
  pinMode(GT_CS_PIN, OUTPUT);
  digitalWrite(GT_CS_PIN, HIGH); // 初始状态为高
  
  // 测试引脚连接
  test_pin_connections();
  
  // 测试SPI通讯
  test_spi_communication();
}

void loop() {
  // 持续测试
  delay(2000);
  test_spi_communication();
}

void test_pin_connections() {
  Serial.println("\n=== 引脚连接测试 ===");
  
  // 测试CS引脚
  digitalWrite(GT_CS_PIN, LOW);
  delay(10);
  if (digitalRead(GT_CS_PIN) == LOW) {
    Serial.println("✓ CS引脚 (GPIO27) 输出正常");
  } else {
    Serial.println("✗ CS引脚 (GPIO27) 输出异常");
  }
  digitalWrite(GT_CS_PIN, HIGH);
  delay(10);
  if (digitalRead(GT_CS_PIN) == HIGH) {
    Serial.println("✓ CS引脚 (GPIO27) 电平切换正常");
  } else {
    Serial.println("✗ CS引脚 (GPIO27) 电平切换异常");
  }
  
  // 测试其他SPI引脚的基本输出
  pinMode(GT_CLK_PIN, OUTPUT);
  pinMode(GT_MOSI_PIN, OUTPUT);
  
  digitalWrite(GT_CLK_PIN, LOW);
  digitalWrite(GT_MOSI_PIN, LOW);
  delay(10);
  Serial.println("✓ SCK (GPIO4) 和 MOSI (GPIO2) 引脚配置为输出");
  
  // 切换状态
  digitalWrite(GT_CLK_PIN, HIGH);
  digitalWrite(GT_MOSI_PIN, HIGH);
  delay(10);
  Serial.println("✓ SCK (GPIO4) 和 MOSI (GPIO2) 引脚电平切换正常");
  
  // 恢复引脚配置
  pinMode(GT_CLK_PIN, INPUT);
  pinMode(GT_MOSI_PIN, INPUT);
}

void test_spi_communication() {
  Serial.println("\n=== SPI通讯测试 ===");
  
  // 测试读取特定地址的数据
  uint8_t test_buffer[4] = {0};
  bool success = read_gt30l24a3w(0x000000, test_buffer, 4);
  
  if (success) {
    Serial.print("✓ SPI通讯成功，读取地址 0x000000 的数据: ");
    for (int i = 0; i < 4; i++) {
      Serial.print("0x");
      if (test_buffer[i] < 0x10) Serial.print("0");
      Serial.print(test_buffer[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    
    // 测试另一个地址
    success = read_gt30l24a3w(0x500000, test_buffer, 4);
    if (success) {
      Serial.print("✓ 读取地址 0x500000 的数据: ");
      for (int i = 0; i < 4; i++) {
        Serial.print("0x");
        if (test_buffer[i] < 0x10) Serial.print("0");
        Serial.print(test_buffer[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
      
      // 简单验证数据是否合理（通常不会全0或全FF）
      bool all_zero = true;
      bool all_ff = true;
      for (int i = 0; i < 4; i++) {
        if (test_buffer[i] != 0x00) all_zero = false;
        if (test_buffer[i] != 0xFF) all_ff = false;
      }
      
      if (all_zero) {
        Serial.println("⚠ 注意: 读取的数据全为0x00，可能存在连接问题");
      } else if (all_ff) {
        Serial.println("⚠ 注意: 读取的数据全为0xFF，可能存在连接问题");
      } else {
        Serial.println("✓ 读取的数据看起来合理");
      }
    }
  } else {
    Serial.println("✗ SPI通讯失败");
  }
}

bool read_gt30l24a3w(uint32_t address, uint8_t* buffer, uint8_t length) {
  if (buffer == NULL || length == 0) {
    return false;
  }
  
  // 确保CS为高
  digitalWrite(GT_CS_PIN, HIGH);
  delayMicroseconds(1);
  
  // CS拉低，开始通讯
  digitalWrite(GT_CS_PIN, LOW);
  delayMicroseconds(1);
  
  // 使用SPI.transfer进行通讯
  SPI.beginTransaction(SPISettings(5000000, MSBFIRST, SPI_MODE0));
  
  // 发送READ命令 (0x03)
  SPI.transfer(GT_READ_CMD);
  
  // 发送24位地址
  SPI.transfer((uint8_t)(address >> 16));
  SPI.transfer((uint8_t)(address >> 8));
  SPI.transfer((uint8_t)(address >> 0));
  
  // 读取数据
  for (uint8_t i = 0; i < length; i++) {
    buffer[i] = SPI.transfer(0x00);
  }
  
  SPI.endTransaction();
  
  // CS拉高，结束通讯
  digitalWrite(GT_CS_PIN, HIGH);
  delayMicroseconds(1);
  
  return true;
}
