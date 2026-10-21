#pragma once
#include <cstdint>
#include <vector>

namespace MREngine {

/**
 * Finds an app's icon inside a VXP file image, without loading or running the app.
 *
 * Returns the raw icon bytes (a complete GIF, PNG, JPEG or BMP) ready to hand to
 * load_image(), or an empty vector when the package has none that can be read.
 *
 * Two sources are tried, in order:
 *
 *  1. Every `VREAPPLOGO` icon blob in the file. The container header varies between
 *     packages - some put a 3-byte variant tag then a size, one variant drops a magic
 *     byte and inserts a format tag instead, and the tag disagrees with the actual
 *     contents (packages tagged `JPG` hold a PNG, `PIC` holds a GIF). So neither the
 *     tag nor the declared size is trusted: the payload is bounded by the declared
 *     size purely to stay inside this icon, then a real image signature is looked for
 *     near its start.
 *
 *  2. The `.vm_res` resource directory, ranked by resource name. This is what recovers
 *     packages whose VRE blob is a 31-byte stub with no image behind it but which ship
 *     a usable `logo.png` or `icon.gif` alongside it.
 */
std::vector<uint8_t> extract_vxp_icon(const std::vector<uint8_t>& file);

} // namespace MREngine