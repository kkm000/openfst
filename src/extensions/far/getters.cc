// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
//
// Definitions and functions for invoking and using Far main functions that
// support multiple and extensible arc types.

#include <fst/extensions/far/getters.h>

#include <string>
#include <vector>

#include <fstream>

namespace fst {

namespace script {

bool GetFarType(const std::string &str, FarType *far_type) {
  if (str == "fst") {
    *far_type = FarType::FST;
  } else if (str == "stlist") {
    *far_type = FarType::STLIST;
  } else if (str == "sttable") {
    *far_type = FarType::STTABLE;
  } else if (str == "default") {
    *far_type = FarType::DEFAULT;
  } else {
    return false;
  }
  return true;
}

bool GetFarEntryType(const std::string &str, FarEntryType *entry_type) {
  if (str == "line") {
    *entry_type = FarEntryType::LINE;
  } else if (str == "file") {
    *entry_type = FarEntryType::FILE;
  } else {
    return false;
  }
  return true;
}

void ExpandArgs(int argc, char **argv, int *argcp, char ***argvp) {
}

}  // namespace script

std::string GetFarTypeString(FarType far_type) {
  switch (far_type) {
    case FarType::FST:
      return "fst";
    case FarType::STLIST:
      return "stlist";
    case FarType::STTABLE:
      return "sttable";
    case FarType::DEFAULT:
      return "default";
    default:
      return "<unknown>";
  }
}

}  // namespace fst
