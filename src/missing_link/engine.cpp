/**
 * Copyright (c) 2017
 * Circuit Happy, LLC
 */

#include <iostream>
#include <vector>
#include "missing_link/engine.hpp"
#include "missing_link/output.hpp"
#include "missing_link/user_interface.hpp"

using namespace std;
using namespace MissingLink;

Engine::State::State()
  : running(true)
  , playState(PlayState::Stopped)
  , inputMode(InputMode::BPM)
  , settings(Settings::Load())
  , link(settings.load().tempo)
{
  link.enable(true);
}

const Engine::OutputModel Engine::State::getOutput(std::chrono::microseconds last) {
  OutputModel output;

  const auto now = link.clock().micros();
  output.now = now;

  auto timeline = link.captureAudioTimeline();
  output.tempo = timeline.tempo();

  if (last == std::chrono::microseconds(0)) {
    output.clockTriggered = false;
    output.resetTriggered = false;
    return output;
  }

  const auto currentSettings = settings.load();
  const double beats = timeline.beatAtTime(now, currentSettings.quantum);
  const double lastBeats = timeline.beatAtTime(last, currentSettings.quantum);

  const int edgesPerBeat = currentSettings.getPPQN() * 2;
  const int edgesPerLoop = edgesPerBeat * currentSettings.quantum;

  const int edge = (int)floor(beats * (double)edgesPerBeat);
  const int lastEdge = (int)floor(lastBeats * (double)edgesPerBeat);

  output.clockTriggered = (edge % 2 == 0) && (edge != lastEdge);
  output.resetTriggered = output.clockTriggered && (edge % edgesPerLoop == 0);

  return output;
}

const double Engine::State::getNormalizedPhase() {
  const auto now = link.clock().micros();
  const auto currentSettings = settings.load();
  const auto timeline = link.captureAppTimeline();
  const double phase = timeline.phaseAtTime(now, currentSettings.quantum);
  return min(1.0, max(0.0, phase / (double)currentSettings.quantum));
}

Engine::Process::Process(State &state, std::chrono::microseconds sleepTime)
  : m_state(state)
  , m_sleepTime(sleepTime)
  , m_bStopped(true)
{}

Engine::Process::~Process() {
  Stop();
}

void Engine::Process::Run() {
  if (m_pThread != nullptr) { return; }
  m_bStopped = false;
  m_pThread = unique_ptr<thread>(new thread(&Process::run, this));
}

void Engine::Process::Stop() {
  if (m_pThread == nullptr) { return; }
  m_bStopped = true;
  m_pThread->join();
  m_pThread = nullptr;
}

void Engine::Process::run() {
  while (!m_bStopped) {
    process();
    sleep();
  }
}

void Engine::Process::sleep() {
  std::this_thread::sleep_for(m_sleepTime);
}

Engine::Engine()
  : m_pView(shared_ptr<MainView>(new MainView()))
  , m_pTapTempo(unique_ptr<TapTempo>(new TapTempo()))
{
  auto outputProcess = unique_ptr<OutputProcess>(new OutputProcess(m_state));
  m_processes.push_back(std::move(outputProcess));

  auto viewProcess = unique_ptr<ViewUpdateProcess>(new ViewUpdateProcess(m_state, m_pView));
  m_processes.push_back(std::move(viewProcess));

  auto uiProcess = unique_ptr<UserInputProcess>(new UserInputProcess(m_state));
  uiProcess->onPlayStop = bind(&Engine::playStop, this);
  uiProcess->onTapTempo = bind(&TapTempo::Tap, m_pTapTempo.get());
  uiProcess->onEncoderRotate = bind(&Engine::routeEncoderAdjust, this, placeholders::_1);
  uiProcess->onEncoderPress = bind(&Engine::toggleMode, this);
  m_processes.push_back(std::move(uiProcess));

  m_pTapTempo->onNewTempo = bind(&Engine::setTempo, this, placeholders::_1);

  // TODO: setup tempo callback
  m_pView->WriteDisplay("120.0");
}

void Engine::Run() {
  for (auto &process : m_processes) {
    process->Run();
  }

  while (m_state.running) {
    Settings settings = m_state.settings.load();
    Settings::Save(settings);
    this_thread::sleep_for(chrono::seconds(1));
  }

  for (auto &process : m_processes) {
    process->Stop();
  }
}

void Engine::playStop() {
  switch (m_state.playState) {
    case PlayState::Stopped:
      // reset the timeline to zero if there are no peers
      if (m_state.link.numPeers() == 0) {
        resetTimeline();
      }
      m_state.playState = PlayState::Cued;
      break;
    case PlayState::Playing:
    case PlayState::Cued:
      m_state.playState = PlayState::Stopped;
      break;
    default:
      break;
  }
}

void Engine::toggleMode() {
  InputMode inputMode = m_state.inputMode.load();
  inputMode = static_cast<InputMode>((static_cast<int>(inputMode) + 1) % 3);
  m_state.inputMode = inputMode;
  switch (inputMode) {
    case InputMode::BPM:
      m_pView->WriteDisplayTemporarily("BPM", 500);
      break;
    case InputMode::Loop:
      m_pView->WriteDisplayTemporarily("LOOP", 500);
      break;
    case InputMode::Clock:
      m_pView->WriteDisplayTemporarily("CLK", 500);
      break;
    default:
      break;
  }
}

void Engine::resetTimeline() {
  // Reset to beat zero in 1 ms
  auto timeline = m_state.link.captureAppTimeline();
  auto resetTime = m_state.link.clock().micros() + std::chrono::milliseconds(1);
  timeline.forceBeatAtTime(0, resetTime, m_state.settings.load().quantum);
  m_state.link.commitAppTimeline(timeline);
}

void Engine::setTempo(double tempo) {
  auto now = m_state.link.clock().micros();
  auto timeline = m_state.link.captureAppTimeline();
  timeline.setTempo(tempo, now);
  m_state.link.commitAppTimeline(timeline);

  auto settings = m_state.settings.load();
  settings.tempo = tempo;
  m_state.settings = settings;

  // switch back to tempo mode
  m_state.inputMode = InputMode::BPM;
}

void Engine::routeEncoderAdjust(float amount) {
  float rounded = round(amount);
  switch (m_state.inputMode) {
    case InputMode::BPM:
      tempoAdjust(rounded);
      break;
    case InputMode::Loop:
      loopAdjust((int)rounded);
      break;
    case InputMode::Clock:
      ppqnAdjust((int)rounded);
      break;
    default:
      break;
  }
}

void Engine::tempoAdjust(float amount) {
  auto timeline = m_state.link.captureAppTimeline();
  double tempo = timeline.tempo() + amount;
  setTempo(tempo);
}

void Engine::loopAdjust(int amount) {
  auto settings = m_state.settings.load();
  settings.quantum = std::max(1, settings.quantum + amount);
  m_state.settings = settings;
}

void Engine::ppqnAdjust(int amount) {
  int max_index = Settings::ppqn_options.size() - 1;
  auto settings = m_state.settings.load();
  settings.ppqn_index = std::min(max_index, std::max(0, settings.ppqn_index + amount));
  m_state.settings = settings;
}
