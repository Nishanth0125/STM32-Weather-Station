# STM32-Weather-Station
embedded project on STM32 Nucleo F446RE 
displaying real-time sensor data on SSD1306 OLED.

## Hardware Used
- STM32 Nucleo F446RE
- BMP280 — Pressure + Temperature (I2C)
- DHT11 — Temperature + Humidity (Single-wire)
- SSD1306 OLED 0.96" (I2C)

## Features
- BMP280 driver written from scratch (no library)
- DHT11 single-wire protocol implementation
- OLED dual screen switchable via button press
- EXTI interrupt with hardware debounce
- UART debug output at 115200 baud
- TIM6 microsecond delay timer

## Pin Connections

### BMP280 + OLED → I2C1
| Pin | Nucleo |
|-----|--------|
| SDA | PB7 |
| SCL | PB6 |

### DHT11
| Pin | Nucleo |
|-----|--------|
| DATA | PA1 |

## How to Build
1. Open in STM32CubeIDE
2. Build with Ctrl+B
3. Flash via ST-Link (Ctrl+F11)
4. PuTTY → COM port → 115200 baud

## Environment
- STM32CubeIDE v2.1.1
- HAL F4 v1.28.3
- STM32F446RE @ 84MHz
