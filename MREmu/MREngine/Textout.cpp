#include "Graphic.h"
#include "../Memory.h"
#include <vmgraph.h>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Image.hpp>

extern MREngine::Graphic* graphic;

static sf::Color rgb565_to_8888(unsigned short color) {
    return sf::Color(VM_COLOR_GET_RED(color), VM_COLOR_GET_GREEN(color), VM_COLOR_GET_BLUE(color));
}
static unsigned short rgb8888_to_565(sf::Color color) {
    return VM_COLOR_888_TO_565(color.r, color.g, color.b);
}
static sf::Color blend2colors(sf::Color fg, sf::Color bg) {
    unsigned int alpha = fg.a + 1;
    unsigned int inv_alpha = 256 - fg.a;
    sf::Color res;
    res.r = (unsigned char)((alpha * fg.r + inv_alpha * bg.r) >> 8);
    res.g = (unsigned char)((alpha * fg.g + inv_alpha * bg.g) >> 8);
    res.b = (unsigned char)((alpha * fg.b + inv_alpha * bg.b) >> 8);
    res.a = 0xff;
    return res;
}

VMINT vm_graphic_get_character_height(void) {
    return 16; // temp
}

VMINT vm_graphic_get_character_width(VMWCHAR c);

VMINT vm_graphic_get_string_width(VMWSTR str);

VMINT vm_graphic_get_string_height(VMWSTR str);

VMINT vm_graphic_measure_character(VMWCHAR c, VMINT* width, VMINT* height);

VMINT vm_graphic_get_character_info(VMWCHAR c, vm_graphic_char_info* char_info);

void vm_graphic_set_font(font_size_t size);

void vm_graphic_textout(VMUINT8* disp_buf, VMINT x, VMINT y, VMWSTR s, VMINT length, VMUINT16 color) {
	if (disp_buf == 0)
		return;

    if (!graphic ||!graphic->font_is_ready)
        return;

    sf::Text text;
    text.setFont(graphic->font);
    //text.setString(std::wstring((wchar_t*)s));
    text.setString(std::wstring((wchar_t*)s));
    text.setCharacterSize(16);
    text.setFillColor(rgb565_to_8888(color));

    auto text_size = text.getLocalBounds();

	int width = text_size.left + text_size.width;
	int height = text_size.top + text_size.height;
    
    sf::RenderTexture rt;
    rt.create(width, height);
    rt.clear(sf::Color::Transparent);
    rt.draw(text);
    rt.display();

    sf::Image im = rt.getTexture().copyToImage();
    sf::Color* text_buf = (sf::Color*)im.getPixelsPtr();

	MREngine::canvas_signature* cs_dst = (MREngine::canvas_signature*)(disp_buf - VM_CANVAS_DATA_OFFSET);
	if (memcmp(cs_dst->magic, CANVAS_MAGIC, 9))
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);


	int st_x = std::max(0, x);
	int st_y = std::max(0, y);

	int end_x = std::min<int>(cfp_dst->width, x + width);
	int end_y = std::min<int>(cfp_dst->height, y + height);

	auto& clip = get_current_app_graphic().clip;
	if (clip.flag) {
		if (st_x < clip.left)
			st_x = clip.left;
		if (st_y < clip.top)
			st_y = clip.top;
		if (end_x > clip.right)
			end_x = clip.right;
		if (end_y > clip.bottom)
			end_y = clip.bottom;
	}

	for (int sy = st_y; sy < end_y; ++sy)
		for (int sx = st_x; sx < end_x; ++sx) {
			int im_x = sx - x;
			int im_y = sy - y;

            if (text_buf[im_y * width + im_x].a == 0)
                continue;
            auto bl = blend2colors(text_buf[im_y * width + im_x], rgb565_to_8888(buf16_dst[sy * cfp_dst->width + sx]));
            buf16_dst[sy * cfp_dst->width + sx] = rgb8888_to_565(bl);
			//buf16_dst[sy * cfp_dst->width + sx] = rgb8888_to_565(text_buf[im_y * width + im_x]);
		}
}

void vm_graphic_textout_by_baseline(VMUINT8* disp_buf, VMINT x, VMINT y, VMWSTR s, VMINT length, VMUINT16 color, VMINT baseline);

VM_GDI_RESULT vm_font_set_font_size(VMINT size);

VM_GDI_RESULT vm_font_set_font_style(VMINT bold, VMINT italic, VMINT underline);

VM_GDI_RESULT vm_graphic_textout_to_layer(VMINT handle, VMINT x, VMINT y, VMWSTR s, VMINT length) {
    auto& layers = get_current_app_graphic().layers;

    if (handle < 0 || handle >= layers.size())
        return VM_GDI_FAILED;

    auto& layer = layers[handle];

    vm_graphic_textout((VMUINT8*)layer.buf, x, y, s, length, get_current_app_graphic().global_color.vm_color_565);

    return VM_GDI_SUCCEED;
}

VMINT vm_graphic_get_string_baseline(VMWSTR string);

VM_GDI_RESULT vm_graphic_textout_to_layer_by_baseline(VMINT handle, VMINT x, VMINT y, VMWSTR s, VMINT length, VMINT baseline);

VMINT vm_graphic_is_use_vector_font(void);

VM_GDI_RESULT vm_graphic_draw_abm_text(VMINT handle, VMINT x, VMINT y, VMINT color, VMUINT8* font_data, VMINT font_width, VMINT font_height);

VMUINT vm_graphic_get_char_num_in_width(VMWCHAR* string, VMUINT width, VMINT  checklinebreak, VMUINT gap);

VMUINT vm_graphic_get_char_num_in_width_ex(VMWCHAR* string, VMUINT width, VMINT  checklinebreak, VMUINT gap);

VMUINT vm_get_string_width_height_ex(
    VMWCHAR* string,
    VMINT gap,
    VMINT n,
    VMINT* pWidth,
    VMINT* pHeight,
    VMINT max_width,
    VMUINT8 checkLineBreak,
    VMUINT8 checkCompleteWord);

vm_font_engine_error_message_enum vm_graphic_show_truncated_text(VM_GDI_HANDLE dest_layer_handle,
    VMINT x,
    VMINT y,
    VMINT xwidth,
    VMWCHAR* st,
    VMWCHAR* truncated_symbol,
    VMINT bordered,
    VMUINT16 color);

VMINT vm_graphic_get_character_height_ex(VMUWCHAR c);

VMINT vm_graphic_get_highest_char_height_of_all_language(void);

VMINT vm_graphic_get_char_height_alllang(VMINT size);

VMINT vm_graphic_get_char_baseline_alllang(VMINT size);