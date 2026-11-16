#pragma once

namespace rf {

// The build stamp (cmake/build_stamp.cmake): "V0.9.0  BUILD 291  F0CD14F  2026-11-14 16:40" (the version, the GitHub release's build
// number when it has one, the commit, the build's date and minute), shown at the bottom right of the title screen
// (design/screens.md "Title").
const char* buildStampLine();

// The GitHub release's build number (its tag build-<n>, the commit count: docs/RELEASE_PIPELINE.md); 0 for a local build.
unsigned buildNumber();

}  // namespace rf
