/**
 * Copyright (c) 2017
 * Circuit Happy, LLC
 */

#include <iostream>
#include "missing_link/gpio.hpp"
#include "missing_link/io_expander.hpp"
#include "missing_link/led_driver.hpp"

using namespace std;
using namespace MissingLink;
using namespace MissingLink::GPIO;

// Register address definitions
enum LEDDriver::Register : uint8_t {
  MODE1     = 0x00, // Mode 1
  PWMSTART  = 0x02, // Start of 16 consecutive PWM registers
  LEDOUT0   = 0x14, // LED Output state 0
  LEDOUT1   = 0x15, // LED Output state 1
  LEDOUT2   = 0x16, // LED Output state 2
  LEDOUT3   = 0x17, // LED Output state 3
};

LEDDriver::LEDDriver(uint8_t i2cBus, uint8_t i2cAddress)
  : m_i2cDevice(unique_ptr<I2CDevice>(new I2CDevice(i2cBus, i2cAddress)))
{}

LEDDriver::~LEDDriver() {}

void LEDDriver::Configure() {
  // enable oscillator
  m_i2cDevice->WriteByte(MODE1, 0x04);
  // enable all LED groups for individual and group PWM control
  m_i2cDevice->WriteByte(LEDOUT0, 0xff);
  m_i2cDevice->WriteByte(LEDOUT1, 0xff);
  m_i2cDevice->WriteByte(LEDOUT2, 0xff);
  m_i2cDevice->WriteByte(LEDOUT3, 0xff);
}

void LEDDriver::SetBrightness(float brightness, int index) {
  uint8_t address = PWMSTART + index;
  uint8_t scaledBrightness = (uint8_t)(brightness * 255.0);
  m_i2cDevice->WriteByte(address, scaledBrightness);
}
