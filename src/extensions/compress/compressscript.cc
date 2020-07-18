// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.

#include <fst/extensions/compress/compressscript.h>

#include <fst/arc-map.h>
#include <fst/script/script-impl.h>

namespace fst {
namespace script {

bool Compress(const FstClass &fst, const std::string &filename,
              const bool gzip) {
  CompressInnerArgs iargs(fst, filename, gzip);
  CompressArgs args(iargs);
  Apply<Operation<CompressArgs>>("Compress", fst.ArcType(), &args);
  return args.retval;
}

bool Decompress(const std::string &filename, MutableFstClass *fst,
                const bool gzip) {
  DecompressInnerArgs iargs(filename, fst, gzip);
  DecompressArgs args(iargs);
  Apply<Operation<DecompressArgs>>("Decompress", fst->ArcType(), &args);
  return args.retval;
}

// Register operations for common arc types.

REGISTER_FST_OPERATION(Compress, StdArc, CompressArgs);
REGISTER_FST_OPERATION(Compress, LogArc, CompressArgs);
REGISTER_FST_OPERATION(Compress, Log64Arc, CompressArgs);

REGISTER_FST_OPERATION(Decompress, StdArc, DecompressArgs);
REGISTER_FST_OPERATION(Decompress, LogArc, DecompressArgs);
REGISTER_FST_OPERATION(Decompress, Log64Arc, DecompressArgs);

}  // namespace script
}  // namespace fst
