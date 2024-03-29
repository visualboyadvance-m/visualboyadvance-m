#ifndef VBAM_CORE_BASE_VERSION_H_
#define VBAM_CORE_BASE_VERSION_H_

#include <string>

#include "core/base/version_gen.h"

// Full version information, generated by the build system.
// e.g. "VisualBoyAdvance-M 2.1.9-316e4a43 msvc-316e4a43"
extern const std::string kVbamNameAndSubversion;

// Version information, generated by the build system.
// e.g. "2.1.9-316e4a43"
extern const std::string kVbamVersion;

#endif  /* VBAM_CORE_BASE_VERSION_H_ */
