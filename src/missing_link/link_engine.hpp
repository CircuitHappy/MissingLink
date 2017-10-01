/**
 * Copyright (c) 2017
 * Circuit Happy, LLC
 */

#pragma once

#include <memory>
#include <thread>
#include <ableton/link.hpp>
#include "missing_link/gpio.hpp"
#include "missing_link/io.hpp"

namespace MissingLink {

  class LinkEngine {

    public:

      LinkEngine();
      void Run();

    private:

      static constexpr double PULSES_PER_BEAT = 4.0;
      static constexpr double PULSE_LENGTH = 0.030; // seconds
      static constexpr double QUANTUM = 4;

      enum PlayState {
          Stopped,
          Cued,
          Playing
      };

      struct State {
        std::atomic<bool> running;
        std::atomic<PlayState> playState;
        ableton::Link link;
        State();
      };

      class Process {

        public:

          Process(State &state);
          virtual ~Process();

          virtual void Run();
          void Stop();

        protected:

          State &m_state;
          std::atomic<bool> m_bStopped;
          std::unique_ptr<std::thread> m_pThread;

          virtual void run() = 0;
      };

      class UIProcess : public Process {

        public:

          UIProcess(State &state);

        private:

          std::unique_ptr<GPIO::Pin> m_pInterrupt;

          void run() override;
      };

      State m_state;
      std::shared_ptr<IO> m_pIO;
      std::unique_ptr<UIProcess> m_pUIProcess;

      std::chrono::microseconds m_lastOutputTime;

      void runOutput();
      void runDisplaySocket();
  };

}
