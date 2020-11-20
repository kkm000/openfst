// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.

#include <fst/flags.h>

DEFINE_string(arc_type, "standard", "Output arc type");
DEFINE_bool(decode, false, "Decode");
DEFINE_bool(gzip, false, "Also applies gzip (de)compression");

int fstcompress_main(int argc, char **argv);

int main(int argc, char **argv) { return fstcompress_main(argc, argv); }
