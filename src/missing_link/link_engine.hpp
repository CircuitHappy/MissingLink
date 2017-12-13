/**
 * Copyright (c) 2017
 * Circuit Happy, LLC
 */

#pragma once

#include <memory>
#include <thread>
#include <ableton/link.hpp>
#include "missing_link/user_interface.hpp"

namespace MissingLink {

  class LinkEngine {

    public:

      LinkEngine();
      void Run();

    private:

      static constexpr int CLOCKS_PER_BEAT = 2;
      static constexpr double PULSE_LENGTH = 0.030; // seconds

      enum PlayState {
          Stopped,
          Cued,
          Playing
      };

      struct State {
        std::atomic<bool> running;
        std::atomic<PlayState> playState;
        std::atomic<UserInterface::EncoderMode> encoderMode;
        ableton::Link link;
        std::atomic<int> quantum;
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

          void run() override;
      };

      State m_state;
      std::shared_ptr<UserInterface> m_pUI;
      std::unique_ptr<UIProcess> m_pUIProcess;

      std::chrono::microseconds m_lastOutputTime;

      void runOutput();
      void runDisplaySocket();

      void playStop();
      void toggleMode();
      void tempoAdjust(float amount);
      void loopAdjust(int amount);
  };

}
