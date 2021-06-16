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
// Extracts component FSTs from an finite-state archive.

#ifndef FST_EXTENSIONS_FAR_EXTRACT_H_
#define FST_EXTENSIONS_FAR_EXTRACT_H_

#include <iomanip>
#include <memory>
#include <string>
#include <vector>

#include <fst/extensions/far/far.h>
#include <fst/util.h>
#include <string_view>

namespace fst {

template <class Arc>
inline void FarWriteFst(const Fst<Arc> *fst, std::string_view key,
                        std::string *okey, int *nrep, int32 generate_sources,
                        int i, std::string_view source_prefix,
                        std::string_view source_suffix) {
  DCHECK_NE(fst, nullptr);
  DCHECK_NE(okey, nullptr);
  DCHECK_NE(nrep, nullptr);
  if (key == *okey) {
    ++*nrep;
  } else {
    *nrep = 0;
  }
  okey->assign(key.data(), key.size());
  std::ostringstream source_path;
  source_path << source_prefix;
  if (generate_sources) {
    source_path << std::setw(generate_sources) << std::setfill('0') << i;
  } else {
    source_path << key;
    if (*nrep > 0) source_path << '.' << *nrep;
  }
  source_path << source_suffix;
  fst->Write(source_path.str());
}

template <class Arc>
void FarExtract(const std::vector<std::string> &isources,
                int32 generate_sources, const std::string &keys,
                const std::string &key_separator,
                const std::string &range_delimiter,
                const std::string &source_prefix,
                const std::string &source_suffix) {
  std::unique_ptr<FarReader<Arc>> far_reader(FarReader<Arc>::Open(isources));
  if (!far_reader) return;
  std::string okey;
  int nrep = 0;
  // User has specified a set of FSTs to extract, where some of these may in
  // fact be ranges.
  if (!keys.empty()) {
    std::vector<std::string_view> key_vector =
        SplitString(keys, key_separator, true);
    int i = 0;
    for (size_t k = 0; k < key_vector.size(); ++k, ++i) {
      std::string_view key = key_vector[k];
      std::vector<std::string_view> range_vector =
          SplitString(key, range_delimiter, false);
      if (range_vector.size() == 1) {  // Not a range
        if (!far_reader->Find(key)) {
          LOG(ERROR) << "FarExtract: Cannot find key " << key;
          return;
        }
        const auto *fst = far_reader->GetFst();
        FarWriteFst(fst, key, &okey, &nrep, generate_sources, i, source_prefix,
                    source_suffix);
      } else if (range_vector.size() == 2) {  // A legal range
        std::string_view begin_key = range_vector[0];
        std::string_view end_key = range_vector[1];
        if (begin_key.empty() || end_key.empty()) {
          LOG(ERROR) << "FarExtract: Illegal range specification " << key;
          return;
        }
        if (!far_reader->Find(begin_key)) {
          LOG(ERROR) << "FarExtract: Cannot find key " << begin_key;
          return;
        }
        for (; !far_reader->Done(); far_reader->Next(), ++i) {
          const auto &ikey = far_reader->GetKey();
          if (end_key < ikey) break;
          const auto *fst = far_reader->GetFst();
          FarWriteFst(fst, ikey, &okey, &nrep, generate_sources, i,
                      source_prefix, source_suffix);
        }
      } else {
        LOG(ERROR) << "FarExtract: Illegal range specification " << key;
        return;
      }
    }
    return;
  }
  // Nothing specified, so just extracts everything.
  for (size_t i = 1; !far_reader->Done(); far_reader->Next(), ++i) {
    const auto &key = far_reader->GetKey();
    const auto *fst = far_reader->GetFst();
    FarWriteFst(fst, key, &okey, &nrep, generate_sources, i, source_prefix,
                source_suffix);
  }
}

}  // namespace fst

#endif  // FST_EXTENSIONS_FAR_EXTRACT_H_
