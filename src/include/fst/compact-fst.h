// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
// FST Class for memory-efficient representation of common types of
// FSTs: linear automata, acceptors, unweighted FSTs, ...

#ifndef FST_LIB_COMPACT_FST_H_
#define FST_LIB_COMPACT_FST_H_

#include <iterator>
#include <utility>
#include <vector>

#include <fst/log.h>

#include <fst/cache.h>
#include <fst/expanded-fst.h>
#include <fst/fst-decl.h>  // For optional argument declarations
#include <fst/mapped-file.h>
#include <fst/matcher.h>
#include <fst/test-properties.h>
#include <fst/util.h>


namespace fst {

struct CompactFstOptions : public CacheOptions {
  // The default caching behaviour is to do no caching. Most compactors are
  // cheap and therefore we save memory by not doing caching.
  CompactFstOptions() : CacheOptions(true, 0) {}

  explicit CompactFstOptions(const CacheOptions &opts) : CacheOptions(opts) {}
};

// The Compactor class determines how arcs and final weights are compacted and
// expanded.
//
// Final weights are treated as transitions to the superfinal state, i.e.,
// ilabel = olabel = kNoLabel and nextstate = kNoStateId.
//
// There are two types of compactors:
//
// * Fixed out-degree compactors: 'compactor.Size()' returns a positive integer
//   's'. An FST can be compacted by this compactor only if each state has
//   exactly 's' outgoing transitions (counting a non-Zero() final weight as a
//   transition). A typical example is a compactor for string FSTs, i.e.,
//   's == 1'.
//
// * Variable out-degree compactors: 'compactor.Size() == -1'. There are no
//   out-degree restrictions for these compactors.
//
// Interface:
//
// class Compactor {
//  public:
//   // Element is the type of the compacted transitions.
//   using Element = ...
//
//   // Returns the compacted representation of a transition 'arc'
//   // at a state 's'.
//   Element Compact(StateId s, const Arc &arc);
//
//   // Returns the transition at state 's' represented by the compacted
//   // transition 'e'.
//   Arc Expand(StateId s, const Element &e) const;
//
//   // Returns -1 for variable out-degree compactors, and the mandatory
//   // out-degree otherwise.
//   ssize_t Size() const;
//
//   // Tests whether an FST can be compacted by this compactor.
//   bool Compatible(const Fst<A> &fst) const;
//
//   // Returns the properties that are always true for an FST compacted using
//   // this compactor
//   uint64 Properties() const;
//
//   // Returns a string identifying the type of compactor.
//   static const string &Type();
//
//   // Writes a compactor to a file.
//   bool Write(std::ostream &strm) const;
//
//   // Reads a compactor from a file.
//   static Compactor *Read(std::istream &strm);
//
//   // Default constructor (optional, see comment below).
//   Compactor();
// };
//
// The default constructor is only required for FST_REGISTER to work (i.e.,
// enabling Convert() and the command-line utilities to work with this new
// compactor). However, a default constructor always needs to be specified for
// this code to compile, but one can have it simply raise an error when called,
// like so:
//
// Compactor::Compactor() {
//   FSTERROR() << "Compactor: No default constructor";
// }

// Default implementation data for CompactFst, which can shared between
// otherwise independent copies.
//
// The implementation contains two arrays: 'states_' and 'compacts_'.
//
// For fixed out-degree compactors, the 'states_' array is unallocated. The
// 'compacts_' contains the compacted transitions. Its size is 'ncompacts_'.
// The outgoing transitions at a given state are stored consecutively. For a
// given state 's', its 'compactor.Size()' outgoing transitions (including
// superfinal transition when 's' is final), are stored in position
// ['s*compactor.Size()', '(s+1)*compactor.Size()').
//
// For variable out-degree compactors, the states_ array has size
// 'nstates_ + 1' and contains pointers to positions into 'compacts_'. For a
// given state 's', the compacted transitions of 's' are stored in positions
// ['states_[s]', 'states_[s + 1]') in 'compacts_'. By convention,
// 'states_[nstates_] == ncompacts_'.
//
// In both cases, the superfinal transitions (when 's' is final, i.e.,
// 'Final(s) != Weight::Zero()') are stored first.
//
// The unsigned type U is used to represent indices into the compacts_ array.
template <class Element, class Unsigned>
class DefaultCompactStore {
 public:
  DefaultCompactStore()
      : states_(nullptr),
        compacts_(nullptr),
        nstates_(0),
        ncompacts_(0),
        narcs_(0),
        start_(kNoStateId),
        error_(false) {}

  template <class Arc, class Compactor>
  DefaultCompactStore(const Fst<Arc> &fst, const Compactor &compactor);

  template <class Iterator, class Compactor>
  DefaultCompactStore(const Iterator &begin, const Iterator &end,
                      const Compactor &compactor);

  ~DefaultCompactStore() {
    if (!states_region_) delete[] states_;
    if (!compacts_region_) delete[] compacts_;
  }

  template <class Compactor>
  static DefaultCompactStore<Element, Unsigned> *Read(
      std::istream &strm, const FstReadOptions &opts, const FstHeader &hdr,
      const Compactor &compactor);

  bool Write(std::ostream &strm, const FstWriteOptions &opts) const;

  Unsigned States(ssize_t i) const { return states_[i]; }

  const Element &Compacts(size_t i) const { return compacts_[i]; }

  size_t NumStates() const { return nstates_; }

  size_t NumCompacts() const { return ncompacts_; }

  size_t NumArcs() const { return narcs_; }

  ssize_t Start() const { return start_; }

  bool Error() const { return error_; }

  // Returns a string identifying the type of data storage container.
  static const string &Type();

 private:
  std::unique_ptr<MappedFile> states_region_;
  std::unique_ptr<MappedFile> compacts_region_;
  Unsigned *states_;
  Element *compacts_;
  size_t nstates_;
  size_t ncompacts_;
  size_t narcs_;
  ssize_t start_;
  bool error_;
};

template <class Element, class Unsigned>
template <class Arc, class Compactor>
DefaultCompactStore<Element, Unsigned>::DefaultCompactStore(
    const Fst<Arc> &fst, const Compactor &compactor)
    : states_(nullptr),
      compacts_(nullptr),
      nstates_(0),
      ncompacts_(0),
      narcs_(0),
      start_(kNoStateId),
      error_(false) {
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;
  start_ = fst.Start();
  // Counts # of states and arcs.
  StateId nfinals = 0;
  for (StateIterator<Fst<Arc>> siter(fst); !siter.Done(); siter.Next()) {
    ++nstates_;
    const auto s = siter.Value();
    for (ArcIterator<Fst<Arc>> aiter(fst, s); !aiter.Done(); aiter.Next()) {
      ++narcs_;
    }
    if (fst.Final(s) != Weight::Zero()) ++nfinals;
  }
  if (compactor.Size() == -1) {
    states_ = new Unsigned[nstates_ + 1];
    ncompacts_ = narcs_ + nfinals;
    compacts_ = new Element[ncompacts_];
    states_[nstates_] = ncompacts_;
  } else {
    states_ = nullptr;
    ncompacts_ = nstates_ * compactor.Size();
    if ((narcs_ + nfinals) != ncompacts_) {
      FSTERROR() << "DefaultCompactStore: Compactor incompatible with FST";
      error_ = true;
      return;
    }
    compacts_ = new Element[ncompacts_];
  }
  size_t pos = 0;
  size_t fpos = 0;
  for (size_t s = 0; s < nstates_; ++s) {
    fpos = pos;
    if (compactor.Size() == -1) states_[s] = pos;
    if (fst.Final(s) != Weight::Zero()) {
      compacts_[pos++] = compactor.Compact(
          s, Arc(kNoLabel, kNoLabel, fst.Final(s), kNoStateId));
    }
    for (ArcIterator<Fst<Arc>> aiter(fst, s); !aiter.Done(); aiter.Next()) {
      compacts_[pos++] = compactor.Compact(s, aiter.Value());
    }
    if ((compactor.Size() != -1) && ((pos - fpos) != compactor.Size())) {
      FSTERROR() << "DefaultCompactStore: Compactor incompatible with FST";
      error_ = true;
      return;
    }
  }
  if (pos != ncompacts_) {
    FSTERROR() << "DefaultCompactStore: Compactor incompatible with FST";
    error_ = true;
    return;
  }
}

template <class Element, class Unsigned>
template <class Iterator, class Compactor>
DefaultCompactStore<Element, Unsigned>::DefaultCompactStore(
    const Iterator &begin, const Iterator &end, const Compactor &compactor)
    : states_(nullptr),
      compacts_(nullptr),
      nstates_(0),
      ncompacts_(0),
      narcs_(0),
      start_(kNoStateId),
      error_(false) {
  using Arc = typename Compactor::Arc;
  using Weight = typename Arc::Weight;
  if (compactor.Size() != -1) {
    ncompacts_ = std::distance(begin, end);
    if (compactor.Size() == 1) {
      // For strings, allows implicit final weight. Empty input is the empty
      // string.
      if (ncompacts_ == 0) {
        ++ncompacts_;
      } else {
        const auto arc =
            compactor.Expand(ncompacts_ - 1, *(begin + (ncompacts_ - 1)));
        if (arc.ilabel != kNoLabel) ++ncompacts_;
      }
    }
    if (ncompacts_ % compactor.Size()) {
      FSTERROR() << "DefaultCompactStore: Size of input container incompatible"
                 << " with compactor";
      error_ = true;
      return;
    }
    if (ncompacts_ == 0) return;
    start_ = 0;
    nstates_ = ncompacts_ / compactor.Size();
    compacts_ = new Element[ncompacts_];
    size_t i = 0;
    Iterator it = begin;
    for (; it != end; ++it, ++i) {
      compacts_[i] = *it;
      if (compactor.Expand(i, *it).ilabel != kNoLabel) ++narcs_;
    }
    if (i < ncompacts_) {
      compacts_[i] = compactor.Compact(
          i, Arc(kNoLabel, kNoLabel, Weight::One(), kNoStateId));
    }
  } else {
    if (std::distance(begin, end) == 0) return;
    // Count # of states, arcs and compacts.
    auto it = begin;
    for (size_t i = 0; it != end; ++it, ++i) {
      const auto arc = compactor.Expand(i, *it);
      if (arc.ilabel != kNoLabel) {
        ++narcs_;
        ++ncompacts_;
      } else {
        ++nstates_;
        if (arc.weight != Weight::Zero()) ++ncompacts_;
      }
    }
    start_ = 0;
    compacts_ = new Element[ncompacts_];
    states_ = new Unsigned[nstates_ + 1];
    states_[nstates_] = ncompacts_;
    size_t i = 0;
    size_t s = 0;
    for (it = begin; it != end; ++it) {
      const auto arc = compactor.Expand(i, *it);
      if (arc.ilabel != kNoLabel) {
        compacts_[i++] = *it;
      } else {
        states_[s++] = i;
        if (arc.weight != Weight::Zero()) compacts_[i++] = *it;
      }
    }
    if ((s != nstates_) || (i != ncompacts_)) {
      FSTERROR() << "DefaultCompactStore: Ill-formed input container";
      error_ = true;
      return;
    }
  }
}

template <class Element, class Unsigned>
template <class Compactor>
DefaultCompactStore<Element, Unsigned>
    *DefaultCompactStore<Element, Unsigned>::Read(std::istream &strm,
                                                  const FstReadOptions &opts,
                                                  const FstHeader &hdr,
                                                  const Compactor &compactor) {
  std::unique_ptr<DefaultCompactStore<Element, Unsigned>> data(
      new DefaultCompactStore<Element, Unsigned>());
  data->start_ = hdr.Start();
  data->nstates_ = hdr.NumStates();
  data->narcs_ = hdr.NumArcs();
  if (compactor.Size() == -1) {
    if ((hdr.GetFlags() & FstHeader::IS_ALIGNED) && !AlignInput(strm)) {
      LOG(ERROR) << "DefaultCompactStore::Read: Alignment failed: "
                 << opts.source;
      return nullptr;
    }
    auto b = (data->nstates_ + 1) * sizeof(Unsigned);
    data->states_region_.reset(MappedFile::Map(
        &strm, opts.mode == FstReadOptions::MAP, opts.source, b));
    if (!strm || !data->states_region_) {
      LOG(ERROR) << "DefaultCompactStore::Read: Read failed: " << opts.source;
      return nullptr;
    }
    data->states_ =
        static_cast<Unsigned *>(data->states_region_->mutable_data());
  } else {
    data->states_ = nullptr;
  }
  data->ncompacts_ = compactor.Size() == -1 ? data->states_[data->nstates_]
                                            : data->nstates_ * compactor.Size();
  if ((hdr.GetFlags() & FstHeader::IS_ALIGNED) && !AlignInput(strm)) {
    LOG(ERROR) << "DefaultCompactStore::Read: Alignment failed: "
               << opts.source;
    return nullptr;
  }
  size_t b = data->ncompacts_ * sizeof(Element);
  data->compacts_region_.reset(
      MappedFile::Map(&strm, opts.mode == FstReadOptions::MAP, opts.source, b));
  if (!strm || !data->compacts_region_) {
    LOG(ERROR) << "DefaultCompactStore::Read: Read failed: " << opts.source;
    return nullptr;
  }
  data->compacts_ =
      static_cast<Element *>(data->compacts_region_->mutable_data());
  return data.release();
}

template <class Element, class Unsigned>
bool DefaultCompactStore<Element, Unsigned>::Write(
    std::ostream &strm, const FstWriteOptions &opts) const {
  if (states_) {
    if (opts.align && !AlignOutput(strm)) {
      LOG(ERROR) << "DefaultCompactStore::Write: Alignment failed: "
                 << opts.source;
      return false;
    }
    strm.write(reinterpret_cast<char *>(states_),
               (nstates_ + 1) * sizeof(Unsigned));
  }
  if (opts.align && !AlignOutput(strm)) {
    LOG(ERROR) << "DefaultCompactStore::Write: Alignment failed: "
               << opts.source;
    return false;
  }
  strm.write(reinterpret_cast<char *>(compacts_), ncompacts_ * sizeof(Element));
  strm.flush();
  if (!strm) {
    LOG(ERROR) << "DefaultCompactStore::Write: Write failed: " << opts.source;
    return false;
  }
  return true;
}

template <class Element, class Unsigned>
const string &DefaultCompactStore<Element, Unsigned>::Type() {
  static const string type = "compact";
  return type;
}

template <class Arc, class Element, class Unsigned, class CompactStore,
          class CacheStore>
class CompactFst;

template <class F, class G>
void Cast(const F &, G *);

namespace internal {

// Implementation class for CompactFst, which contains parametrizeable
// Fst data storage (DefaultCompactStore by default) and Fst cache.
template <class Arc, class Compactor, class Unsigned,
          class CompactStore =
              DefaultCompactStore<typename Compactor::Element, Unsigned>,
          class CacheStore = DefaultCacheStore<Arc>>
class CompactFstImpl
    : public CacheBaseImpl<typename CacheStore::State, CacheStore> {
 public:
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;
  using DataStorage = CompactStore;

  using FstImpl<Arc>::SetType;
  using FstImpl<Arc>::SetProperties;
  using FstImpl<Arc>::Properties;
  using FstImpl<Arc>::SetInputSymbols;
  using FstImpl<Arc>::SetOutputSymbols;
  using FstImpl<Arc>::WriteHeader;

  using ImplBase = CacheBaseImpl<typename CacheStore::State, CacheStore>;
  using ImplBase::PushArc;
  using ImplBase::HasArcs;
  using ImplBase::HasFinal;
  using ImplBase::HasStart;
  using ImplBase::SetArcs;
  using ImplBase::SetFinal;
  using ImplBase::SetStart;

  CompactFstImpl()
      : ImplBase(CompactFstOptions()),
        compactor_(),
        data_() {
    string type = "compact";
    if (sizeof(Unsigned) != sizeof(uint32)) {
      string size;
      Int64ToStr(8 * sizeof(Unsigned), &size);
      type += size;
    }
    type += "_";
    type += Compactor::Type();
    if (CompactStore::Type() != "compact") {
      type += "_";
      type += CompactStore::Type();
    }
    SetType(type);
    SetProperties(kNullProperties | kStaticProperties);
  }

  CompactFstImpl(const Fst<Arc> &fst, std::shared_ptr<Compactor> compactor,
                 const CompactFstOptions &opts,
                 std::shared_ptr<CompactStore> data)
      : ImplBase(opts), compactor_(std::move(compactor)) {
    Init(fst, data);
  }

  template <class Iterator>
  CompactFstImpl(const Iterator &b, const Iterator &e,
                 std::shared_ptr<Compactor> compactor,
                 const CompactFstOptions &opts)
      : ImplBase(opts), compactor_(std::move(compactor)) {
    Init(b, e);
  }

  CompactFstImpl(const CompactFstImpl<Arc, Compactor, Unsigned, CompactStore,
                                      CacheStore> &impl)
      : ImplBase(impl),
        compactor_(impl.compactor_ == nullptr
                       ? nullptr
                       : std::make_shared<Compactor>(*impl.compactor_)),
        data_(impl.data_) {
    SetType(impl.Type());
    SetProperties(impl.Properties());
    SetInputSymbols(impl.InputSymbols());
    SetOutputSymbols(impl.OutputSymbols());
  }

  // Allows to change the cache store from OtherI to I.
  template <class OtherCacheStore>
  explicit CompactFstImpl(
      const CompactFstImpl<Arc, Compactor, Unsigned, CompactStore,
                           OtherCacheStore> &impl)
      : ImplBase(CacheOptions(impl.GetCacheGc(), impl.GetCacheLimit())),
        compactor_(impl.compactor_ == nullptr
                       ? nullptr
                       : std::make_shared<Compactor>(*impl.compactor_)),
        data_(impl.data_) {
    SetType(impl.Type());
    SetProperties(impl.Properties());
    SetInputSymbols(impl.InputSymbols());
    SetOutputSymbols(impl.OutputSymbols());
  }

  StateId Start() {
    if (!HasStart()) SetStart(data_->Start());
    return ImplBase::Start();
  }

  Weight Final(StateId s) {
    if (HasFinal(s)) return ImplBase::Final(s);
    Arc arc(kNoLabel, kNoLabel, Weight::Zero(), kNoStateId);
    if ((compactor_->Size() != -1) ||
        (data_->States(s) != data_->States(s + 1))) {
      arc = ComputeArc(s, compactor_->Size() == -1 ? data_->States(s) :
                       s * compactor_->Size());
    }
    return arc.ilabel == kNoLabel ? arc.weight : Weight::Zero();
  }

  StateId NumStates() const {
    if (Properties(kError)) return 0;
    return data_->NumStates();
  }

  size_t NumArcs(StateId s) {
    if (HasArcs(s)) return ImplBase::NumArcs(s);
    Unsigned i;
    Unsigned num_arcs;
    if (compactor_->Size() == -1) {
      i = data_->States(s);
      num_arcs = data_->States(s + 1) - i;
    } else {
      i = s * compactor_->Size();
      num_arcs = compactor_->Size();
    }
    if (num_arcs > 0) {
      const auto &arc = ComputeArc(s, i, kArcILabelValue);
      if (arc.ilabel == kNoStateId) --num_arcs;
    }
    return num_arcs;
  }

  size_t NumInputEpsilons(StateId s) {
    if (!HasArcs(s) && !Properties(kILabelSorted)) Expand(s);
    if (HasArcs(s)) return ImplBase::NumInputEpsilons(s);
    return CountEpsilons(s, false);
  }

  size_t NumOutputEpsilons(StateId s) {
    if (!HasArcs(s) && !Properties(kOLabelSorted)) Expand(s);
    if (HasArcs(s)) return ImplBase::NumOutputEpsilons(s);
    return CountEpsilons(s, true);
  }

  size_t CountEpsilons(StateId s, bool output_epsilons) {
    size_t begin =
        compactor_->Size() == -1 ? data_->States(s) : s * compactor_->Size();
    size_t end = compactor_->Size() == -1 ? data_->States(s + 1)
                                          : (s + 1) * compactor_->Size();
    size_t num_eps = 0;
    for (size_t i = begin; i < end; ++i) {
      const auto &arc =
          ComputeArc(s, i, output_epsilons ? kArcOLabelValue : kArcILabelValue);
      const auto label = output_epsilons ? arc.olabel : arc.ilabel;
      if (label == kNoLabel) {
        continue;
      } else if (label > 0) {
        break;
      }
      ++num_eps;
    }
    return num_eps;
  }

  static CompactFstImpl<Arc, Compactor, Unsigned, CompactStore, CacheStore>
      *Read(std::istream &strm, const FstReadOptions &opts) {
    std::unique_ptr<
        CompactFstImpl<Arc, Compactor, Unsigned, CompactStore, CacheStore>>
        impl(new CompactFstImpl<Arc, Compactor, Unsigned, CompactStore,
                                CacheStore>());
    FstHeader hdr;
    if (!impl->ReadHeader(strm, opts, kMinFileVersion, &hdr)) {
      return nullptr;
    }
    // Ensures compatibility.
    if (hdr.Version() == kAlignedFileVersion) {
      hdr.SetFlags(hdr.GetFlags() | FstHeader::IS_ALIGNED);
    }
    impl->compactor_ = std::shared_ptr<Compactor>(Compactor::Read(strm));
    if (!impl->compactor_) return nullptr;
    impl->data_ = std::shared_ptr<CompactStore>(
        CompactStore::Read(strm, opts, hdr, *impl->compactor_));
    if (!impl->data_) return nullptr;
    return impl.release();
  }

  bool Write(std::ostream &strm, const FstWriteOptions &opts) const {
    FstHeader hdr;
    hdr.SetStart(data_->Start());
    hdr.SetNumStates(data_->NumStates());
    hdr.SetNumArcs(data_->NumArcs());
    // Ensures compatibility.
    const auto file_version = opts.align ? kAlignedFileVersion : kFileVersion;
    WriteHeader(strm, opts, file_version, &hdr);
    compactor_->Write(strm);
    return data_->Write(strm, opts);
  }

  // Provides information needed for generic state iterator.
  void InitStateIterator(StateIteratorData<Arc> *data) const {
    data->base = nullptr;
    data->nstates = data_->NumStates();
  }

  void InitArcIterator(StateId s, ArcIteratorData<Arc> *data) {
    if (!HasArcs(s)) Expand(s);
    ImplBase::InitArcIterator(s, data);
  }

  Arc ComputeArc(StateId s, Unsigned i, uint32 f = kArcValueFlags) const {
    return compactor_->Expand(s, data_->Compacts(i), f);
  }

  void Expand(StateId s) {
    size_t begin =
        compactor_->Size() == -1 ? data_->States(s) : s * compactor_->Size();
    size_t end = compactor_->Size() == -1 ? data_->States(s + 1)
                                          : (s + 1) * compactor_->Size();
    for (auto i = begin; i < end; ++i) {
      const auto &arc = ComputeArc(s, i);
      if (arc.ilabel == kNoLabel) {
        SetFinal(s, arc.weight);
      } else {
        PushArc(s, arc);
      }
    }
    if (!HasFinal(s)) SetFinal(s, Weight::Zero());
    SetArcs(s);
  }

  template <class Iterator>
  void SetCompactElements(const Iterator &begin, const Iterator &end) {
    SetProperties(kStaticProperties | compactor_->Properties());
    data_ = std::make_shared<CompactStore>(begin, end, *compactor_);
    if (data_->Error()) SetProperties(kError, kError);
  }

  const Compactor *GetCompactor() const { return compactor_.get(); }

  std::shared_ptr<Compactor> SharedCompactor() const { return compactor_; }

  const CompactStore *Data() const { return data_.get(); }

  std::shared_ptr<CompactStore> SharedData() const { return data_; }

  // Properties always true of this FST class.
  static constexpr uint64 kStaticProperties = kExpanded;

 protected:
  template <class OtherArc, class OtherCompactor, class OtherCacheStore>
  explicit CompactFstImpl(
      const CompactFstImpl<OtherArc, OtherCompactor, Unsigned, CompactStore,
                           OtherCacheStore> &impl)
      : ImplBase(CacheOptions(impl.GetCacheGc(), impl.GetCacheLimit())),
        compactor_(std::make_shared<Compactor>(*impl.GetCompactor())),
        data_(impl.SharedData()) {
    SetType(impl.Type());
    SetProperties(impl.Properties());
    SetInputSymbols(impl.InputSymbols());
    SetOutputSymbols(impl.OutputSymbols());
  }

 private:
  // Allows access during write.
  friend class CompactFst<Arc, Compactor, Unsigned, CompactStore, CacheStore>;

  void Init(const Fst<Arc> &fst, std::shared_ptr<CompactStore> data) {
    string type = "compact";
    if (sizeof(Unsigned) != sizeof(uint32)) {
      string size;
      Int64ToStr(8 * sizeof(Unsigned), &size);
      type += size;
    }
    type += "_";
    type += compactor_->Type();
    if (CompactStore::Type() != "compact") {
      type += "_";
      type += CompactStore::Type();
    }
    SetType(type);
    SetInputSymbols(fst.InputSymbols());
    SetOutputSymbols(fst.OutputSymbols());
    data_ = (data) ? data : std::make_shared<CompactStore>(fst, *compactor_);
    if (data_->Error()) SetProperties(kError, kError);
    const auto copy_properties =
        fst.Properties(kMutable, false)
            ? fst.Properties(kCopyProperties, true)
            : CheckProperties(
                  fst, kCopyProperties & ~kWeightedCycles & ~kUnweightedCycles,
                  kCopyProperties);
    if ((copy_properties & kError) || !compactor_->Compatible(fst)) {
      FSTERROR() << "CompactFstImpl: Input FST incompatible with compactor";
      SetProperties(kError, kError);
      return;
    }
    SetProperties(copy_properties | kStaticProperties);
  }

  template <class Iterator>
  void Init(const Iterator &begin, const Iterator &end) {
    string type = "compact";
    if (sizeof(Unsigned) != sizeof(uint32)) {
      string size;
      Int64ToStr(8 * sizeof(Unsigned), &size);
      type += size;
    }
    type += "_";
    type += compactor_->Type();
    SetType(type);
    SetProperties(kStaticProperties | compactor_->Properties());
    data_ = std::make_shared<CompactStore>(begin, end, *compactor_);
    if (data_->Error()) SetProperties(kError, kError);
  }

  // Current unaligned file format version.
  static constexpr int kFileVersion = 2;
  // Current aligned file format version.
  static constexpr int kAlignedFileVersion = 1;
  // Minimum file format version supported.
  static constexpr int kMinFileVersion = 1;

  std::shared_ptr<Compactor> compactor_;
  std::shared_ptr<CompactStore> data_;
};

template <class Arc, class Compactor, class Unsigned, class CompactStore,
          class CacheStore>
constexpr uint64 CompactFstImpl<Arc, Compactor, Unsigned, CompactStore,
                                CacheStore>::kStaticProperties;

template <class Arc, class Compactor, class Unsigned, class CompactStore,
          class CacheStore>
constexpr int CompactFstImpl<Arc, Compactor, Unsigned, CompactStore,
                             CacheStore>::kFileVersion;

template <class Arc, class Compactor, class Unsigned, class CompactStore,
          class CacheStore>
constexpr int CompactFstImpl<Arc, Compactor, Unsigned, CompactStore,
                             CacheStore>::kAlignedFileVersion;

template <class Arc, class Compactor, class Unsigned, class CompactStore,
          class CacheStore>
constexpr int CompactFstImpl<Arc, Compactor, Unsigned, CompactStore,
                             CacheStore>::kMinFileVersion;

}  // namespace internal

// This class attaches interface to implementation and handles reference
// counting, delegating most methods to ImplToExpandedFst. The Unsigned type
// is used to represent indices into the compact arc array. (Template
// argument defaults are declared in fst-decl.h.)
template <class A, class Compactor, class Unsigned, class CompactStore,
          class CacheStore>
class CompactFst : public ImplToExpandedFst<internal::CompactFstImpl<
                       A, Compactor, Unsigned, CompactStore, CacheStore>> {
 public:
  template <class F, class G>
  void friend Cast(const F &, G *);

  using Arc = A;
  using StateId = typename Arc::StateId;

  using Impl = internal::CompactFstImpl<A, Compactor, Unsigned, CompactStore,
                                        CacheStore>;

  friend class StateIterator<
      CompactFst<A, Compactor, Unsigned, CompactStore, CacheStore>>;
  friend class ArcIterator<
      CompactFst<A, Compactor, Unsigned, CompactStore, CacheStore>>;

  CompactFst() : ImplToExpandedFst<Impl>(std::make_shared<Impl>()) {}

  // If data is not nullptr, it is assumed to be already initialized.
  explicit CompactFst(
      const Fst<Arc> &fst, const Compactor &compactor = Compactor(),
      const CompactFstOptions &opts = CompactFstOptions(),
      std::shared_ptr<CompactStore> data = std::shared_ptr<CompactStore>())
      : ImplToExpandedFst<Impl>(std::make_shared<Impl>(
            fst, std::make_shared<Compactor>(compactor), opts, data)) {}

  // If data is not nullptr, it is assumed to be already initialized.
  CompactFst(
      const Fst<Arc> &fst, std::shared_ptr<Compactor> compactor,
      const CompactFstOptions &opts = CompactFstOptions(),
      std::shared_ptr<CompactStore> data = std::shared_ptr<CompactStore>())
      : ImplToExpandedFst<Impl>(
            std::make_shared<Impl>(fst, compactor, opts, data)) {}

  // The following 2 constructors take as input two iterators delimiting a set
  // of (already) compacted transitions, starting with the transitions out of
  // the initial state. The format of the input differs for fixed out-degree
  // and variable out-degree compactors.
  //
  // - For fixed out-degree compactors, the final weight (encoded as a
  // compacted transition) needs to be given only for final states. All strings
  // (compactor of size 1) will be assume to be terminated by a final state
  // even when the final state is not implicitely given.
  //
  // - For variable out-degree compactors, the final weight (encoded as a
  // compacted transition) needs to be given for all states and must appeared
  // first in the list (for state s, final weight of s, followed by outgoing
  // transitons in s).
  //
  // These 2 constructors allows the direct construction of a CompactFst
  // without first creating a more memory-hungry regular FST. This is useful
  // when memory usage is severely constrained.
  template <class Iterator>
  explicit CompactFst(const Iterator &begin, const Iterator &end,
                      const Compactor &compactor = Compactor(),
                      const CompactFstOptions &opts = CompactFstOptions())
      : ImplToExpandedFst<Impl>(std::make_shared<Impl>(
            begin, end, std::make_shared<Compactor>(compactor), opts)) {}

  template <class Iterator>
  CompactFst(const Iterator &begin, const Iterator &end,
             std::shared_ptr<Compactor> compactor,
             const CompactFstOptions &opts = CompactFstOptions())
      : ImplToExpandedFst<Impl>(
            std::make_shared<Impl>(begin, end, compactor, opts)) {}

  // See Fst<>::Copy() for doc.
  CompactFst(
      const CompactFst<A, Compactor, Unsigned, CompactStore, CacheStore> &fst,
      bool safe = false)
      : ImplToExpandedFst<Impl>(fst, safe) {}

  // Get a copy of this CompactFst. See Fst<>::Copy() for further doc.
  CompactFst<A, Compactor, Unsigned, CompactStore, CacheStore> *Copy(
      bool safe = false) const override {
    return new CompactFst<A, Compactor, Unsigned, CompactStore, CacheStore>(
        *this, safe);
  }

  // Read a CompactFst from an input stream; return nullptr on error
  static CompactFst<A, Compactor, Unsigned, CompactStore, CacheStore> *Read(
      std::istream &strm, const FstReadOptions &opts) {
    auto *impl = Impl::Read(strm, opts);
    return impl ? new CompactFst<A, Compactor, Unsigned, CompactStore,
                                 CacheStore>(std::shared_ptr<Impl>(impl))
                : nullptr;
  }

  // Read a CompactFst from a file; return nullptr on error
  // Empty filename reads from standard input
  static CompactFst<A, Compactor, Unsigned, CompactStore, CacheStore> *Read(
      const string &filename) {
    auto *impl = ImplToExpandedFst<Impl>::Read(filename);
    return impl ? new CompactFst<A, Compactor, Unsigned, CompactStore,
                                 CacheStore>(std::shared_ptr<Impl>(impl))
                : nullptr;
  }

  bool Write(std::ostream &strm, const FstWriteOptions &opts) const override {
    return GetImpl()->Write(strm, opts);
  }

  bool Write(const string &filename) const override {
    return Fst<Arc>::WriteFile(filename);
  }

  template <class FST>
  static bool WriteFst(const FST &fst, const Compactor &compactor,
                       std::ostream &strm, const FstWriteOptions &opts);

  void InitStateIterator(StateIteratorData<Arc> *data) const override {
    GetImpl()->InitStateIterator(data);
  }

  void InitArcIterator(StateId s, ArcIteratorData<Arc> *data) const override {
    GetMutableImpl()->InitArcIterator(s, data);
  }

  MatcherBase<Arc> *InitMatcher(MatchType match_type) const override {
    return new SortedMatcher<
        CompactFst<A, Compactor, Unsigned, CompactStore, CacheStore>>(
        *this, match_type);
  }

  template <class Iterator>
  void SetCompactElements(const Iterator &begin, const Iterator &end) {
    GetMutableImpl()->SetCompactElements(begin, end);
  }

 private:
  using ImplToFst<Impl, ExpandedFst<Arc>>::GetImpl;
  using ImplToFst<Impl, ExpandedFst<Arc>>::GetMutableImpl;

  explicit CompactFst(std::shared_ptr<Impl> impl)
      : ImplToExpandedFst<Impl>(impl) {}

  // Use overloading to extract the type of the argument.
  static Impl *GetImplIfCompactFst(
      const CompactFst<A, Compactor, Unsigned, CompactStore, CacheStore>
          &compact_fst) {
    return compact_fst.GetImpl();
  }

  // This does not give privileged treatment to subclasses of CompactFst.
  template <typename NonCompactFst>
  static Impl *GetImplIfCompactFst(const NonCompactFst &fst) {
    return nullptr;
  }

  CompactFst &operator=(const CompactFst &fst) = delete;
};

// Writes FST in Compact format, with a possible pass over the machine before
// writing to compute the number of states and arcs.
template <class A, class Compactor, class Unsigned, class CompactStore,
          class CacheStore>
template <class FST>
bool CompactFst<A, Compactor, Unsigned, CompactStore, CacheStore>::WriteFst(
    const FST &fst, const Compactor &compactor, std::ostream &strm,
    const FstWriteOptions &opts) {
  using Arc = A;
  using Weight = typename A::Weight;
  using Element = typename Compactor::Element;
  const auto file_version =
      opts.align ? Impl::kAlignedFileVersion : Impl::kFileVersion;
  size_t num_arcs = -1;
  size_t num_states = -1;
  auto first_pass_compactor = compactor;
  if (auto *impl = GetImplIfCompactFst(fst)) {
    num_arcs = impl->Data()->NumArcs();
    num_states = impl->Data()->NumStates();
    first_pass_compactor = *impl->GetCompactor();
  } else {
    // A first pass is needed to compute the state of the compactor, which
    // is saved ahead of the rest of the data structures. This unfortunately
    // means forcing a complete double compaction when writing in this format.
    // TODO(allauzen): eliminate mutable state from compactors.
    num_arcs = 0;
    num_states = 0;
    for (StateIterator<FST> siter(fst); !siter.Done(); siter.Next()) {
      const auto s = siter.Value();
      ++num_states;
      if (fst.Final(s) != Weight::Zero()) {
        first_pass_compactor.Compact(
            s, Arc(kNoLabel, kNoLabel, fst.Final(s), kNoStateId));
      }
      for (ArcIterator<FST> aiter(fst, s); !aiter.Done(); aiter.Next()) {
        ++num_arcs;
        first_pass_compactor.Compact(s, aiter.Value());
      }
    }
  }
  FstHeader hdr;
  hdr.SetStart(fst.Start());
  hdr.SetNumStates(num_states);
  hdr.SetNumArcs(num_arcs);
  string type = "compact";
  if (sizeof(Unsigned) != sizeof(uint32)) {
    string size;
    Int64ToStr(8 * sizeof(Unsigned), &size);
    type += size;
  }
  type += "_";
  type += Compactor::Type();
  if (CompactStore::Type() != "compact") {
    type += "_";
    type += CompactStore::Type();
  }
  const auto copy_properties = fst.Properties(kCopyProperties, true);
  if ((copy_properties & kError) || !compactor.Compatible(fst)) {
    FSTERROR() << "Fst incompatible with compactor";
    return false;
  }
  uint64 properties = copy_properties | Impl::kStaticProperties;
  internal::FstImpl<Arc>::WriteFstHeader(fst, strm, opts, file_version, type,
                                         properties, &hdr);
  first_pass_compactor.Write(strm);
  if (first_pass_compactor.Size() == -1) {
    if (opts.align && !AlignOutput(strm)) {
      LOG(ERROR) << "CompactFst::Write: Alignment failed: " << opts.source;
      return false;
    }
    Unsigned compacts = 0;
    for (StateIterator<FST> siter(fst); !siter.Done(); siter.Next()) {
      const auto s = siter.Value();
      strm.write(reinterpret_cast<const char *>(&compacts), sizeof(compacts));
      if (fst.Final(s) != Weight::Zero()) {
        ++compacts;
      }
      compacts += fst.NumArcs(s);
    }
    strm.write(reinterpret_cast<const char *>(&compacts), sizeof(compacts));
  }
  if (opts.align && !AlignOutput(strm)) {
    LOG(ERROR) << "Could not align file during write after writing states";
  }
  const auto &second_pass_compactor = compactor;
  Element element;
  for (StateIterator<FST> siter(fst); !siter.Done(); siter.Next()) {
    const auto s = siter.Value();
    if (fst.Final(s) != Weight::Zero()) {
      element = second_pass_compactor.Compact(
          s, A(kNoLabel, kNoLabel, fst.Final(s), kNoStateId));
      strm.write(reinterpret_cast<const char *>(&element), sizeof(element));
    }
    for (ArcIterator<FST> aiter(fst, s); !aiter.Done(); aiter.Next()) {
      element = second_pass_compactor.Compact(s, aiter.Value());
      strm.write(reinterpret_cast<const char *>(&element), sizeof(element));
    }
  }
  strm.flush();
  if (!strm) {
    LOG(ERROR) << "CompactFst write failed: " << opts.source;
    return false;
  }
  return true;
}

// Specialization for CompactFst; see generic version in fst.h for sample
// usage (but use the CompactFst type!). This version should inline.
template <class Arc, class Compactor, class Unsigned, class CompactStore,
          class CacheStore>
class StateIterator<
    CompactFst<Arc, Compactor, Unsigned, CompactStore, CacheStore>> {
 public:
  using StateId = typename Arc::StateId;

  explicit StateIterator(
      const CompactFst<Arc, Compactor, Unsigned, CompactStore, CacheStore> &fst)
      : nstates_(fst.GetImpl()->NumStates()), s_(0) {}

  bool Done() const { return s_ >= nstates_; }

  StateId Value() const { return s_; }

  void Next() { ++s_; }

  void Reset() { s_ = 0; }

 private:
  StateId nstates_;
  StateId s_;
};

// Specialization for CompactFst with DefaultCompactStore. Never caches,
// always iterates over the underlying compact elements.
template <class Arc, class Compactor, class Unsigned, class CacheStore>
class ArcIterator<CompactFst<
    Arc, Compactor, Unsigned,
    DefaultCompactStore<typename Compactor::Element, Unsigned>, CacheStore>> {
 public:
  using StateId = typename Arc::StateId;
  using Element = typename Compactor::Element;

  ArcIterator(
      const CompactFst<Arc, Compactor, Unsigned,
                       DefaultCompactStore<Element, Unsigned>, CacheStore> &fst,
      StateId s)
      : compactor_(fst.GetImpl()->GetCompactor()),
        state_(s),
        compacts_(nullptr),
        pos_(0),
        flags_(kArcValueFlags) {
    const auto *data = fst.GetImpl()->Data();
    size_t offset;
    if (compactor_->Size() == -1) {  // Variable out-degree compactor.
      offset = data->States(s);
      num_arcs_ = data->States(s + 1) - offset;
    } else {  // Fixed out-degree compactor.
      offset = s * compactor_->Size();
      num_arcs_ = compactor_->Size();
    }
    if (num_arcs_ > 0) {
      compacts_ = &(data->Compacts(offset));
      arc_ = compactor_->Expand(s, *compacts_, kArcILabelValue);
      if (arc_.ilabel == kNoStateId) {
        ++compacts_;
        --num_arcs_;
      }
    }
  }

  bool Done() const { return pos_ >= num_arcs_; }

  const Arc &Value() const {
    arc_ = compactor_->Expand(state_, compacts_[pos_], flags_);
    return arc_;
  }

  void Next() { ++pos_; }

  size_t Position() const { return pos_; }

  void Reset() { pos_ = 0; }

  void Seek(size_t pos) { pos_ = pos; }

  uint32 Flags() const { return flags_; }

  void SetFlags(uint32 f, uint32 m) {
    flags_ &= ~m;
    flags_ |= (f & kArcValueFlags);
  }

 private:
  const Compactor *compactor_;  // Borrowed reference.
  StateId state_;
  const Element *compacts_;  // Borrowed reference.
  size_t pos_;
  size_t num_arcs_;
  mutable Arc arc_;
  uint32 flags_;
};

// // Specialization for CompactFst.
// // This is an optionally caching arc iterator.
// // TODO(allauzen): implements the kArcValueFlags, the current
// /// implementation only implements the kArcNoCache flag.
// template <class A, class C, class U>
// class ArcIterator<CompactFst<A, C, U>> {
//  public:
//   using StateId = typename A::StateId;
//
//   ArcIterator(const CompactFst<A, C, U> &fst, StateId s)
//       : fst_(fst), state_(s), pos_(0), num_arcs_(0), offset_(0),
//         flags_(kArcValueFlags) {
//     cache_data_.ref_count = 0;
//
//     if (fst_.GetImpl()->HasArcs(state_)) {
//       fst_.GetImpl()->InitArcIterator(s, &cache_data_);
//       num_arcs_ = cache_data_.narcs;
//       return;
//     }
//
//     const C *compactor = fst_.GetImpl()->GetCompactor();
//     const DefaultCompactStore<A, C, U> *data = fst_.GetImpl()->Data();
//     if (compactor->Size() == -1) {  // Variable out-degree compactor
//       offset_ = data->States(s);
//       num_arcs_ = data->States(s + 1) - offset_;
//     } else {  // Fixed out-degree compactor
//       offset_ =  s * compactor->Size();
//       num_arcs_ = compactor->Size();
//     }
//     if (num_arcs_ > 0) {
//       const A &arc = fst_.GetImpl()->ComputeArc(s, offset_);
//       if (arc.ilabel == kNoStateId) {
//         ++offset_;
//         --num_arcs_;
//       }
//     }
//   }
//
//   ~ArcIterator() {
//     if (cache_data_.ref_count)
//       --(*cache_data_.ref_count);
//   }
//
//   bool Done() const { return pos_ >= num_arcs_; }
//
//   const A& Value() const {
//     if (cache_data_.ref_count == 0) {
//       if (flags_ & kArcNoCache) {
//         arc_ = fst_.GetImpl()->ComputeArc(state_, pos_ + offset_);
//         return arc_;
//       } else {
//         fst_.GetImpl()->InitArcIterator(state_, &cache_data_);
//       }
//     }
//     return cache_data_.arcs[pos_];
//   }
//
//   void Next() { ++pos_; }
//
//   size_t Position() const { return pos_; }
//
//   void Reset() { pos_ = 0;  }
//
//   void Seek(size_t pos) { pos_ = pos; }
//
//   uint32 Flags() const { return flags_; }
//
//   void SetFlags(uint32 f, uint32 m) {
//     flags_ &= ~m;
//     flags_ |= f;
//
//     if (!(flags_ & kArcNoCache) && cache_data_.ref_count == 0)
//       fst_.GetImpl()->InitArcIterator(state_, &cache_data_);
//   }
//
//  private:
//   mutable const CompactFst<A, C, U> &fst_;
//   StateId state_;
//   size_t pos_;
//   size_t num_arcs_;
//   size_t offset_;
//   uint32 flags_;
//   mutable A arc_;
//   mutable ArcIteratorData<A> cache_data_;
// };

// Compactor for unweighted string FSTs.
template <class A>
class StringCompactor {
 public:
  using Arc = A;
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  using Element = Label;

  Element Compact(StateId s, const Arc &arc) const { return arc.ilabel; }

  Arc Expand(StateId s, const Element &p, uint32 f = kArcValueFlags) const {
    return Arc(p, p, Weight::One(), p != kNoLabel ? s + 1 : kNoStateId);
  }

  constexpr ssize_t Size() const { return 1; }

  constexpr uint64 Properties() const {
    return kString | kAcceptor | kUnweighted;
  }

  bool Compatible(const Fst<Arc> &fst) const {
    const auto props = Properties();
    return fst.Properties(props, true) == props;
  }

  static const string &Type() {
    static const string type = "string";
    return type;
  }

  bool Write(std::ostream &strm) const { return true; }

  static StringCompactor *Read(std::istream &strm) {
    return new StringCompactor;
  }
};

// Compactor for weighted string FSTs.
template <class A>
class WeightedStringCompactor {
 public:
  using Arc = A;
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  using Element = std::pair<Label, Weight>;

  Element Compact(StateId s, const Arc &arc) const {
    return std::make_pair(arc.ilabel, arc.weight);
  }

  Arc Expand(StateId s, const Element &p, uint32 f = kArcValueFlags) const {
    return Arc(p.first, p.first, p.second,
               p.first != kNoLabel ? s + 1 : kNoStateId);
  }

  constexpr ssize_t Size() const { return 1; }

  constexpr uint64 Properties() const { return kString | kAcceptor; }

  bool Compatible(const Fst<Arc> &fst) const {
    const auto props = Properties();
    return fst.Properties(props, true) == props;
  }

  static const string &Type() {
    static const string type = "weighted_string";
    return type;
  }

  bool Write(std::ostream &strm) const { return true; }

  static WeightedStringCompactor *Read(std::istream &strm) {
    return new WeightedStringCompactor;
  }
};

// Compactor for unweighted acceptor FSTs.
template <class A>
class UnweightedAcceptorCompactor {
 public:
  using Arc = A;
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  using Element = std::pair<Label, StateId>;

  Element Compact(StateId s, const Arc &arc) const {
    return std::make_pair(arc.ilabel, arc.nextstate);
  }

  Arc Expand(StateId s, const Element &p, uint32 f = kArcValueFlags) const {
    return Arc(p.first, p.first, Weight::One(), p.second);
  }

  constexpr ssize_t Size() const { return -1; }

  constexpr uint64 Properties() const { return kAcceptor | kUnweighted; }

  bool Compatible(const Fst<Arc> &fst) const {
    const auto props = Properties();
    return fst.Properties(props, true) == props;
  }

  static const string &Type() {
    static const string type = "unweighted_acceptor";
    return type;
  }

  bool Write(std::ostream &strm) const { return true; }

  static UnweightedAcceptorCompactor *Read(std::istream &istrm) {
    return new UnweightedAcceptorCompactor;
  }
};

// Compactor for weighted acceptor FSTs.
template <class A>
class AcceptorCompactor {
 public:
  using Arc = A;
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  using Element = std::pair<std::pair<Label, Weight>, StateId>;

  Element Compact(StateId s, const Arc &arc) const {
    return std::make_pair(std::make_pair(arc.ilabel, arc.weight),
                          arc.nextstate);
  }

  Arc Expand(StateId s, const Element &p, uint32 f = kArcValueFlags) const {
    return Arc(p.first.first, p.first.first, p.first.second, p.second);
  }

  constexpr ssize_t Size() const { return -1; }

  constexpr uint64 Properties() const { return kAcceptor; }

  bool Compatible(const Fst<Arc> &fst) const {
    const auto props = Properties();
    return fst.Properties(props, true) == props;
  }

  static const string &Type() {
    static const string type = "acceptor";
    return type;
  }

  bool Write(std::ostream &strm) const { return true; }

  static AcceptorCompactor *Read(std::istream &strm) {
    return new AcceptorCompactor;
  }
};

// Compactor for unweighted FSTs.
template <class A>
class UnweightedCompactor {
 public:
  using Arc = A;
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  using Element = std::pair<std::pair<Label, Label>, StateId>;

  Element Compact(StateId s, const Arc &arc) const {
    return std::make_pair(std::make_pair(arc.ilabel, arc.olabel),
                          arc.nextstate);
  }

  Arc Expand(StateId s, const Element &p, uint32 f = kArcValueFlags) const {
    return Arc(p.first.first, p.first.second, Weight::One(), p.second);
  }

  constexpr ssize_t Size() const { return -1; }

  constexpr uint64 Properties() const { return kUnweighted; }

  bool Compatible(const Fst<Arc> &fst) const {
    const auto props = Properties();
    return fst.Properties(props, true) == props;
  }

  static const string &Type() {
    static const string type = "unweighted";
    return type;
  }

  bool Write(std::ostream &strm) const { return true; }

  static UnweightedCompactor *Read(std::istream &strm) {
    return new UnweightedCompactor;
  }
};

template <class Arc, class Unsigned /* = uint32 */>
using CompactStringFst = CompactFst<Arc, StringCompactor<Arc>, Unsigned>;

template <class Arc, class Unsigned /* = uint32 */>
using CompactWeightedStringFst =
    CompactFst<Arc, WeightedStringCompactor<Arc>, Unsigned>;

template <class Arc, class Unsigned /* = uint32 */>
using CompactAcceptorFst = CompactFst<Arc, AcceptorCompactor<Arc>, Unsigned>;

template <class Arc, class Unsigned /* = uint32 */>
using CompactUnweightedFst =
    CompactFst<Arc, UnweightedCompactor<Arc>, Unsigned>;

template <class Arc, class Unsigned /* = uint32 */>
using CompactUnweightedAcceptorFst =
    CompactFst<Arc, UnweightedAcceptorCompactor<Arc>, Unsigned>;

using StdCompactStringFst = CompactStringFst<StdArc, uint32>;

using StdCompactWeightedStringFst = CompactWeightedStringFst<StdArc, uint32>;

using StdCompactAcceptorFst = CompactAcceptorFst<StdArc, uint32>;

using StdCompactUnweightedFst = CompactUnweightedFst<StdArc, uint32>;

using StdCompactUnweightedAcceptorFst =
    CompactUnweightedAcceptorFst<StdArc, uint32>;

}  // namespace fst

#endif  // FST_LIB_COMPACT_FST_H_
