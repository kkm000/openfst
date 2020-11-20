// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.

#include <fst/flags.h>

DEFINE_string(project_type, "input",
              "Side to project from, one of: \"input\", \"output\"");

int fstproject_main(int argc, char **argv);

int main(int argc, char **argv) { return fstproject_main(argc, argv); }
