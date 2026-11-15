#pragma once
// Which platform this build is for (docs/IOS.md): RF_IOS on iPhone and iPad, RF_MOBILE on a phone or tablet of either kind (a
// full-screen window, touch first, files beside the app). The desktop dev build is neither.

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && defined(TARGET_OS_IOS) && TARGET_OS_IOS
#define RF_IOS 1
#else
#define RF_IOS 0
#endif

#if defined(__ANDROID__) || RF_IOS
#define RF_MOBILE 1
#else
#define RF_MOBILE 0
#endif
