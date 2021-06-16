// Copyright 2005-2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the 'License');
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
// FST utility definitions.

#include <fst/util.h>

#include <cctype>
#include <charconv>
#include <sstream>
#include <string>

#include <fst/flags.h>
#include <fst/types.h>
#include <fst/log.h>
#include <fst/mapped-file.h>
#include <string_view>
#include <optional>

// Utility flag definitions

DEFINE_bool(fst_error_fatal, true,
            "FST errors are fatal; o.w. return objects flagged as bad: "
            "e.g., FSTs: kError property set, FST weights: not a Member()");

namespace fst {

std::vector<std::string_view> SplitString(const std::string_view line,
                                           const std::string_view delim,
                                           const bool omit_empty_strings) {
  std::vector<std::string_view> vec;
  size_t prev_pos = 0, pos = 0;
  while (pos <= line.length()) {
    pos = line.find_first_of(delim, pos);
    if (pos == std::string_view::npos) {
      pos = line.length();
    }
    if (!omit_empty_strings || pos != prev_pos)
      vec.push_back(line.substr(prev_pos, pos - prev_pos));
    prev_pos = ++pos;
  }
  return vec;
}

std::optional<int64> ParseInt64(std::string_view s) {
  int64 n;
  if (const auto [p, ec] = std::from_chars(s.begin(), s.end(), n);
      ec != std::errc() || p != s.end()) {
    return std::nullopt;
  }
  return n;
}

int64 StrToInt64(std::string_view s, std::string_view source, size_t nline,
                 bool allow_negative, bool *error) {
  if (error) *error = false;
  const std::optional<int64> maybe_n = ParseInt64(s);
  if (!maybe_n.has_value() || (!allow_negative && *maybe_n < 0)) {
    FSTERROR() << "StrToInt64: Bad integer = " << s << "\", source = " << source
               << ", line = " << nline;
    if (error) *error = true;
    return 0;
  }
  return *maybe_n;
}

void ConvertToLegalCSymbol(std::string *s) {
  for (auto it = s->begin(); it != s->end(); ++it) {
    if (!isalnum(*it)) {
      *it = '_';
    }
  }
}

// Skips over input characters to align to 'align' bytes. Returns false if can't
// align.
bool AlignInput(std::istream &strm, size_t align) {
  char c;
  for (size_t i = 0; i < align; ++i) {
    int64 pos = strm.tellg();
    if (pos < 0) {
      LOG(ERROR) << "AlignInput: Can't determine stream position";
      return false;
    }
    if (pos % align == 0) break;
    strm.read(&c, 1);
  }
  return true;
}

// Write null output characters to align to 'align' bytes. Returns false if
// can't align.
bool AlignOutput(std::ostream &strm, size_t align) {
  for (size_t i = 0; i < align; ++i) {
    int64 pos = strm.tellp();
    if (pos < 0) {
      LOG(ERROR) << "AlignOutput: Can't determine stream position";
      return false;
    }
    if (pos % align == 0) break;
    strm.write("", 1);
  }
  return true;
}

int AlignBufferWithOutputStream(std::ostream &strm,
                                std::ostringstream &buffer,
                                size_t align) {
  const auto strm_pos = strm.tellp();
  if (strm_pos == -1) {
    LOG(ERROR) << "Cannot determine stream position";
    return -1;
  }
  const int stream_offset = strm_pos % align;
  for (int i = 0; i < stream_offset; ++i) buffer.write("", 1);
  return stream_offset;
}

}  // namespace fst
