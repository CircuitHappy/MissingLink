/**
 * Copyright (c) 2017
 * Circuit Happy, LLC
 */

#include <iostream>
#include <cmath>
#include "missing_link/control.hpp"

using namespace std;
using namespace MissingLink;

namespace MissingLink {
  typedef std::chrono::milliseconds Millis;
}

Control::Control(vector<int> pinIndices)
{
  for (auto index : pinIndices) {
    m_flagMask |= (1 << index);
  }
}

Control::~Control() {}

void Control::HandleInterrupt(uint8_t flag, uint8_t state, shared_ptr<IOExpander> pExpander) {
  handleInterrupt(flag, state, pExpander);
}

Button::Button(int pinIndex) : Control({ pinIndex }) {}

Button::~Button() {}

void Button::handleInterrupt(uint8_t flag, uint8_t state, shared_ptr<IOExpander> pExpander) {

  auto now = Clock::now();
  auto last = m_lastEvent;
  m_lastEvent = now;

  if ((now - last) < Millis(50)) {
    return;
  }

  // check change to ON
  if ((flag & state) == 0) {
    return;
  }

  if (onTriggered) {
    onTriggered();
  }
}


RotaryEncoder::RotaryEncoder(int pinIndexA, int pinIndexB)
  : Control({ pinIndexA, pinIndexB })
  , m_aFlag(1 << pinIndexA)
  , m_bFlag(1 << pinIndexB)
  , m_encVal(0)
  , m_lastEncSeq(0)
{}

RotaryEncoder::~RotaryEncoder() {}

void RotaryEncoder::handleInterrupt(uint8_t flag, uint8_t state, shared_ptr<IOExpander> pExpander) {
  bool aOn = (m_aFlag & state) != 0;
  bool bOn = (m_bFlag & state) != 0;
  decode(aOn, bOn);
}

void RotaryEncoder::decode(bool aOn, bool bOn) {

  auto now = Clock::now();
  m_lastChange = now;

  uint8_t aVal = aOn ? 0x01 : 0x00;
  uint8_t bVal = bOn ? 0x01 : 0x00;

  unsigned int seq = (aVal ^ bVal) | (bVal << 1);
  unsigned int delta = (seq - m_lastEncSeq) & 0b11;

  m_lastEncSeq = seq;

  // Reset value if the last change was more than 250ms ago
  if (now - m_lastChange >= Millis(250)) {
    m_encVal = 0;
  }

  switch (delta) {
    case 1:
      m_encVal++;
      break;
    case 3:
      m_encVal--;
      break;
    default:
      break;
  }

  if (std::abs(m_encVal) < 4) {
    return;
  }

  float acc = 1.0;
  float rotationAmount = 0.0;

  const float accThreshold = 100.0;

  auto interval = std::chrono::duration_cast<Millis>(now - m_lastChange).count();
  if (interval > 0) {
    float factor = fmin(1.0, fmax(0.0, (accThreshold - (float)interval)/accThreshold));
    acc += factor * 50.0;
  }

  if (m_encVal > 0) {
    rotationAmount = 1.0 * acc;
  } else {
    rotationAmount = -1.0 * acc;
  }

  m_encVal = 0;

  if (onRotated) {
    onRotated(rotationAmount);
  }
}

