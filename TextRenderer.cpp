/*

    Help render a bunch of text into a texture.
    Output a 2D image (texture) that will be used by later stuff

    Much of the code is directly taken from the following online examples:
    - https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c
    - https://freetype.org/freetype2/docs/tutorial/example1.c

*/

#include "TextRenderer.hpp"

#include <math.h>
#include <hb.h>
#include <hb-ft.h>

TextRenderer::TextRenderer(std::string const text_to_render, std::string const font_path_name)
: text(text_to_render), font_path(font_path_name) {
    // fontfile: path to font. It's all C again
    const char *fontfile = font_path.c_str();

    if ((ft_error = FT_Init_FreeType (&ft_library)))
        abort();
    if ((ft_error = FT_New_Face (ft_library, fontfile, 0, &ft_face)))
        abort();
    if ((ft_error = FT_Set_Char_Size (ft_face, FONT_SIZE*64, FONT_SIZE*64, 0, 0)))
        abort();
    
    /* Create hb-ft font. */
    hb_font = hb_ft_font_create (ft_face, NULL);

    // Everything below might belong to rasterization realm instead.
}

std::vector< glm::u8vec4 > TextRenderer::Rasterize(size_t length, size_t &width, size_t &height) {
    // width and height are return values.
    // The size of the rendered image is dynamic!

    /* Create hb-buffer and populate. */
    hb_buffer_t *hb_buffer;
    hb_buffer = hb_buffer_create ();
    hb_buffer_add_utf8 (hb_buffer, text.c_str(), -1, 0, -1);
    hb_buffer_guess_segment_properties (hb_buffer);

    /* Shape it! */
    hb_shape (hb_font, hb_buffer, NULL, 0);

    /* Get glyph information and positions out of the buffer. */
    unsigned int len = hb_buffer_get_length (hb_buffer);
    hb_glyph_info_t *info = hb_buffer_get_glyph_infos (hb_buffer, NULL);
    hb_glyph_position_t *pos = hb_buffer_get_glyph_positions (hb_buffer, NULL);

    /* Rasterize at absolute positions. */
    
    // positions
    size_t current_x = 0;
    size_t current_y = 0;

    // freetype stubs
    FT_GlyphSlot slot = ft_face->glyph;
    FT_Bitmap &bitmap = ft_face->glyph->bitmap;
    FT_Error error;
    
    // ensure length is not too large
    if (text.length() > length) {
        length = text.length();
    }

    // calculate expected size of the raster
    size_t image_width = 0;
    size_t image_height = 0;
    for (size_t idx = 0; idx < length; idx++) {
        current_x += pos[idx].x_advance;
        current_y += pos[idx].y_advance;
        image_width = std::max(image_width, current_x + FONT_SIZE * 64);
        image_height = std::max(image_height, current_y + FONT_SIZE * 64);
    }

    // make image
    std::vector<glm::u8vec4> image(image_width * image_height);

    // Finally, Rasterize
    for (size_t idx = 0; idx < length; idx++) {
        hb_codepoint_t gid   = info[idx].codepoint;
        unsigned int cluster = info[idx].cluster;
        size_t x_position = current_x + pos[idx].x_offset;
        size_t y_position = current_y + pos[idx].y_offset;

        // Load a char
        error = FT_Load_Char(ft_face, text[idx], FT_LOAD_RENDER);
        
        FT_Int i, j, p, q;
        FT_Int x_max = x_position + bitmap.width;
        FT_Int y_max = y_position + bitmap.rows;

        /* for simplicity, we assume that `bitmap->pixel_mode' */
        /* is `FT_PIXEL_MODE_GRAY' (i.e., not a bitmap font)   */

        for (i = x_position, p = 0; i < x_max; i++, p++)
        {
            for (j = y_position, q = 0; j < y_max; j++, q++)
            {
                if (i < 0 || j < 0 ||
                    i >= image_width || j >= image_height)
                    continue;

                // blit
                uint8_t alpha = bitmap.buffer[q * bitmap.width + p];
                image[j * image_width + i] = glm::vec4(0xff, 0xff, 0xff, alpha);
            }
        }

        // Advance!
        current_x += pos[idx].x_advance;
        current_y += pos[idx].y_advance;
    }
    
    hb_buffer_destroy (hb_buffer);

    // Set return values.
    width = image_width;
    height = image_height;

    return image;
}

TextRenderer::~TextRenderer() {
    hb_font_destroy (hb_font);

    FT_Done_Face (ft_face);
    FT_Done_FreeType (ft_library);
}