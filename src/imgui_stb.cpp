// Provides the stb_truetype and stb_rect_pack implementations that a
// prebuilt system libimgui (Debian/Ubuntu) leaves undefined. Debian's
// libimgui.a is built with IMGUI_DISABLE_STB_TRUETYPE_IMPLEMENTATION
// and IMGUI_DISABLE_STB_RECT_PACK_IMPLEMENTATION, so the consumer must
// supply these symbols or linking fails with undefined references to
// stbtt_* and stbrp_*. The headers are taken from the system ImGui
// include directory (set on the target); the build falls back to the
// vendored copy if the system package does not ship them.
#define STB_RECT_PACK_IMPLEMENTATION
#include "imstb_rectpack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "imstb_truetype.h"
