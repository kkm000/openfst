// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.

#include <fst/script/text-io.h>

#include <cstring>
#include <fstream>
#include <ostream>
#include <sstream>
#include <utility>

#include <fst/types.h>
#include <fst/log.h>
#include <fstream>
#include <fst/util.h>

namespace fst {
namespace script {

// Reads vector of weights; returns true on success.
bool ReadPotentials(const string &weight_type, const string &filename,
                    std::vector<WeightClass> *potentials) {
  std::ifstream strm(filename.c_str());
  if (!strm.good()) {
    LOG(ERROR) << "ReadPotentials: Can't open file: " << filename;
    return false;
  }
  static const int kLineLen = 8096;
  char line[kLineLen];
  size_t nline = 0;
  potentials->clear();
  while (!strm.getline(line, kLineLen).fail()) {
    ++nline;
    std::vector<char *> col;
    SplitToVector(line, "\n\t ", &col, true);
    if (col.size() == 0 || col[0][0] == '\0') continue;
    if (col.size() != 2) {
      FSTERROR() << "ReadPotentials: Bad number of columns, "
                 << "file = " << filename << ", line = " << nline;
      return false;
    }
    ssize_t s = StrToInt64(col[0], filename, nline, false);
    WeightClass weight(weight_type, col[1]);
    while (potentials->size() <= s) {
      potentials->push_back(WeightClass::Zero(weight_type));
    }
    potentials->back() = weight;
  }
  return true;
}

// Writes vector of weights; returns true on success.
bool WritePotentials(const string &filename,
                     const std::vector<WeightClass> &potentials) {
  std::ofstream fstrm;
  if (!filename.empty()) {
    fstrm.open(filename.c_str());
    if (!fstrm) {
      LOG(ERROR) << "WritePotentials: Can't open file: " << filename;
      return false;
    }
  }
  std::ostream &ostrm = fstrm.is_open() ? fstrm : std::cout;
  ostrm.precision(9);
  for (auto s = 0; s < potentials.size(); ++s)
    ostrm << s << "\t" << potentials[s] << "\n";
  if (ostrm.fail())
    LOG(ERROR) << "WritePotentials: Write failed: "
               << (filename.empty() ? "standard output" : filename);
  return !ostrm;
}

}  // namespace script
}  // namespace fst
