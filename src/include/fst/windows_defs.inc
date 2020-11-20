// Aggregator file for common things that need to be defined when building
// for Windows.
#ifdef _WIN32
#include <basetsd.h>
using ssize_t = SSIZE_T;
#endif