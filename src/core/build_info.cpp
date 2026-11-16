#include "core/build_info.h"

#include "build_stamp.h"

namespace rf {

const char* buildStampLine() {
    // (a commit-less checkout leaves two spaces' gap, not a stray one; a local build has no build number)
    static const char kLine[] = "V" RF_VERSION RF_BUILD_LABEL "  " RF_BUILD_COMMIT "  " RF_BUILD_DATE;
    return kLine;
}

unsigned buildNumber() { return RF_BUILD_NUMBER; }

}  // namespace rf
