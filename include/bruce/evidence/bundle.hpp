#pragma once
#include "bruce/core/replay_control.hpp"
#include "bruce/core/types.hpp"
namespace bruce {
std::string sha256(std::string_view value,const ReplayCheckpoint& checkpoint={});
std::string read_bounded(const std::string& path, std::size_t limit=max_file_bytes,const ReplayCheckpoint& checkpoint={});
void export_exclusive(const std::string& path, std::string_view content,const ReplayCheckpoint& checkpoint={});
}
