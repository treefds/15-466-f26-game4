#include "GL.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <list>
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>

#include <hb.h>
#include <hb-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

constexpr int FONT_SIZE = 36;
constexpr int MARGIN = static_cast<int>(FONT_SIZE * 0.5f);

// The Text Renderer class; has a method that can render text.
struct TextRenderer {
    TextRenderer() = default;
    TextRenderer(std::string const text_to_render, std::string const font_path_name);
    virtual ~TextRenderer();

    std::vector< glm::u8vec4 > Rasterize(size_t length, size_t &width, size_t &height);
    

    // Custom Data
    std::string text = "";
    std::string font_path = "";

private:
    /* FreeType stuff ---------- */
    FT_Library ft_library;
    FT_Face ft_face;
    FT_Error ft_error;

    /* Harfbuzz ---------------- */

    // The HB font
    hb_font_t *hb_font;

    // Formatting related


};