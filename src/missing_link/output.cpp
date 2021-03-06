/**
 * Copyright (c) 2018
 * Circuit Happy, LLC
 */

#include <algorithm>
#include <thread>
#include <pthread.h>
#include <iostream>
#include <sstream>
#include <chrono>
#include <ableton/Link.hpp>
#include "missing_link/hw_defs.h"
#include "missing_link/types.hpp"
#include "missing_link/view.hpp"
#include "missing_link/engine.hpp"
#include "missing_link/output.hpp"
#include "missing_link/wifi_status.hpp"

using namespace MissingLink;
using namespace MissingLink::GPIO;
using std::min;
using std::max;

OutputProcess::OutputProcess(Engine &engine)
  : Engine::Process(engine, std::chrono::microseconds(500))
  , m_pClockOut(std::unique_ptr<Pin>(new Pin(ML_CLOCK_PIN, Pin::OUT)))
  , m_pResetOut(std::unique_ptr<Pin>(new Pin(ML_RESET_PIN, Pin::OUT)))
{
  m_pClockOut->Write(LOW);
  m_pResetOut->Write(LOW);
}

void OutputProcess::Run() {
  Process::Run();
  sched_param param;
  param.sched_priority = 90;
  if(::pthread_setschedparam(m_pThread->native_handle(), SCHED_FIFO, &param) < 0) {
    std::cerr << "Failed to set output thread priority\n";
  }
}

void OutputProcess::process() {
  auto midiOut = m_engine.GetMidiOut();
  auto playState = m_engine.GetPlayState();
  const auto model = m_engine.GetOutputModel(m_lastOutTime);
  m_lastOutTime = model.now;

  switch (playState) {
    case Engine::PlayState::Stopped:
      if (m_transportStopped == false) {
        midiOut->StopTransport();
        m_transportStopped = true;
      }
      setClock(false);
      setReset(false);
      break;
    case Engine::PlayState::Cued:
      // start playing on first clock of loop
      if (!model.resetTriggered) {
        break;
      }
      // Deliberate fallthrough here
      m_engine.SetPlayState(Engine::PlayState::Playing);
    case Engine::PlayState::Playing:
      triggerOutputs(model.clockTriggered, model.resetTriggered);
      break;
    case Engine::PlayState::CuedStop:
      // stop playing on first clock of loop
      if (model.resetTriggered) {
        m_engine.SetPlayState(Engine::PlayState::Stopped);
        midiOut->StopTransport(); //stop before start of next loop
        m_transportStopped = true;
      } else {
        //keep playing the clock
        triggerOutputs(model.clockTriggered, model.resetTriggered);
      }
      break;
    default:
      break;
  }
  if (model.midiClockTriggered) { midiOut->ClockOut(); } //always output midi clock
}

void OutputProcess::triggerOutputs(bool clockTriggered, bool resetTriggered) {
  auto midiOut = m_engine.GetMidiOut();
  auto playState = m_engine.GetPlayState();
  auto mainView = m_engine.GetMainView();
  bool resetTrig = true;
  if (m_engine.getResetMode() == 2) {
    resetTrig = false;
  }
  if (resetTriggered) {
    setReset(resetTrig);
    //first reset trigger is start of sequence, tell midi to StartTransport
    //or, a manually queued MIDI Start Transport
    if (m_transportStopped || m_engine.GetQueuedStartTransport()) {
      midiOut->StartTransport();
      mainView->flashLedRing();
      m_transportStopped = false;
    }
  }
  if (clockTriggered) { setClock(true); }

  if (clockTriggered || resetTriggered) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    switch (m_engine.getResetMode()) {
      case 0:
        if (playState == Engine::PlayState::Playing) {
          setReset(false);
        }
        break;
      case 1:
        if (playState == Engine::PlayState::Playing) {
          setReset(true);
        }
        break;
      case 2:
        if (playState == Engine::PlayState::Playing) {
          setReset(true);
        }
        break;
      default:
        setReset(false);
        break;
    }
    setClock(false);
  }
}

void OutputProcess::setClock(bool high) {
  if (m_clockHigh == high) { return; }
  m_clockHigh = high;
  m_pClockOut->Write(high ? HIGH : LOW);
}

void OutputProcess::setReset(bool high) {
  if (m_resetHigh == high) { return; }
  m_resetHigh = high;
  m_pResetOut->Write(high ? HIGH : LOW);
}

namespace MissingLink {

  static const int NUM_ANIM_FRAMES = 6;

  static const float CueAnimationFrames[][6] =  {
    {0.2, 0, 0, 0.1, 0.2, 0.3},
    {0.2, 0.2, 0, 0, 0.1, 0.2},
    {0.2, 0.2, 0.2, 0, 0, 0.1},
    {0.2, 0.2, 0.2, 0.2, 0, 0},
    {0.2, 0.2, 0.2, 0.2, 0.2, 0},
    {0.2, 0.2, 0.2, 0.2, 0.2, 0.2},
  };

  static const float PlayAnimationFrames[][6] = {
    {1, 0.1, 0.1, 0.1, 0.1, 0.1},
    {0.1, 1, 0.1, 0.1, 0.1, 0.1},
    {0.1, 0.1, 1, 0.1, 0.1, 0.1},
    {0.1, 0.1, 0.1, 1, 0.1, 0.1},
    {0.1, 0.1, 0.1, 0.1, 1, 0.1},
    {0.1, 0.1, 0.1, 0.1, 0.1, 1},
  };

  static const float CuedStopAnimationFrames[][6] = {
    {0, 0.1, 0.2, 0.3, 0.4, 0.5},
    {0, 0.03, 0.1, 0.2, 0.3, 0.4},
    {0, 0, 0.03, 0.1, 0.2, 0.3},
    {0, 0, 0, 0.03, 0.1, 0.2},
    {0, 0, 0, 0, 0.03, 0.1},
    {0.01, 0, 0, 0, 0, 0},
  };

  static const float StoppedAnimationFrames[][6] = {
    {0.05, 0, 0, 0.02, 0.02, 0.03},
    {0.03, 0.05, 0, 0, 0.02, 0.02},
    {0.02, 0.03, 0.05, 0, 0, 0.02},
    {0.02, 0.02, 0.03, 0.05, 0, 0},
    {0, 0.02, 0.02, 0.03, 0.05, 0},
    {0, 0, 0.02, 0.02, 0.03, 0.05},
  };

  //Animation for WIFI LED when AP is ready to connect to
  static std::vector<float> WifiAccessPointReady =
    {
      0, 0.05, 0.1, 0.15, 0.2, 0.25, 0.3, 0.35, 0.4, 0.45, 0.5, 0.55,
      0.6, 0.65, 0.7, 0.75, 0.8, 0.85, 0.9, 0.95, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
      0.95, 0.9, 0.85, 0.8, 0.75, 0.7, 0.65, 0.6, 0.55, 0.5, 0.45, 0.4,
      0.35, 0.3, 0.25, 0.2, 0.15, 0.1, 0.05, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

  //Animation for WIFI LED when failed to connect to known AP, booting AP mode
  static std::vector<float> WifiTryingToConnect =
  {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  };

}

ViewUpdateProcess::ViewUpdateProcess(Engine &engine, std::shared_ptr<MainView> pView)
  : Engine::Process(engine, std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::milliseconds(15)))
  , m_pView(pView)
{}

void ViewUpdateProcess::process() {
  const auto phase = m_engine.GetNormalizedPhase();
  const double beatPhase = m_engine.GetBeatPhase();
  const auto playState = m_engine.GetPlayState();
  m_pView->setLogoLight(beatPhase);
  animatePhase(phase, playState);
  m_pView->displayWifiStatusFrame(getWifiStatusFrame(m_engine.getWifiStatus()));
  m_pView->ScrollTempMessage();
  m_pView->UpdateDisplay();
}

void ViewUpdateProcess::animatePhase(float normalizedPhase, Engine::PlayState playState) {

  const int animFrameIndex = min(
    NUM_ANIM_FRAMES - 1,
    max(0, (int)floor(normalizedPhase * NUM_ANIM_FRAMES))
  );

  switch (playState) {
    case Engine::PlayState::Cued:
      m_pView->SetAnimationLEDs(CueAnimationFrames[animFrameIndex]);
      break;
    case Engine::PlayState::Playing:
      m_pView->SetAnimationLEDs(PlayAnimationFrames[animFrameIndex]);
      break;
    case Engine::PlayState::CuedStop:
      m_pView->SetAnimationLEDs(CuedStopAnimationFrames[animFrameIndex]);
      break;
    case Engine::PlayState::Stopped:
        if (m_engine.GetNumberOfPeers() > 0) {
          m_pView->SetAnimationLEDs(StoppedAnimationFrames[animFrameIndex]);
        } else {
          m_pView->ClearAnimationLEDs();
        }
        break;
    default:
      m_pView->ClearAnimationLEDs();
      break;
  }
}

float ViewUpdateProcess::getWifiStatusFrame(int wifiStatus) {
  std::vector<float> animationFrames;
  static int frameCount = 1;
  static int frameIndex = 0;
  //int wifiStatus = AP_MODE;
  switch (wifiStatus) {
    case AP_MODE :
      animationFrames = WifiAccessPointReady;
    break;
    case TRYING_TO_CONNECT :
      animationFrames = WifiTryingToConnect;
      break;
    case NO_WIFI_FOUND :
      animationFrames = {0.0};
      break;
    case WIFI_CONNECTED :
      animationFrames = {1.0};
      break;
    case REBOOT :
      animationFrames = {0.0};
      break;
    default :
      animationFrames = {0.0};
      break;
  }
  frameCount = animationFrames.size();
  frameIndex++;
  if (frameIndex > (frameCount - 1)) {
    frameIndex = 0;
  }
  return animationFrames.at(frameIndex);
}
