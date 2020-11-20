// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.

#ifndef FST_EXTENSIONS_COMPRESS_COMPRESSSCRIPT_H_
#define FST_EXTENSIONS_COMPRESS_COMPRESSSCRIPT_H_

#include <string>
#include <tuple>

#include <fst/extensions/compress/compress.h>
#include <fst/script/arg-packs.h>
#include <fst/script/fst-class.h>

namespace fst {
namespace script {

using CompressInnerArgs =
    std::tuple<const FstClass &, const std::string &, const bool>;

using CompressArgs = WithReturnValue<bool, CompressInnerArgs>;

template <class Arc>
void Compress(CompressArgs *args) {
  const Fst<Arc> &fst = *std::get<0>(args->args).GetFst<Arc>();
  const auto &source = std::get<1>(args->args);
  const auto gzip = std::get<2>(args->args);
  args->retval = Compress(fst, source, gzip);
}

bool Compress(const FstClass &fst, const std::string &source, const bool gzip);

using DecompressInnerArgs =
    std::tuple<const std::string &, MutableFstClass *, const bool>;

using DecompressArgs = WithReturnValue<bool, DecompressInnerArgs>;

template <class Arc>
void Decompress(DecompressArgs *args) {
  const auto &source = std::get<0>(args->args);
  MutableFst<Arc> *fst = std::get<1>(args->args)->GetMutableFst<Arc>();
  const auto gzip = std::get<2>(args->args);
  args->retval = Decompress(source, fst, gzip);
}

bool Decompress(const std::string &source, MutableFstClass *fst,
                const bool gzip);

}  // namespace script
}  // namespace fst

#endif  // FST_EXTENSIONS_COMPRESS_COMPRESSSCRIPT_H_
