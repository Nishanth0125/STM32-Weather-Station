# STM32 Weather Station 

A bare-metal embedded project on **STM32 Nucleo F446RE** that reads real-time environmental data from BMP280 and DHT11 sensors and displays it on an SSD1306 OLED display. The BMP280 driver is implemented from scratch using direct I2C register access — no external sensor library used.

---




**Screen 1 — BMP280 Data:**

```
BMP280 Sensor
Temp: 27.4 C
Pres: 972 hPa
BTN: DHT11 >>
```

**Screen 2 — DHT11 Data (press blue button):**

```
DHT11 Sensor
Temp: 27 C
Humi: 65 %
<< BTN: BMP280
```

---

## Hardware

| Component | Description | Interface |
|-----------|-------------|-----------|
| STM32 Nucleo F446RE | Main microcontroller @ 84MHz | — |
| BMP280 | Pressure + Temperature sensor | I2C |
| DHT11 | Temperature + Humidity sensor | Single-wire |
| SSD1306 OLED 0.96" | 128x64 pixel display | I2C |

---

## Features

- **BMP280 driver written from scratch** — direct I2C register access, no library
- BMP280 datasheet **compensation formulas** implemented for accurate readings
- **DHT11 single-wire protocol** bit-banged with microsecond timer (TIM6)
- **SSD1306 OLED** dual screen display
- **Blue button** switches between BMP280 and DHT11 screens
- **EXTI interrupt** on PC13 with software debounce (200ms)
- **UART debug output** at 115200 baud via USART2 (ST-Link virtual COM)
- **TIM6** microsecond delay for DHT11 timing
- **Timeout protection** on DHT11 reads — prevents infinite hang on disconnect

---

## System Architecture

```
┌─────────────────────────────────────────────────────┐
│                STM32F446RE @ 84MHz                  │
│                                                     │
│  ┌──────────┐    I2C1     ┌─────────┐               │
│  │  BMP280  │◄───────────►│         │               │
│  │ 0x76     │    PB6/PB7  │  Main   │               │
│  └──────────┘             │  Loop   │               │
│                           │         │◄── TIM2 ISR   │
│  ┌──────────┐    I2C1     │  1000ms │    LED blink  │
│  │ SSD1306  │◄───────────►│         │               │
│  │  OLED    │             └────┬────┘               │
│  └──────────┘                  │                    │
│                                │                    │
│  ┌──────────┐  Single-wire     │                    │
│  │  DHT11   │◄─────────────────┘                    │
│  │   PA1    │   TIM6 us delay                       │
│  └──────────┘                                       │
│                                                     │
│  ┌──────────┐  EXTI13                               │
│  │  Button  │──────────► display_mode toggle        │
│  │   PC13   │            debounced 200ms            │
│  └──────────┘                                       │
│                                                     │
│  USART2 ──────────────────► ST-Link Virtual COM     │
│  PA2/PA3                    115200 baud             │
└─────────────────────────────────────────────────────┘
```

---

## Pin Connections

### BMP280 + SSD1306 OLED — Shared I2C1 Bus

| Sensor Pin | Nucleo F446RE | Notes |
|------------|---------------|-------|
| VCC | 3.3V | Do NOT use 5V |
| GND | GND | |
| SDA | PB7 | I2C1_SDA |
| SCL | PB6 | I2C1_SCL |
| CSB (BMP280) | 3.3V | Forces I2C mode |
| SDO (BMP280) | GND | Sets address to 0x76 |

### DHT11

| DHT11 Pin | Nucleo F446RE | Notes |
|-----------|---------------|-------|
| VCC | 3.3V | |
| GND | GND | |
| DATA | PA1 | Bit-banged single wire |

### Onboard Peripherals

| Function | Pin | Notes |
|----------|-----|-------|
| Onboard LED | PA5 | Heartbeat blink |
| User Button | PC13 | EXTI interrupt |
| UART TX | PA2 | USART2 to ST-Link |
| UART RX | PA3 | USART2 from ST-Link |

---

## BMP280 Register Map Used

| Register | Address | Purpose |
|----------|---------|---------|
| CHIP_ID | 0xD0 | Verify chip (returns 0x58 or 0x60) |
| CTRL_MEAS | 0xF4 | Set oversampling + mode |
| CONFIG | 0xF5 | Set filter + standby time |
| PRESS_MSB | 0xF7 | Raw pressure data |
| TEMP_MSB | 0xFA | Raw temperature data |
| CALIB | 0x88 | 24 bytes calibration data |

**Register Configuration:**
```c
BMP280_WriteReg(0xF4, 0x57);  // temp x2, pressure x16, normal mode
BMP280_WriteReg(0xF5, 0x10);  // filter x4, standby 250ms
```

---

## I2C Device Addresses

| Device | Address | Condition |
|--------|---------|-----------|
| BMP280 | 0x76 | SDO → GND |
| SSD1306 OLED | 0x3C | Default |

---

## Project Structure

```
STM32-Weather-Station/
├── Core/
│   ├── Inc/
│   │   ├── main.h
│   │   ├── ssd1306.h
│   │   └── ssd1306_fonts.h
│   ├── Src/
│   │   ├── main.c          ← BMP280 driver + DHT11 + display logic
│   │   ├── ssd1306.c
│   │   └── ssd1306_fonts.c
│   └── Startup/
│       └── startup_stm32f446retx.s
├── Drivers/
│   └── STM32F4xx_HAL_Driver/
├── Nucleo_BMP280.ioc       ← CubeMX configuration
├── STM32F446RETX_FLASH.ld  ← Linker script
└── README.md
```

---

## How to Build and Flash

### Requirements
- STM32CubeIDE v2.x
- STM32CubeHAL F4 v1.28.x
- STM32 Nucleo F446RE board
- Mini-USB cable

### Steps
1. Clone this repository
```bash
git clone https://github.com/Nishanth0125/STM32-Weather-Station.git
```
2. Open **STM32CubeIDE**
3. **File → Open Projects from File System** → select folder
4. Build: **Ctrl+B**
5. Flash: **Ctrl+F11**

### View Debug Output
1. Open **PuTTY**
2. Connection type → **Serial**
3. Serial line → **COMx** (check Device Manager)
4. Speed → **115200**
5. Click **Open**

Expected output:
```
BMP280 found! ID: 0x58
Mode: 0 | BMP:32.4C 972hPa | DHT:29C 65%
Mode: 0 | BMP:32.4C 972hPa | DHT:29C 65%
```

---

## Key Implementation Details

### BMP280 Compensation Formula
Raw ADC values from BMP280 are meaningless without applying factory calibration. The sensor stores 24 bytes of unique calibration coefficients at address 0x88. These are used in the compensation formulas from the BMP280 datasheet:

```c
// t_fine must be calculated first — pressure formula depends on it
float BMP280_CompensateTemp(int32_t adc_T)
{
    int32_t var1, var2;
    var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1)))
            * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1))
            * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12)
            * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    return (float)((t_fine * 5 + 128) >> 8) / 100.0f;
}
```

### Button Debounce
Hardware buttons bounce 5–50ms on press. Time-based software debounce ignores presses within 200ms of the last valid press:

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin == GPIO_PIN_13)
    {
        static uint32_t last_press = 0;
        if(HAL_GetTick() - last_press > 200)
        {
            display_mode ^= 1;
            last_press = HAL_GetTick();
        }
    }
}
```

### DHT11 Timeout Protection
All DHT11 polling loops have 100ms timeouts to prevent infinite hang if sensor disconnects:

```c
timeout = HAL_GetTick();
while(!(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1)))
{
    if(HAL_GetTick() - timeout > 100) return 0;
}
```

---

## What I Learned

- Direct **I2C register access** without any sensor abstraction library
- Reading and implementing **BMP280 datasheet compensation formulas**
- **Single-wire protocol** bit-banging with microsecond precision using hardware timer
- **EXTI interrupt** configuration and software debouncing
- **Volatile variables** for safe ISR ↔ main loop communication
- **Timeout protection** on sensor reads for robust firmware
- STM32 **HAL peripheral initialization** and CubeMX code generation
- **SSD1306 OLED** multi-screen state machine display controller

---

## Development Environment

| Tool | Version |
|------|---------|
| STM32CubeIDE | v2.1.1 |
| STM32CubeHAL F4 | v1.28.3 |
| Target MCU | STM32F446RE |
| System Clock | 84 MHz (HSI + PLL) |
| Debugger | ST-Link V2 (onboard) |

---

## Future Improvements

- [ ] FreeRTOS — separate tasks for sensor reading and display
- [ ] UART receive interrupt — control display mode from PC
- [ ] DMA-based UART transmission
- [ ] Moving average filter on BMP280 readings
- [ ] Battery level indicator (ADC)
- [ ] Sleep mode between readings for power saving


