/**
 * Copyright (c) 2018
 * Circuit Happy, LLC
 */

#pragma once

namespace MissingLink {

/// POD struct represeting persistent link engine settings
struct Settings {

  double tempo;
  int loop_size;
  int launch_quant;
  int ppqn_index;
  int reset_mode;
  static const std::vector<int> ppqn_options;
  int delay_compensation;
  bool start_stop_sync;
  int ap_mode;

  // Defaults
  Settings() : tempo(120.0), loop_size(4), launch_quant(0), ppqn_index(2), reset_mode(0), delay_compensation(0), start_stop_sync(false), ap_mode(0) {}

  // Load from config file
  static Settings Load();

  // Save to config file
  static void Save(const Settings settings);

  //look up ppqn value in ppqn_options vector
  int getPPQN() const;

};

}
