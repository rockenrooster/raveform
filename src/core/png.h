#pragma once
// Minimal PNG writer: 8-bit RGB, zlib stream of stored (uncompressed) deflate blocks, CRC32 and
// Adler32. No third-party code. Screenshots are a debug action, so this may allocate.

#include "core/types.h"

namespace rf {

enum class PixelLayout { RGBA8, BGRA8 };

// `src` is the physical swapchain image (4 bytes per pixel, tightly packed rows). `rotationDeg` is the
// clockwise pre-rotation that was applied to the content (0/90/180/270); the writer undoes it so the
// PNG shows the logical, upright landscape image.
bool writePngUpright(const char* path, const u8* src, u32 srcWidth, u32 srcHeight, PixelLayout layout,
                     int rotationDeg);

}  // namespace rf
