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

    // length is going to be ignored now. due to ligature and stuff

    /* Create hb-buffer and populate. */
    hb_buffer_t *hb_buffer;
    hb_buffer = hb_buffer_create ();
    hb_buffer_add_utf8 (hb_buffer, text.c_str(), -1, 0, -1);
    hb_buffer_guess_segment_properties (hb_buffer);

    /* Shape it! */
    hb_shape (hb_font, hb_buffer, NULL, 0);

    /* Get glyph information and positions out of the buffer. */
    unsigned int len = hb_buffer_get_length(hb_buffer);
    hb_glyph_info_t *info = hb_buffer_get_glyph_infos(hb_buffer, NULL);
    hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(hb_buffer, NULL);

    /* Rasterize at absolute positions. */
    
    // positions
    float current_x = 0.0f;
    float current_y = 0.0f;

    // freetype stubs
    FT_Bitmap &bitmap = ft_face->glyph->bitmap;
    FT_Error error;
    
    // ensure length is not too large
    if (text.length() > length) {
        // unused for now
        length = text.length();
    }

    // calculate expected size of the raster
    size_t image_width = 0;
    size_t image_height = 0;
    for (size_t idx = 0; idx < len; idx++) {
        current_x += pos[idx].x_advance / 64.0f;
        current_y += pos[idx].y_advance / 64.0f;
        image_width = std::max(image_width, static_cast<size_t>(current_x) + FONT_SIZE * 2);
        image_height = std::max(image_height, static_cast<size_t>(current_y) + FONT_SIZE * 2);
    }

    current_x = 0.0f;
    current_y = 0.0f;

    // make image
    std::vector<glm::u8vec4> image(image_width * image_height);

    // Finally, Rasterize
    for (size_t idx = 0; idx < len; idx++) {
        // load a glyph (including ligature)
        error = FT_Load_Glyph(ft_face, info[idx].codepoint, FT_LOAD_RENDER);
        if (error) {
            //...
            printf("Error loading Glyph\n");
        }
        
        int x_position_origin = current_x + pos[idx].x_offset;
        int y_position_origin = current_y + pos[idx].y_offset;

        int x_position = static_cast<int>(x_position_origin) + ft_face->glyph->bitmap_left;
        // FONT SIZE is the baseline of the font (line below g,q,p...)
        int y_position = FONT_SIZE - (static_cast<int>(y_position_origin) + ft_face->glyph->bitmap_top);

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
                    i >= static_cast<int>(image_width) || j >= static_cast<int>(image_height))
                    continue;
                // blit
                uint8_t alpha = bitmap.buffer[q * bitmap.pitch + p];
                image[(image_height - j - 1) * image_width + i] = glm::vec4(0xff, 0xff, 0xff, alpha);
            }
        }
        // Advance!
        current_x += pos[idx].x_advance / 64.0f;
        current_y += pos[idx].y_advance / 64.0f;
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