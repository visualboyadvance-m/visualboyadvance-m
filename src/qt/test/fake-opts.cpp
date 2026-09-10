// Fake implementation of the gopts object for the unit tests, which link the
// option system but not opts.cpp (that one needs the running application).
#include "qt/opts.h"

opts_t gopts;

opts_t::opts_t() {}
