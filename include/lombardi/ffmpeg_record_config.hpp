#pragma once

#include <string>

namespace lombardi {

struct ffmpeg_record_config {
  bool enabled = false;
  std::string output_path = "dump/capture";
  double fps = 60.0;
  int crf = 21;
  double segment_seconds = 10.0;
};

} // namespace lombardi
