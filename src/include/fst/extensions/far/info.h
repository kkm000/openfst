// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.

#ifndef FST_EXTENSIONS_FAR_INFO_H_
#define FST_EXTENSIONS_FAR_INFO_H_

#include <iomanip>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <fst/extensions/far/far.h>
#include <fst/extensions/far/getters.h>

namespace fst {

template <class Arc>
void AccumulateStatesAndArcs(const Fst<Arc> &fst, size_t *nstate, size_t *narc,
                             size_t *nfinal) {
  for (StateIterator<Fst<Arc>> siter(fst); !siter.Done();
       siter.Next(), ++(*nstate)) {
    ArcIterator<Fst<Arc>> aiter(fst, siter.Value());
    for (; !aiter.Done(); aiter.Next(), ++(*narc)) {
    }
    if (fst.Final(siter.Value()) != Arc::Weight::Zero()) ++(*nfinal);
  }
}

struct KeyInfo {
  string key;
  string type;
  size_t nstate;
  size_t narc;
  size_t nfinal;

  KeyInfo(string k, string t, int64 ns = 0, int64 na = 0, int nf = 0)
      : key(std::move(k)),
        type(std::move(t)),
        nstate(ns),
        narc(na),
        nfinal(nf) {}
};

template <class Arc>
void FarInfo(const std::vector<string> &filenames, const string &begin_key,
             const string &end_key, const bool list_fsts) {
  std::unique_ptr<FarReader<Arc>> reader(FarReader<Arc>::Open(filenames));
  if (!reader) return;
  if (!begin_key.empty()) reader->Find(begin_key);
  auto *infos = list_fsts ? new std::vector<KeyInfo>() : nullptr;
  size_t nfst = 0;
  size_t nstate = 0;
  size_t narc = 0;
  size_t nfinal = 0;
  std::set<string> fst_types;
  for (; !reader->Done(); reader->Next()) {
    const auto &key = reader->GetKey();
    if (!end_key.empty() && end_key < key) break;
    ++nfst;
    const auto *fst = reader->GetFst();
    fst_types.insert(fst->Type());
    if (infos) {
      KeyInfo info(key, fst->Type());
      AccumulateStatesAndArcs(*fst, &info.nstate, &info.narc, &info.nfinal);
      nstate += info.nstate;
      narc += info.narc;
      nfinal += info.nfinal;
      infos->push_back(info);
    } else {
      AccumulateStatesAndArcs(*fst, &nstate, &narc, &nfinal);
    }
  }
  if (!infos) {
    std::cout << std::left << std::setw(50) << "far type"
              << GetFarTypeString(reader->Type()) << std::endl;
    std::cout << std::left << std::setw(50) << "arc type" << Arc::Type()
              << std::endl;
    std::cout << std::left << std::setw(50) << "fst type";
    for (std::set<string>::const_iterator iter = fst_types.begin();
         iter != fst_types.end(); ++iter) {
      if (iter != fst_types.begin()) std::cout << ",";
      std::cout << *iter;
    }
    std::cout << std::endl;
    std::cout << std::left << std::setw(50) << "# of FSTs" << nfst << std::endl;
    std::cout << std::left << std::setw(50) << "total # of states" << nstate
              << std::endl;
    std::cout << std::left << std::setw(50) << "total # of arcs" << narc
              << std::endl;
    std::cout << std::left << std::setw(50) << "total # of final states"
              << nfinal << std::endl;
  } else {
    // FIXME(kbg): Grok, then document this.
    int wkey = 10;
    int wtype = 10;
    int wnstate = 14;
    int wnarc = 12;
    int wnfinal = 20;
    for (size_t i = 0; i < infos->size(); ++i) {
      const auto &info = (*infos)[i];
      if (info.key.size() + 2 > wkey) wkey = info.key.size() + 2;
      if (info.type.size() + 2 > wtype) wtype = info.type.size() + 2;
      if (ceil(log10(info.nstate)) + 2 > wnstate) {
        wnstate = ceil(log10(info.nstate)) + 2;
      }
      if (ceil(log10(info.narc)) + 2 > wnarc) {
        wnarc = ceil(log10(info.narc)) + 2;
      }
      if (ceil(log10(info.nfinal)) + 2 > wnfinal) {
        wnfinal = ceil(log10(info.nfinal)) + 2;
      }
    }
    std::cout << std::left << std::setw(wkey) << "key" << std::setw(wtype)
              << "type" << std::right << std::setw(wnstate) << "# of states"
              << std::setw(wnarc) << "# of arcs" << std::setw(wnfinal)
              << "# of final states" << std::endl;
    for (size_t i = 0; i < infos->size(); ++i) {
      const auto &info = (*infos)[i];
      std::cout << std::left << std::setw(wkey) << info.key << std::setw(wtype)
                << info.type << std::right << std::setw(wnstate) << info.nstate
                << std::setw(wnarc) << info.narc << std::setw(wnfinal)
                << info.nfinal << std::endl;
    }
  }
}

}  // namespace fst

#endif  // FST_EXTENSIONS_FAR_INFO_H_
