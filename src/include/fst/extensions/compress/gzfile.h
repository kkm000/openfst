// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
// Resource handles for gzip files written to or read from stringstreams. These
// are necessary to provide the compression routines with streams reading from
// or writing to compressed files (or the UNIX standard streams), and are not
// intended for general use.

#ifndef FST_EXTENSIONS_COMPRESS_GZFILE_H_
#define FST_EXTENSIONS_COMPRESS_GZFILE_H_

#include <cstddef>
#include <memory>
#include <sstream>
#include <string>

#include <fst/compat.h>
#include <fst/log.h>
#include <fst/fst.h>
#include <zlib.h>

namespace fst {

// Gives the zlib gzFile type an OO-like interface. String inputs are all
// C-style strings. The caller is responsible to get the file modes appropriate
// for the IO methods being called. The ! operator can be used to check for
// errors after construction or read/writing.
class GzFile {
 public:
  GzFile(const char *source, const char *mode)
      : gzfile_(gzopen(source, mode)), error_(gzfile_ == nullptr) {}

  ~GzFile() { gzclose(gzfile_); }

  bool operator!() const { return error_; }

  // Returns false on EOF and sets error if short read does not reach an EOF.
  int Read(void *buf, unsigned int size) {
    const auto bytes_read = gzread(gzfile_, buf, size);
    if ((bytes_read < size) && !gzeof(gzfile_)) error_ = true;
    return bytes_read;
  }

  // Sets error on short writes.
  void Write(const char *buf, unsigned int size) {
    if (gzwrite(gzfile_, buf, size) != size) error_ = true;
  }

 private:
  gzFile gzfile_;
  bool error_;
};

// Resource handle for writing stringstream to GzFile.
class OGzFile {
 public:
  explicit OGzFile(const std::string &source) : OGzFile(source.c_str()) {}

  explicit OGzFile(const char *source) : gz_(GzFile(source, "wb")) {}

  inline bool operator!() const { return !gz_; }

  void Write(const std::stringstream &ssbuf) {
    const auto sbuf = ssbuf.str();
    gz_.Write(sbuf.data(), sbuf.size());
  }

 private:
  GzFile gz_;
};

// Resource handle for reading stringstream from GzFile.
class IGzFile {
 public:
  explicit IGzFile(const std::string &source) : IGzFile(source.c_str()) {}

  explicit IGzFile(const char *source) : gz_(GzFile(source, "rb")) {}

  inline bool operator!() const { return !gz_; }

  // This is a great case for "move", but GCC 4 is missing the C+11 standard
  // move constructor for stringstream, so a unique_ptr is the next best thing.
  std::unique_ptr<std::stringstream> Read() {
    char buf[bufsize_];
    std::unique_ptr<std::stringstream> sstrm(new std::stringstream);
    // We always read at least once, and the result of the last read is always
    // pushed onto the stringstream. We use the "write" member because << onto
    // a stringstream stops at the null byte, which might be data!
    int bytes_read;
    while ((bytes_read = gz_.Read(buf, bufsize_)) == bufsize_) {
      sstrm->write(buf, bufsize_);
    }
    sstrm->write(buf, bytes_read);
    return sstrm;
  }

 private:
  GzFile gz_;
  // This is the same size as the default internal buffer for zlib.
  static const size_t bufsize_ = 8192;
};

}  // namespace fst

#endif  // FST_EXTENSIONS_COMPRESS_GZFILE_H_
