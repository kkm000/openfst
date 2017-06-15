#include <fst/mapped-file.h>

namespace fst {

MappedFile::MappedFile(const MemoryRegion &region) : region_(region) {
}

MappedFile::~MappedFile() {
}

MappedFile* MappedFile::Map(std::istream *istrm, bool memorymap,
                            const string &source, size_t size) {
  return NULL;
}

MappedFile* MappedFile::Allocate(size_t size, int align) {
  return NULL;
}

MappedFile* MappedFile::Borrow(void *data) {
  return NULL;
}

}  // namespace fst
