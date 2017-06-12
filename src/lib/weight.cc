#include <fst/weight.h>

DEFINE_string(fst_weight_separator, ",",
              "Character separator between printed composite weights; "
              "must be a single character");

DEFINE_string(fst_weight_parentheses, "",
              "Characters enclosing the first weight of a printed composite "
              "weight (e.g., pair weight, tuple weight and derived classes) to "
              "ensure proper I/O of nested composite weights; "
              "must have size 0 (none) or 2 (open and close parenthesis)");

namespace fst {

CompositeWeightWriter::CompositeWeightWriter(std::ostream &ostrm)
    : ostrm_(ostrm), i_(0) {
  if (FLAGS_fst_weight_separator.size() != 1) {
    FSTERROR() << "CompositeWeightWriter: "
               << "FLAGS_fst_weight_separator.size() is not equal to 1";
    ostrm.clear(std::ios::badbit);
    return;
  }
  if (!FLAGS_fst_weight_parentheses.empty()) {
    if (FLAGS_fst_weight_parentheses.size() != 2) {
      FSTERROR() << "CompositeWeightWriter: "
                 << "FLAGS_fst_weight_parentheses.size() is not equal to 2";
      ostrm.clear(std::ios::badbit);
      return;
    }
  }
}

void CompositeWeightWriter::WriteBegin() {
  if (!FLAGS_fst_weight_parentheses.empty()) {
    ostrm_ << FLAGS_fst_weight_parentheses[0];
  }
}

void CompositeWeightWriter::WriteEnd() {
  if (!FLAGS_fst_weight_parentheses.empty()) {
    ostrm_ << FLAGS_fst_weight_parentheses[1];
  }
}

CompositeWeightReader::CompositeWeightReader(std::istream &istrm)
    : istrm_(istrm),
      c_(0),
      has_parens_(false),
      depth_(0),
      open_paren_(0),
      close_paren_(0) {
  if (FLAGS_fst_weight_separator.size() != 1) {
    FSTERROR() << "ComposeWeightReader: "
               << "FLAGS_fst_weight_separator.size() is not equal to 1";
    istrm_.clear(std::ios::badbit);
    return;
  }
  separator_ = FLAGS_fst_weight_separator[0];
  if (!FLAGS_fst_weight_parentheses.empty()) {
    if (FLAGS_fst_weight_parentheses.size() != 2) {
      FSTERROR() << "ComposeWeightReader: "
                 << "FLAGS_fst_weight_parentheses.size() is not equal to 2";
      istrm_.clear(std::ios::badbit);
      return;
    }
    has_parens_ = true;
    open_paren_ = FLAGS_fst_weight_parentheses[0];
    close_paren_ = FLAGS_fst_weight_parentheses[1];
  }
}

void CompositeWeightReader::ReadBegin() {
  do {  // Skips whitespace.
    c_ = istrm_.get();
  } while (std::isspace(c_));
  if (has_parens_) {
    if (c_ != open_paren_) {
      FSTERROR() << "CompositeWeightReader: Open paren missing: "
                 << "fst_weight_parentheses flag set correcty?";
      istrm_.clear(std::ios::badbit);
      return;
    }
    ++depth_;
    c_ = istrm_.get();
  }
}

void CompositeWeightReader::ReadEnd() {
  if (c_ != EOF && !std::isspace(c_)) {
    FSTERROR() << "CompositeWeightReader: excess character: '"
               << static_cast<char>(c_)
               << "': fst_weight_parentheses flag set correcty?";
    istrm_.clear(std::ios::badbit);
  }
}

}  // namespace fst
