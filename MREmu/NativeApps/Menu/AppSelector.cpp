#include "AppSelector.h"
#include "BottomSheet.h"
#include "WallpaperData.h"
#include "IconsData.h"
#include "../../MREngine/Image.h"
#include "../../MREngine/VxpIcon.h"
#include "../../AppManager.h"
#include "../../DragAndDrop.h"
#include <vector>
#include <string>
#include <cctype>
#include <cstring>
#include <vmcert.h>
#include <vmgraph.h>
#include <vmio.h>
#include <vmpromng.h>
#include <vmstdlib.h>
#include <vmtimer.h>
#include <algorithm>
#include <cmath>

VMWSTR vm_ucs2_string(VMSTR s);
extern AppManager* g_appManager;

namespace NativeApps::Menu::AppSelector {
	int layer_h = 0;
	VMUINT8* layer_buf = 0;
	int w = 0, h = 0;
	int c_h = 0, b_h = 0;
	int img_wh = 0;

	int scroll_pos = 0;
	int touch_start_y = 0;
	int touch_last_y = 0;
	bool touched = false;
	int touch_time = 0;

	int marquee_offset = 0;
	int marquee_timer_id = 0;
	int marquee_state = 0;
	int marquee_pause_ticks = 28;
	int last_selected = 0;

	VMUINT last_scroll_time = 0;
	bool scrollbar_visible = false;

	BottomSheet details_sheet;
	BottomSheet confirm_sheet;

	VMUINT16 gray = VM_COLOR_888_TO_565(50, 50, 50);

	// =========================================================================
	// Grid appearance
	//
	// Everything that decides how the grid looks, gathered here so it can be
	// tuned without reading the drawing code.
	//
	// A name ending in Pct is a percentage, a one ending in Px is already in
	// pixels, and one ending in Div is an integer divisor - percentages are
	// used only where the underlying expression really was a percentage, since
	// for the rest the round number does not give the same integer split.
	// =========================================================================

	// --- Shape of the grid --------------------------------------------------

	constexpr int grid_cols = 4;
	constexpr int grid_rows_per_page = 4;

	// Space left between neighbouring tiles, as a percentage of the cell pitch,
	// floored at kMinGapPx.
	//
	// Rows and columns are separate constants because they are separate design
	// decisions - a deeper row gap reads as grouping, a wider column gap as
	// columns. Be aware of the geometry, though: the tile is square and so is
	// the cell, so the tile is sized from the *larger* of the two and the
	// smaller is absorbed by the centring. Differing values do nothing until
	// the pitch is made rectangular.
	static constexpr int kColGapPct = 20;
	static constexpr int kRowGapPct = 20;
	static constexpr int kMinGapPx = 2;

	// Bounds on the cell pitch, so the grid neither collapses nor turns into a
	// handful of enormous tiles on a stepped-up screen.
	static constexpr int kMinCellPx = 12;
	static constexpr int kMaxCellPx = 64;

	// Height of the title and softkey bars: the text height plus this much
	// breathing room.
	static constexpr int kBarPadPx = 6;

	// Both bars are the same flat dark fill rather than a dim over the
	// wallpaper: a translucent one lets the wallpaper show through the text.
	static constexpr int kBarBgR = 12, kBarBgG = 16, kBarBgB = 28;

	// Inset of the outermost softkey label from the screen edge, and of the
	// title's text from the left.
	static constexpr int kBarMarginPx = 4;

	// A held softkey gets a block behind it, not just a colour change on the
	// letters. It hugs the text - sized to the glyph cell rather than the whole
	// bar, or it fills the strip and the label appears to float on it.
	static constexpr int kPressPadXPx = 5;
	static constexpr int kPressPadYPx = 1;
	static constexpr int kPressR = 232, kPressG = 236, kPressB = 243;

	// Letters on a pressed softkey, dark against that block.
	static constexpr int kPressInkR = 10, kPressInkG = 12, kPressInkB = 20;

	// --- Tile shape ---------------------------------------------------------

	// Superellipse exponent for the corners. They are continuous curves rather
	// than circular arcs, which is what makes them read as one shape with the
	// straight edges instead of a circle tangent to four lines.
	//
	// n and the radius are a pair, not independent knobs: raising n fills the
	// corner in, so a big radius at n = 4 still leaves the tip looking square.
	// n = 3 with kCornerRadiusPct is round enough to read as rounded while
	// keeping the continuous curve a circle does not have.
	static constexpr float kSquircleN = 3.0f;

	// Corner radius, as a percentage of the tile's side, floored so a very
	// small tile still has a corner to round.
	static constexpr int kCornerRadiusPct = 44;
	static constexpr int kMinRadiusPx = 2;

	// --- Selection ----------------------------------------------------------

	// Accent blue, shared by the selection outline and the title bar's text so
	// the two read as one thing. Both used to be plain white, so they read as
	// unrelated.
	static constexpr int kAccentR = 126, kAccentG = 178, kAccentB = 242;

	// Thickness of the outline around the selected tile, in pixels.
	//
	// Drawn as a slightly larger squircle *underneath* the tile rather than as
	// a frame on top of it. vm_graphic_roundrect has circular corners, so a
	// frame from it sits on a squircle as a visibly different shape; growing
	// the tile's own mask instead follows the curve exactly at any size.
	static constexpr int kFocusRimPx = 2;
	static constexpr int kFocusAlpha = 235;

	// --- Tile chrome --------------------------------------------------------

	// Package artwork is drawn [num]/[den] of the tile and centred, so the clip
	// throws away its outermost band. See the overdraw comment in draw_tile().
	static constexpr int kIconOverdrawNum = 11;
	static constexpr int kIconOverdrawDen = 10;

	// There is deliberately no drop shadow under the tiles. A dark squircle
	// offset down and right reads as a rim rather than as depth at this size:
	// the tile's own antialiased edge blends against whatever is underneath it,
	// so a dark shape under a light tile doubles the fringe instead of
	// softening it, and the icons came out looking outlined in grey.

	// The "Switch to List View" tile gets a pale plate instead of app artwork,
	// so that it does not read as another entry.
	static constexpr int kSwitchPlateR = 236, kSwitchPlateG = 240, kSwitchPlateB = 248;
	static constexpr int kSwitchPlateAlpha = 235;

	// Glyph box on that tile, as a fraction of the tile side, centred.
	static constexpr int kGlyphBoxNum = 5;
	static constexpr int kGlyphBoxDen = 8;

	// Ink for every glyph the grid draws itself, on a pale plate.
	static constexpr int kInkR = 44, kInkG = 52, kInkB = 76;

	// --- Folder and placeholder tiles ---------------------------------------
	//
	// Neither is real artwork. A package with no extractable icon gets the
	// placeholder, a directory gets the folder; see docs/vxp_icons.md for why
	// some packages are in that state.

	static constexpr int kFolderPlateR = 226, kFolderPlateG = 230, kFolderPlateB = 238;
	static constexpr int kFolderTabR = 246, kFolderTabG = 202, kFolderTabB = 74;
	static constexpr int kFolderBodyR = 250, kFolderBodyG = 214, kFolderBodyB = 100;
	static constexpr int kFolderEdgeR = 176, kFolderEdgeG = 132, kFolderEdgeB = 30;

	// Folder body, as a fraction of the tile. Kept clear of the plate's own
	// corner curve: at three quarters of the tile the body's right edge ran
	// into it and got chopped off, which made the folder look broken rather
	// than merely tight.
	static constexpr int kFolderGlyphWPct = 64;
	static constexpr int kFolderGlyphHPct = 42;
	static constexpr int kFolderGlyphYDiv = 16;  // pushed down, as size / 16
	static constexpr int kFolderTabWPct = 45;
	static constexpr int kFolderLipDiv = 5;     // lip thickness, as gh / 5

	// The darker band along the folder's bottom edge. Kept faint: at full
	// strength it read as a separate stripe under the folder rather than as its
	// shaded edge.
	static constexpr int kFolderEdgeAlpha = 110;

	// Placeholder mark: the gamepad from res/icons/source/gamepad.png. Its ink
	// matches kInk so both of the grid's own glyphs read as one set, but it is a
	// separate constant because a plate that light can take a heavier mark.
	static constexpr int kPlaceholderPlateR = 214, kPlaceholderPlateG = 219,
		kPlaceholderPlateB = 230;
	static constexpr int kPlaceholderInkR = 44, kPlaceholderInkG = 52,
		kPlaceholderInkB = 76;
	static constexpr int kPlaceholderGlyphNum = 3;
	static constexpr int kPlaceholderGlyphDen = 4;

	const VMUINT16 accent_blue = VM_COLOR_888_TO_565(kAccentR, kAccentG, kAccentB);

	int m_i = 0;

	struct vxp {
		std::u16string name;
		std::u16string path;
		VMINT_CANVAS img = 0;      // list-sized icon, decoded during scan()
		VMINT_CANVAS img_big = 0;  // grid-sized icon, decoded on the first grid draw
		std::vector<uint8_t> icon_data;  // kept so the big icon can be decoded later
		size_t file_size = 0;
		std::u16string app_name;
		std::u16string dev_name;
		bool is_dir = false;
	};

	std::u16string current_dir = u"e:\\mre\\";
	std::vector<vxp> vxps;

	// --- Layouts ------------------------------------------------------
	//
	// Two layouts share one selection index. m_i addresses vxps in both of them,
	// plus a trailing switch item at index vxps.size(): a centred row pinned to
	// the bottom of the list, and the last tile of the grid. So anything that
	// reads vxps[m_i] has to rule out is_switch_item(m_i) first.
	// -------------------------------------------------------------------------

	enum class View { List, Grid };
	View view = View::List;

	// Grid geometry. b_h/img_wh above stay the *list* metrics; everything here is
	// derived in compute_screen_metrics() from the screen size alone, so the grid
	// icons can be decoded before scan() knows how many entries there are.
	int title_h = 0;    // top bar carrying the selected entry's name
	int softkey_h = 0;  // bottom Options / Choose / Back row
	int grid_top = 0;   // first row of tiles, just under the title bar
	int grid_h = 0;     // height available to the tiles
	int cell = 0;       // tile pitch, square
	int tile = 0;       // icon size inside a cell
	int grid_icon = 0;  // size the grid icons are decoded at

	// Rows shown at once. The grid pages: it is always exactly 4x4 icons, and the
	// offset is always a whole number of pages. A partially scrolled grid looks
	// like a broken one - a half row of icons under the title bar reads as a
	// clipping bug rather than as content continuing past the fold.
	// grid_cols and grid_rows_per_page are with the other grid constants.

	// Wallpaper behind both layouts. Two buffers because the list needs the image
	// pushed further back than the grid does; see kGridDim and kListDim.
	std::vector<uint16_t> wallpaper;
	std::vector<uint16_t> wallpaper_list;

	// How far each layout pushes the wallpaper back, 0 = untouched, 255 = flat
	// tint. The list sits on the image rather than over it, and white 15px text on
	// anything but near-black needs more headroom than an icon tile that has its
	// own artwork behind it.
	//
	// These are low because the wallpaper is already a dark night sky. They were
	// roughly 2.5x higher when the image was a bright low-poly pattern, and that
	// crushed the galaxy to a black rectangle.
	//
	// The list still needs more than the grid: its wallpaper has a bright band
	// across the middle where the galactic core sits, and that lands exactly
	// where the row labels are.
	static constexpr int kGridDim = 64;
	static constexpr int kListDim = 70;

	// The colour dimming pulls toward. Not black: scaling straight to black on
	// 5/6/5-bit data crushes the channels to zero and the hue with them, so the
	// wallpaper goes grey. Pulling toward the UI's own dark blue keeps the colour
	// readable at the same brightness.
	static constexpr VMUINT16 kDimTint = VM_COLOR_888_TO_565(9, 12, 22);

	// Which softkey the pen is holding down: 0 none, 1 Options, 2 Choose, 3 Back.
	int softkey_press = 0;

	// Accumulated vertical drag while paging the grid, in pixels. Held across the
	// gesture so a drag past the halfway point of a page flips exactly one page.
	int grid_drag = 0;

	// Vertical band blend_at() refuses to write outside. 0 means "the whole
	// layer". The grid sets it to the tile area while it paints: a row scrolled
	// half under the title bar otherwise shows up as a sliced icon behind the
	// text, which reads as a rendering fault rather than as content going under
	// a fixed header.
	int blit_clip_top = 0;
	int blit_clip_bottom = 0;

	// Selectable items: every entry plus the trailing switch item.
	int count() { return (int)vxps.size() + 1; }
	bool is_switch_item(int i) { return i == (int)vxps.size(); }

	// Row pitch and visible area, per layout. The scroll code talks only about
	// these so it does not have to care which layout is up.
	int item_h() { return view == View::Grid ? cell : b_h; }

	// Total rows the entries need, and whole pages of them.
	int grid_total_rows() { return (count() + grid_cols - 1) / grid_cols; }
	int grid_pages() {
		return (grid_total_rows() + grid_rows_per_page - 1) / grid_rows_per_page;
	}
	int grid_page_h() { return grid_rows_per_page * cell; }

	int view_h() { return view == View::Grid ? grid_page_h() : h; }

	// Where a page's first row lands. Centred in the space between the bars, so a
	// grid that cannot fill the height evenly has the slack split above and below
	// rather than all piled at the bottom.
	int grid_y0() { return grid_top + (grid_h - grid_page_h()) / 2; }

	int grid_page_of(int i) { return (i / grid_cols) / grid_rows_per_page; }

	float content_h() {
		if (view == View::Grid)
			return (float)grid_pages() * (float)grid_page_h();
		return (float)count() * (float)b_h;
	}

	static const std::u16string switch_to_grid = u"Switch to Grid View";
	static const std::u16string switch_to_list = u"Switch to List View";

	std::u16string item_label(int i) {
		if (is_switch_item(i))
			return view == View::List ? switch_to_grid : switch_to_list;
		if (i >= 0 && i < (int)vxps.size())
			return vxps[i].name;
		return std::u16string();
	}

	// The emulator exposes no settings store to VXP apps, and DragAndDrop's
	// last-app memory is in-process only outside Windows, so the layout choice
	// lives in a one-byte file in the MRE folder. scan() only accepts .vxp
	// entries, so the file never shows up in the list itself.
	static const std::u16string view_pref_path = u"e:\\mre\\mremu_view.cfg";

	void load_view_pref() {
		int f = vm_file_open((VMWSTR)view_pref_path.c_str(), MODE_READ, 1);
		if (f < 0)
			return;
		uint8_t b = 0;
		VMUINT got = 0;
		vm_file_read(f, &b, 1, &got);
		vm_file_close(f);
		if (got == 1 && b == 1)
			view = View::Grid;
	}

	void save_view_pref() {
		int f = vm_file_open((VMWSTR)view_pref_path.c_str(), MODE_WRITE, 1);
		if (f < 0)
			return;
		uint8_t b = view == View::Grid ? 1 : 0;
		VMUINT written = 0;
		vm_file_write(f, &b, 1, &written);
		vm_file_close(f);
	}

	void scan();
	void draw();
	void key_handler(VMINT event, VMINT keycode);
	void pen_handler(VMINT event, VMINT x, VMINT y);
	void timer_cb(VMINT tid);

	// Defined further down but needed here: the metrics size the grid, and the
	// navigation helpers call the details-sheet builders.
	void build_wallpaper(std::vector<uint16_t>& out, int dim);
	void update_details_sheet(const vxp& app);
	void update_dir_details_sheet(const vxp& dir);

	// This is a namespace, not a struct, so a call needs the definition or at
	// least a declaration ahead of it. The placeholder tile draws a mask glyph
	// and sits above it.
	void blend_mask_glyph(int x, int y, int size, int radius,
		const unsigned char* mask, int side, int num, int den, VMUINT16 ink);

	// --- Scrolling ----------------------------------------------------
	//
	// scroll_raw is the *unbounded* offset, so it can go negative (above the
	// first item) or past the end (below the last item). scroll_pos is the
	// value actually used for drawing/hit-testing; sync_scroll() derives it
	// from scroll_raw by applying the rubber-band curve while out of bounds.
	//
	// The list follows the finger exactly and stops where the finger stops:
	// no fling, no inertia, no settle animation. That is deliberate - the old
	// fling ran on a 16ms timer that re-derived the offset and redrew, so it
	// kept moving the list after the arrow keys had already moved the
	// selection. Nothing here runs on a timer now, so keyboard scrolling is
	// the only other thing that can move the view. Touch scrolling is
	// unaffected: the pen handler still tracks the finger 1:1.
	//
	// All of it is written against content_h()/view_h()/item_h() rather than
	// against vxps.size(), h and b_h directly. That is what lets the grid reuse
	// it unchanged: the grid is the same machinery at a different pitch, with
	// the title and softkey bars taken out of the viewport.
	// -------------------------------------------------------------------------

	constexpr float kRubberC = 0.55f;     // Apple WWDC18 rubberband constant

	float scroll_raw = 0.f;

	float max_scroll_f() {
		return std::max(0.f, content_h() - (float)view_h());
	}

	// Apple's rubberbanding curve: b(x, d, c) = (1 - 1/((x*c/d)+1)) * d
	float rubber_band(float overshoot, float dimension) {
		if (overshoot == 0.f || dimension <= 0.f)
			return 0.f;
		float s = overshoot < 0.f ? -1.f : 1.f;
		float x = std::abs(overshoot);
		return s * (1.f - 1.f / ((x * kRubberC / dimension) + 1.f)) * dimension;
	}

	void sync_scroll() {
		float maxs = max_scroll_f();

		if (view == View::Grid) {
			// No rubber band: a grid offset is always a whole page, so there is no
			// partial position to stretch. The pen handler turns a drag into whole
			// page steps instead.
			scroll_pos = (int)std::max(0.f, std::min(scroll_raw, maxs));
			return;
		}

		float vh = (float)view_h();
		if (scroll_raw < 0.f)
			scroll_pos = (int)(-rubber_band(-scroll_raw, vh));
		else if (scroll_raw > maxs)
			scroll_pos = (int)(maxs + rubber_band(scroll_raw - maxs, vh));
		else
			scroll_pos = (int)scroll_raw;
	}

	// Anything that moves scroll_pos directly must move scroll_raw with it: an
	// out-of-range scroll_raw is rubber-banded by sync_scroll() on the next drag,
	// so leaving the two out of step would snap the view on the next touch.
	void set_scroll_pos(int pos) {
		scroll_pos = pos;
		scroll_raw = (float)scroll_pos;
	}

	// Brings the selected item fully into view with the smallest possible movement,
	// clamped to the scrollable range.
	//
	// The item sits at item_h()*m_i in the content, so its position has to be measured
	// relative to the visible window, which currently starts at scroll_pos. Comparing
	// the raw content offset against the window height only worked while scroll_pos was 0;
	// once the list was scrolled, moving the selection near either edge no longer brought
	// the row on screen.
	void scroll_to_selection() {
		float maxs = max_scroll_f();
		if (maxs <= 0.f) {
			set_scroll_pos(0);
			return;
		}

		if (view == View::Grid) {
			// Snapped to the page holding the selection: the grid has no partial
			// offsets, so there is nothing to align to an edge.
			set_scroll_pos(std::min(grid_page_of(m_i) * grid_page_h(), (int)maxs));
			return;
		}

		// item_top is an *absolute* content offset. The original code subtracted
		// scroll_pos first, turning it into a viewport-relative delta, and then
		// fed that delta to set_scroll_pos - so the view jumped to a position
		// measured in screen pixels from wherever it happened to be, instead of
		// lining the selected row up with an edge. That is what made arrow-key
		// scrolling land on the wrong row (or off the end) on long lists.
		float vh = (float)view_h();
		float item_top = (float)item_h() * m_i;
		float item_bottom = item_top + (float)item_h();

		float target = scroll_raw;          // default: leave the view alone
		if (item_bottom > scroll_raw + vh)  // below the fold
			target = item_bottom - vh;
		else if (item_top < scroll_raw)     // above the top
			target = item_top;

		set_scroll_pos((int)std::max(0.f, std::min(target, maxs)));
	}

	// Pulls an out-of-range offset back to the nearest bound, immediately.
	//
	// This used to be a critically damped spring driven by the 16ms animation
	// timer. Snapping is the whole of the bounds correction now: with no fling
	// there is no velocity to carry into the settle, and an animated correction
	// is exactly the thing that used to keep moving the list after the user had
	// already released it or pressed an arrow key.
	void snap_scroll_to_bounds() {
		float maxs = max_scroll_f();
		if (scroll_raw >= 0.f && scroll_raw <= maxs)
			return;
		scroll_raw = std::max(0.f, std::min(scroll_raw, maxs));
		sync_scroll();
	}

	static const char* get_t9_chars(int keycode) {
		switch (keycode) {
		case VM_KEY_NUM1: return "1";
		case VM_KEY_NUM2: return "abc2";
		case VM_KEY_NUM3: return "def3";
		case VM_KEY_NUM4: return "ghi4";
		case VM_KEY_NUM5: return "jkl5";
		case VM_KEY_NUM6: return "mno6";
		case VM_KEY_NUM7: return "pqrs7";
		case VM_KEY_NUM8: return "tuv8";
		case VM_KEY_NUM9: return "wxyz9";
		case VM_KEY_NUM0: return "0 ";
		default: return "";
		}
	}

	static std::string u16_to_u8(const std::u16string& u16) {
		std::string s;
		s.reserve(u16.size());
		for (char16_t c : u16) {
			if (c < 128)
				s.push_back((char)c);
			else
				s.push_back('?');
		}
		return s;
	}

	void trigger_scrollbar() {
		last_scroll_time = vm_get_tick_count();
	}

	void reset_marquee() {
		marquee_offset = 0;
		marquee_state = 0;
		marquee_pause_ticks = 28;
		last_selected = m_i;
	}

	// Screen-derived metrics, for both layouts. Has to run before scan(), which
	// sizes the list icons, and it caches the wallpaper the grid blits.
	void compute_screen_metrics() {
		w = vm_graphic_get_screen_width();
		h = vm_graphic_get_screen_height();

		c_h = vm_graphic_get_character_height();
		b_h = c_h * 2;
		img_wh = b_h - 2;

		title_h = c_h + kBarPadPx;
		softkey_h = c_h + kBarPadPx;
		grid_top = title_h;
		grid_h = std::max(1, h - title_h - softkey_h);

		// Square cells sized from the screen alone, so the grid icons can be
		// decoded without knowing how many entries scan() will find. Capped by the
		// page height too, so four rows always fit between the bars.
		int pitch = std::min(w / grid_cols, grid_h / grid_rows_per_page);
		cell = std::max(kMinCellPx, std::min(pitch, kMaxCellPx));

		// Gutter between tiles, and the tile centred in its cell. Drawing the icon
		// flush to the cell origin instead would crowd it against the column to its
		// left and leave a wider gap on the right.
		//
		// The tile is square and so is the cell, so one gap governs both axes:
		// see kRowGapPct for why the two are still spelled separately.
		int gap_x = std::max(kMinGapPx, cell * kColGapPct / 100);
		int gap_y = std::max(kMinGapPx, cell * kRowGapPct / 100);
		tile = cell - std::max(gap_x, gap_y);
		grid_icon = tile;

		build_wallpaper(wallpaper, kGridDim);
		build_wallpaper(wallpaper_list, kListDim);
	}

	// The grid draws icons at grid_icon and the list at img_wh, but a package
	// ships a single image, so each entry ends up with two canvases. Only the
	// list-sized one is built during scan(): decoding both up front would double
	// the cold-start decode cost for a layout the user may never open.
	void ensure_big_icon(vxp& v) {
		if (v.img_big || v.icon_data.empty() || grid_icon < 1)
			return;

		// Decoded at the overdraw size, not at grid_icon, so the loader resamples
		// the source once at the resolution it is actually drawn at. Decoding at
		// grid_icon and stretching afterwards would resample the stretched result.
		int px = grid_icon * kIconOverdrawNum / kIconOverdrawDen;
		v.img_big = vm_graphic_load_image_resized_FIX(
			v.icon_data.data(), (VMINT)v.icon_data.size(), px, px);
	}

	void entry() {
		details_sheet.hide();
		confirm_sheet.hide();
		m_i = 0;
		set_scroll_pos(0);
		touched = false;
		softkey_press = 0;
		reset_marquee();
		trigger_scrollbar();

		// Restored before the metrics so the wallpaper and tile sizes are the ones
		// the saved layout will actually draw with.
		load_view_pref();
		compute_screen_metrics();

		layer_h = vm_graphic_create_layer(0, 0, w, h, -1);
		layer_buf = vm_graphic_get_layer_buffer(layer_h);

		scan();
		draw();

		vm_reg_keyboard_callback(key_handler);
		vm_reg_pen_callback(pen_handler);

		if (marquee_timer_id)
			vm_delete_timer(marquee_timer_id);
		marquee_timer_id = vm_create_timer(35, timer_cb);
	}

	void scan() {
		for (auto& v : vxps) {
			if (v.img)
				vm_graphic_release_canvas_FIX(v.img);
			if (v.img_big)
				vm_graphic_release_canvas_FIX(v.img_big);
		}
		vxps.clear();

		std::vector<vxp> dir_entries;
		std::vector<vxp> vxp_entries;

		// Add ".." entry if not at root folder
		if (current_dir != u"e:\\mre\\" && current_dir != u"e:\\mre") {
			std::u16string parent_dir = current_dir;
			if (!parent_dir.empty() && parent_dir.back() == u'\\')
				parent_dir.pop_back();
			size_t last_slash = parent_dir.find_last_of(u"\\/");
			if (last_slash != std::u16string::npos)
				parent_dir = parent_dir.substr(0, last_slash + 1);
			else
				parent_dir = u"e:\\mre\\";

			vxp up_dir;
			up_dir.name = u".. (Back)";
			up_dir.path = parent_dir;
			up_dir.is_dir = true;
			dir_entries.push_back(up_dir);
		}

		vm_fileinfo_ext direntry;
		std::u16string pattern = current_dir;
		if (pattern.empty() || pattern.back() != u'\\')
			pattern += u'\\';
		pattern += u"*";

		int find_h = vm_find_first_ext((VMWSTR)pattern.c_str(), &direntry);
		for (int ret = 0; find_h >= 0 && !ret;
			ret = vm_find_next_ext(find_h, &direntry))
		{
			std::u16string name = (char16_t*)direntry.filefullname;
			if (name == u"." || name == u"..")
				continue;
			std::u16string item_path = current_dir;
			if (item_path.empty() || item_path.back() != u'\\')
				item_path += u'\\';
			item_path += name;

			bool is_folder = (direntry.attributes & VM_FS_ATTR_DIR) != 0;

			if (is_folder) {
				vxp d;
				d.name = name;
				d.path = item_path + u"\\";
				d.is_dir = true;
				dir_entries.push_back(d);
			}
			else {
				// Check for .vxp extension (case-insensitive)
				if (name.size() >= 4) {
					std::u16string ext = name.substr(name.size() - 4);
					for (auto& c : ext) c = (char16_t)std::tolower((char)c);
					if (ext != u".vxp")
						continue;
				}
				else {
					continue;
				}

				VMINT_CANVAS img = 0;
				size_t file_size = 0;
				std::vector<uint8_t> icon_bytes;
				std::u16string app_name;
				std::u16string dev_name;

				int f = vm_file_open((VMWSTR)item_path.c_str(), MODE_READ, 1);
				if (f >= 0) {
					VMUINT size = 0, rsize = 0;
					vm_file_getfilesize(f, &size);
					file_size = size;

					std::vector<uint8_t> data(size);
					vm_file_read(f, data.data(), size, &rsize);

					vm_file_close(f);

					// Icon discovery. The previous inline search trusted the VRE variant tag and
					// its declared size, which only worked for packages whose payload happened
					// to start with an image: the packages tagged JPG need an 8-byte skip that
					// was keyed off the wrong tag, so they silently produced no icon.
					// See MREngine::VxpIcon.
					//
					// The bytes are kept rather than just the decoded canvas: the grid needs
					// the same picture again at a much larger size, and re-running the
					// extractor on every layout switch would mean re-reading the package.
					{
						std::vector<uint8_t> icon = MREngine::extract_vxp_icon(data);
						if (!icon.empty()) {
							img = vm_graphic_load_image_resized_FIX(
								icon.data(), (VMINT)icon.size(), img_wh, img_wh);
							icon_bytes = std::move(icon);
						}
					}

					if (data.size() >= 12) {
						uint32_t tags_offset = *(uint32_t*)&data[data.size() - 12];
						if (tags_offset < data.size() - 8) {
							uint32_t cur = tags_offset;
							bool is_ucs2 = false;

							uint32_t scan_cur = cur;
							while (scan_cur + 8 < data.size()) {
								uint32_t tag_id = *(uint32_t*)&data[scan_cur];
								uint32_t tag_len = *(uint32_t*)&data[scan_cur + 4];
								scan_cur += 8;
								if (tag_id == 0 || scan_cur + tag_len > data.size())
									break;
								if (tag_id == VM_CE_INFO_CHARSET && tag_len >= 4) {
									is_ucs2 = (*(uint32_t*)&data[scan_cur] != 0);
								}
								scan_cur += tag_len;
							}

							while (cur + 8 < data.size()) {
								uint32_t tag_id = *(uint32_t*)&data[cur];
								uint32_t tag_len = *(uint32_t*)&data[cur + 4];
								cur += 8;
								if (tag_id == 0 || cur + tag_len > data.size())
									break;
								if (tag_id == VM_CE_INFO_NAME) {
									if (is_ucs2)
										app_name = std::u16string((char16_t*)&data[cur], tag_len / 2);
									else {
										std::string s((char*)&data[cur], tag_len);
										app_name = std::u16string(s.begin(), s.end());
									}
								}
								else if (tag_id == VM_CE_INFO_DEV) {
									if (is_ucs2)
										dev_name = std::u16string((char16_t*)&data[cur], tag_len / 2);
									else {
										std::string s((char*)&data[cur], tag_len);
										dev_name = std::u16string(s.begin(), s.end());
									}
								}
								else if (tag_id == VM_CE_INFO_NAME_LIST && app_name.empty()) {
									if (tag_len > 4) {
										uint32_t str_len = *(uint32_t*)&data[cur + 4];
										if (cur + 8 + str_len <= data.size()) {
											if (is_ucs2)
												app_name = std::u16string((char16_t*)&data[cur + 8], str_len / 2);
											else {
												std::string s((char*)&data[cur + 8], str_len);
												app_name = std::u16string(s.begin(), s.end());
											}
										}
									}
								}
								cur += tag_len;
							}

							auto trim_u16 = [](std::u16string& s) {
								while (!s.empty() && (s.back() == 0 || s.back() == ' ' || s.back() == '\r' || s.back() == '\n'))
									s.pop_back();
							};
							trim_u16(app_name);
							trim_u16(dev_name);
						}
					}
				}

				// Field-by-field rather than aggregate init: vxp has grown canvas and
				// icon-buffer members, and a positional initialiser would silently
				// shift them instead of failing to compile.
				vxp entry_v;
				entry_v.name = name;
				entry_v.path = item_path;
				entry_v.img = img;
				entry_v.icon_data = std::move(icon_bytes);
				entry_v.file_size = file_size;
				entry_v.app_name = app_name;
				entry_v.dev_name = dev_name;
				entry_v.is_dir = false;
				vxp_entries.push_back(std::move(entry_v));
			}
		}

		// Close the enumeration. It used to be left open, which leaked a handle per
		// scan() and - worse - left a live iterator behind: a second enumeration
		// started while this one was still open would stop after the first entry,
		// which is what made the folder details count wrong.
		if (find_h >= 0)
			vm_find_close_ext(find_h);

		auto sort_entries = [](std::vector<vxp>& list, size_t start_idx) {
			std::sort(list.begin() + start_idx, list.end(), [](const vxp& a, const vxp& b) {
				std::u16string na = a.name;
				std::u16string nb = b.name;
				for (auto& c : na) c = (char16_t)std::tolower((char)c);
				for (auto& c : nb) c = (char16_t)std::tolower((char)c);
				return na < nb;
			});
		};

		// Sort directories (preserving ".." at index 0 if present)
		size_t dir_sort_start = (dir_entries.size() > 0 && dir_entries[0].name == u".. (Back)") ? 1 : 0;
		sort_entries(dir_entries, dir_sort_start);
		sort_entries(vxp_entries, 0);

		for (auto& d : dir_entries)
			vxps.push_back(std::move(d));
		for (auto& v : vxp_entries)
			vxps.push_back(std::move(v));

		std::string last_saved = DragAndDrop::get_last_selected_app();
		if (!last_saved.empty()) {
			for (int i = 0; i < (int)vxps.size(); ++i) {
				if (u16_to_u8(vxps[i].name) == last_saved) {
					m_i = i;
					scroll_to_selection();
					break;
				}
			}
		}
	}

	void update_confirm_sheet(const vxp& app);

	// --- Navigation helpers -------------------------------------------
	//
	// None of these call draw(). vm_graphic_flush_layer() paces itself to ~30fps
	// with a sleep, so a helper that drew as well as its caller would cost a
	// second frame of latency on every key press and tap. Callers draw once.
	// -------------------------------------------------------------------------

	// Re-reads [dir] and resets the view. Shared by the ".." row, the folder
	// details sheet's Open action, Back, and opening a folder from the grid.
	void enter_dir(const std::u16string& dir) {
		current_dir = dir;
		m_i = 0;
		set_scroll_pos(0);
		scan();
		reset_marquee();
		trigger_scrollbar();
	}

	// Steps up one folder. Nothing to do at the MRE root, so it reports whether
	// it actually moved.
	bool open_parent_dir() {
		if (current_dir == u"e:\\mre\\" || current_dir == u"e:\\mre")
			return false;
		std::u16string parent_dir = current_dir;
		if (!parent_dir.empty() && parent_dir.back() == u'\\')
			parent_dir.pop_back();
		size_t last_slash = parent_dir.find_last_of(u"\\/");
		parent_dir = last_slash != std::u16string::npos
			? parent_dir.substr(0, last_slash + 1)
			: std::u16string(u"e:\\mre\\");
		enter_dir(parent_dir);
		return true;
	}

	// Opens the details sheet for whatever is selected - the hardware Options
	// softkey, a long tap, or the grid's Options label. The switch row is a
	// control rather than an entry, so it has nothing to describe.
	void open_details_for_selection() {
		if (is_switch_item(m_i) || m_i < 0 || m_i >= (int)vxps.size())
			return;
		if (vxps[m_i].is_dir)
			update_dir_details_sheet(vxps[m_i]);
		else
			update_details_sheet(vxps[m_i]);
		details_sheet.show();
		reset_marquee();
	}

	// Enter a folder, launch an app, or flip the layout. One place, because
	// OK, a tap and the grid's Choose label all mean the same thing.
	void activate(int i) {
		if (i < 0 || i >= count())
			return;

		if (is_switch_item(i)) {
			view = view == View::List ? View::Grid : View::List;
			// The two layouts have different pitches and different viewport
			// heights, so the old offset means nothing in the new one.
			set_scroll_pos(0);
			save_view_pref();
			reset_marquee();
			trigger_scrollbar();
			// The switch item is the last one in either layout, so without this
			// it lands off the bottom of the grid and the switch appears to have
			// lost the selection.
			scroll_to_selection();
			return;
		}

		vxp& v = vxps[i];
		if (v.is_dir) {
			enter_dir(v.path);
			return;
		}
		DragAndDrop::set_last_selected_app(u16_to_u8(v.name));
		vm_start_app((VMWSTR)v.path.c_str(), 0, 0);
	}

	// T9 jump to the next name starting with a character on the pressed key.
	// Works in both layouts because both index their items row-major.
	void jump_by_letter(VMINT keycode) {
		const char* chars = get_t9_chars(keycode);
		int n = count();
		if (!*chars || n <= 0)
			return;
		for (int step = 1; step <= n; ++step) {
			int idx = (m_i + step) % n;
			std::u16string name = item_label(idx);
			if (name.empty())
				continue;
			char first = (char)std::tolower((char)name[0]);
			// first must be non-zero: strchr matches the terminator, so a NUL
			// would "find" itself in every group.
			if (first && std::strchr(chars, first)) {
				m_i = idx;
				break;
			}
		}
	}

	// Which page slot a point lands on, or -1. The grid is addressed by page and
	// slot rather than by a free-running offset, so this is a plain divide.
	int grid_index_at(int px, int py) {
		if (py < grid_y0() || py >= grid_y0() + grid_page_h())
			return -1;
		if (px < 0 || px >= grid_cols * cell)
			return -1;

		int page_h = std::max(1, grid_page_h());
		int page = scroll_pos / page_h;
		int row = (py - grid_y0()) / std::max(1, cell);
		int col = px / std::max(1, cell);
		int i = (page * grid_rows_per_page + row) * grid_cols + col;
		return i < count() ? i : -1;
	}

	// Which item a point lands on, in whichever layout is up, or -1.
	int index_at(int px, int py) {
		if (view == View::Grid)
			return grid_index_at(px, py);
		int i = (py + scroll_pos) / std::max(1, b_h);
		return (i >= 0 && i < count()) ? i : -1;
	}

	// Grid navigation. Left and right walk along the row and wrap into
	// the neighbouring one; up and down move a whole row, and because the grid
	// pages, crossing a page boundary takes the page with it.
	void move_selection_grid(VMINT keycode) {
		int n = count();
		if (n <= 1)
			return;
		int last_row = grid_total_rows() - 1;

		switch (keycode) {
		case VM_KEY_LEFT:
			m_i = (m_i - 1 + n) % n;
			break;
		case VM_KEY_RIGHT:
			m_i = (m_i + 1) % n;
			break;
		case VM_KEY_UP:
			if (m_i - grid_cols >= 0)
				m_i -= grid_cols;
			break;
		case VM_KEY_DOWN:
			if (m_i / grid_cols < last_row)
				m_i = std::min(m_i + grid_cols, n - 1);
			break;
		}
	}

	// NOTE: no contents count here on purpose. It was tried (a second
	// vm_find_first_ext/enumeration while the app lists its own dir) and it
	// silently stops after the first entry - see the known-issues note in
	// README.md. A wrong number is worse than no number.
	// Mirror of update_details_sheet for a folder. There is no Delete here on
	// purpose - removing a folder is not something this does, and offering the
	// button would imply it. Left opens the folder, right closes.
	void update_dir_details_sheet(const vxp& dir) {
		details_sheet.set_title(u"Folder Details");
		details_sheet.clear_lines();
		details_sheet.add_line(u"Folder: " + dir.name);

		// Paths are long and the sheet is narrow, so let them scroll.
		details_sheet.add_line(dir.path, 0xFFFF, true);

		std::u16string open_path = dir.path;

		details_sheet.set_left_action(u"Open", [open_path]() {
			details_sheet.hide();
			enter_dir(open_path);
			draw();
		}, VM_COLOR_888_TO_565(180, 180, 180), false);

		details_sheet.set_right_action(u"Close", []() {
			details_sheet.hide();
			reset_marquee();
			draw();
		}, 0xFFFF, true);

		details_sheet.set_on_dismiss([]() {
			details_sheet.hide();
			reset_marquee();
			draw();
		});
	}

	void update_details_sheet(const vxp& app) {
		details_sheet.set_title(u"App Details");
		details_sheet.clear_lines();
		details_sheet.add_line(u"File: " + app.name);
		if (!app.app_name.empty())
			details_sheet.add_line(u"Name: " + app.app_name);
		if (!app.dev_name.empty())
			details_sheet.add_line(u"Dev: " + app.dev_name);
		std::string sz_str = "Size: " + std::to_string((app.file_size + 1023) / 1024) + " KB";
		details_sheet.add_line(std::u16string(sz_str.begin(), sz_str.end()), 0xFFFF, false);
		std::string res_str = "Screen: " + std::to_string(w) + "x" + std::to_string(h);
		details_sheet.add_line(std::u16string(res_str.begin(), res_str.end()), 0xFFFF, false);

		details_sheet.set_left_action(u"Delete", []() {
			update_confirm_sheet(vxps[m_i]);
			confirm_sheet.show();
			reset_marquee();
			draw();
		}, VM_COLOR_888_TO_565(180, 180, 180), false);

		details_sheet.set_right_action(u"Close", []() {
			details_sheet.hide();
			reset_marquee();
			draw();
		}, 0xFFFF, true);

		details_sheet.set_on_dismiss([]() {
			details_sheet.hide();
			reset_marquee();
			draw();
		});
	}

	void update_confirm_sheet(const vxp& app) {
		confirm_sheet.set_title(u"Warning!");
		confirm_sheet.clear_lines();
		confirm_sheet.add_line(u"Delete this application?", 0xFFFF, false);
		confirm_sheet.add_line(app.name, VM_COLOR_888_TO_565(210, 210, 210), true);
		confirm_sheet.add_line(u"Removal is permanent.", VM_COLOR_888_TO_565(160, 160, 160), false);

		confirm_sheet.set_left_action(u"Delete", []() {
			vm_file_delete((VMWSTR)vxps[m_i].path.c_str());
			confirm_sheet.hide();
			details_sheet.hide();
			rescan();
		}, VM_COLOR_888_TO_565(180, 180, 180), true);

		confirm_sheet.set_right_action(u"Cancel", []() {
			confirm_sheet.hide();
			reset_marquee();
			draw();
		}, 0xFFFF, false);

		confirm_sheet.set_on_dismiss([]() {
			confirm_sheet.hide();
			reset_marquee();
			draw();
		});
	}

	void rescan() {
		if (layer_buf) {
			details_sheet.hide();
			confirm_sheet.hide();
			scan();
			// count(), not vxps.size(): the trailing switch item is a valid
			// selection too, and deleting the last app used to be the one case
			// that left m_i pointing past the end.
			if (m_i < 0 || m_i >= count())
				m_i = count() - 1;
			reset_marquee();
			trigger_scrollbar();
			draw();
		}
	}

	static int g_max_marquee_scroll = 0;

	void draw_marquee_text(int x, int y, int max_w, int line_h, const std::u16string& text, VMUINT16 color) {
		int text_w = vm_graphic_get_string_width((VMWSTR)text.c_str());
		if (text_w > max_w) {
			int overflow = text_w - max_w + 10;
			if (overflow > g_max_marquee_scroll)
				g_max_marquee_scroll = overflow;
			int off = std::min(marquee_offset, overflow);
			int clip_top = std::max(0, y);
			int clip_bottom = std::min(h - 1, y + line_h - 1);
			if (clip_top <= clip_bottom) {
				vm_graphic_set_clip(x, clip_top, x + max_w, clip_bottom);
				vm_graphic_textout(layer_buf, x - off, y, (VMWSTR)text.c_str(), 100, color);
				vm_graphic_reset_clip();
			}
		}
		else {
			vm_graphic_textout(layer_buf, x, y, (VMWSTR)text.c_str(), 100, color);
		}
	}

	// --- Pixel helpers ------------------------------------------------
	//
	// The layer is 16-bit RGB565 with a stride of w (see the scrollbar), so tiles
	// are composited straight into it. vm_graphic_set_pixel would work but
	// re-derives the canvas signature and re-clips on every single call, and a
	// grid frame touches tens of thousands of pixels.
	// -------------------------------------------------------------------------

	static int clamp_ch(int v, int maxv) {
		return v < 0 ? 0 : (v > maxv ? maxv : v);
	}

	// alpha-weighted blend of two 565 colours, 0..255.
	static VMUINT16 blend_565(VMUINT16 dst, VMUINT16 src, int alpha) {
		if (alpha <= 0)
			return dst;
		if (alpha >= 255)
			return src;
		// Through named locals rather than inline expressions: VM_COLOR_888_TO_565
		// expands to an unparenthesised ">> 5", which MSVC reports as C4554 once its
		// arguments are computations instead of literals.
		int r = VM_COLOR_GET_RED(dst) + ((VM_COLOR_GET_RED(src) - VM_COLOR_GET_RED(dst)) * alpha) / 255;
		int g = VM_COLOR_GET_GREEN(dst) + ((VM_COLOR_GET_GREEN(src) - VM_COLOR_GET_GREEN(dst)) * alpha) / 255;
		int b = VM_COLOR_GET_BLUE(dst) + ((VM_COLOR_GET_BLUE(src) - VM_COLOR_GET_BLUE(dst)) * alpha) / 255;
		return VM_COLOR_888_TO_565(r, g, b);
	}

	void blend_at(int px, int py, VMUINT16 color, int alpha) {
		if (px < 0 || px >= w || py < 0 || py >= h)
			return;
		if (blit_clip_top && (py < blit_clip_top || py >= blit_clip_bottom))
			return;
		uint16_t* p = (uint16_t*)layer_buf + (size_t)py * w + px;
		*p = blend_565(*p, color, alpha);
	}

	// --- Tile shape -----------------------------------------------------
	//
	// The tiles are not rounded rectangles. Their corners are continuous
	// curves: the outline leaves the straight edge already turning, so there is no
	// point where a circular arc begins. That is the superellipse
	// |x|^n + |y|^n = 1, which is what a squircle is.
	//
	// kSquircleN and kCornerRadiusPct are with the other grid constants; they
	// are a pair, and a large radius does not compensate for too small an n.
	// -------------------------------------------------------------------------

	// Coverage of a squircle at (px, py) inside a [size]x[size] tile, 0..255.
	//
	// Antialiased from the implicit function rather than by supersampling. A 2x2
	// sample can only produce five distinct coverage values, and at this radius the
	// corner is most of the tile, so the edge came out visibly stepped. Here the
	// curve is |x|^n + |y|^n = 1, whose first-order distance to the boundary is
	// just F/|grad F| - a full 8-bit ramp across the single pixel the edge passes
	// through, and no supersampling at all.
	//
	// Only the corner boxes need it; elsewhere the tile is a plain rectangle and
	// the early-out skips the arithmetic.
	static int tile_coverage(int px, int py, int size, int radius) {
		bool near_x = px < radius || px >= size - radius;
		bool near_y = py < radius || py >= size - radius;
		if (!near_x || !near_y)
			return 255;

		// Corner centres sit [radius] in from each corner, the same place a
		// circular arc's centre would.
		float cx = (float)(px < radius ? radius : size - 1 - radius);
		float cy = (float)(py < radius ? radius : size - 1 - radius);
		float r = (float)radius;

		float ux = std::fabs((float)px - cx) / r;
		float uy = std::fabs((float)py - cy) / r;

		// Expanded for n = 3, which is deliberate: |u|^3 and |u|^2 are two and one
		// multiplications, where the general exponent would need a pow() per axis
		// per pixel across the whole corner region.
		float f = ux * ux * ux + uy * uy * uy - 1.f;          // < 0 inside
		float gx = 3.f * ux * ux / r;
		float gy = 3.f * uy * uy / r;
		float g = std::sqrt(gx * gx + gy * gy);

		// g is zero only at the exact centre of a corner, which is deep inside the
		// tile and never near an edge, so this guard never affects the outline.
		if (g < 1e-6f)
			return f <= 0.f ? 255 : 0;

		// Distance in pixels to the boundary, negative inside. The linearisation
		// error over a single pixel is far below one, so this is enough.
		float cov = 0.5f - f / g;
		if (cov <= 0.f)
			return 0;
		if (cov >= 1.f)
			return 255;
		return (int)(cov * 255.f);
	}

	// Fills a squircle of [color] at [alpha]. Used for the drop shadows, the
	// focus rim, the switch tile's plate and the icon-less placeholder tiles.
	void blend_rounded_rect(int x, int y, int size, int radius, VMUINT16 color, int alpha) {
		for (int py = 0; py < size; ++py) {
			for (int px = 0; px < size; ++px) {
				int cov = tile_coverage(px, py, size, radius);
				if (cov)
					blend_at(x + px, y + py, color, cov * alpha / 255);
			}
		}
	}

	// Decodes the embedded PNG and scales it into [out] at [dim].
	//
	// The source is res/wallpaper/app_selector.png, embedded by WallpaperData.h
	// because Android has no asset directory beside the binary for a VXP native
	// app to read from. It is decoded once at start-up and then never again:
	// draw_grid() and draw_list() blit the prepared buffer, which is what keeps a
	// wallpaper from costing a full-screen resample on every key press, tap and
	// marquee tick.
	//
	// Nearest rather than filtered sampling: the source is flat-shaded with hard
	// triangle edges, and a blur across them reads as a compression artefact
	// rather than as softness. The dim is baked in here so that drawing the
	// wallpaper is a single memcpy.
	void build_wallpaper(std::vector<uint16_t>& out, int dim) {
		out.clear();

		VMINT_CANVAS c = vm_graphic_load_image_FIX(
			(const_cast<unsigned char*>(Wallpaper::kPng)), Wallpaper::kPngSize);
		if (!c)
			return;

		MREngine::canvas_signature* cs = find_canvas_signature(
			vm_graphic_get_canvas_buffer_FIX(c));
		MREngine::canvas_frame_property* cfp = cs ? get_canvas_frame_by_ind(cs, 0) : 0;
		if (!cfp || cfp->width < 1 || cfp->height < 1) {
			vm_graphic_release_canvas_FIX(c);
			return;
		}

		const uint16_t* src = (const uint16_t*)(cfp + 1);
		int sw = cfp->width, sh = cfp->height;

		out.resize((size_t)w * h);
		for (int y = 0; y < h; ++y) {
			const uint16_t* srow = src + (size_t)(y * sh / h) * sw;
			uint16_t* drow = out.data() + (size_t)y * w;
			for (int x = 0; x < w; ++x)
				drow[x] = blend_565(srow[x * sw / w], kDimTint, dim);
		}

		vm_graphic_release_canvas_FIX(c);
	}

	// --- Grid drawing ---------------------------------------------------

	// Rectangle fill that honours the grid's vertical clip. vm_graphic_fill_rect
	// knows nothing about blit_clip, so a glyph drawn with it on a tile that is
	// half scrolled under the title bar would paint into the bar.
	void blend_rect(int x, int y, int rw, int rh, VMUINT16 color, int alpha) {
		for (int py = 0; py < rh; ++py)
			for (int px = 0; px < rw; ++px)
				blend_at(x + px, y + py, color, alpha);
	}

	// Rounded variant, for the folder's tab and body.
	void blend_rect_round(int x, int y, int rw, int rh, int radius,
		VMUINT16 color, int alpha) {
		int side = std::min(rw, rh);
		for (int py = 0; py < rh; ++py) {
			for (int px = 0; px < rw; ++px) {
				// Corner radius is relative to the short side; the long edges stay
				// straight, which is what a folder's body looks like.
				int cov;
				if (side < 2)
					cov = 255;
				else {
					int sx = px, sy = py;
					bool near_x = sx < radius || sx >= rw - radius;
					bool near_y = sy < radius || sy >= rh - radius;
					if (!near_x || !near_y)
						cov = 255;
					else
						cov = tile_coverage(sx, sy, side, std::min(radius, side / 2));
				}
				if (cov)
					blend_at(x + px, y + py, color, cov * alpha / 255);
			}
		}
	}

	// A folder, drawn as an actual folder on a neutral plate.
	//
	// It used to be a squircle of flat tab colour with a second squircle over
	// three quarters of it, which came out as a solid yellow blob with no tab and
	// no body edge - not recognisable as a folder at all. A plate plus a glyph
	// reads at tile size and matches how the real icons sit on their own
	// backgrounds.
	void draw_folder_tile(int x, int y, int size, int radius) {
		VMUINT16 plate = VM_COLOR_888_TO_565(kFolderPlateR, kFolderPlateG, kFolderPlateB);
		VMUINT16 tab_c = VM_COLOR_888_TO_565(kFolderTabR, kFolderTabG, kFolderTabB);
		VMUINT16 body_c = VM_COLOR_888_TO_565(kFolderBodyR, kFolderBodyG, kFolderBodyB);
		VMUINT16 edge_c = VM_COLOR_888_TO_565(kFolderEdgeR, kFolderEdgeG, kFolderEdgeB);

		blend_rounded_rect(x, y, size, radius, plate, 255);

		int gw = size * kFolderGlyphWPct / 100;
		int gh = size * kFolderGlyphHPct / 100;
		int gx = x + (size - gw) / 2;
		int gy = y + (size - gh) / 2 + size / kFolderGlyphYDiv;

		// Tab: the short raised strip at the top left, tucked behind the body.
		blend_rect_round(gx, gy - gh / 3, gw * kFolderTabWPct / 100, gh / 2 + 1,
			std::max(1, radius / 3), tab_c, 255);

		// Body.
		blend_rect_round(gx, gy, gw, gh, std::max(1, radius / 2), body_c, 255);

		int lip = std::max(1, gh / kFolderLipDiv);
		blend_rect(gx, gy + gh - lip, gw, lip, edge_c, kFolderEdgeAlpha);
	}

	// Stand-in for the handful of packages that ship a stub icon with no image
	// in it at all (see docs/vxp_icons.md): a plain plate with a gamepad on it,
	// so the grid has no holes in it.
	//
	// The gamepad is res/icons/source/gamepad.png, as a coverage mask like every
	// other glyph the grid draws itself.
	void draw_placeholder_tile(int x, int y, int size, int radius) {
		VMUINT16 plate = VM_COLOR_888_TO_565(
			kPlaceholderPlateR, kPlaceholderPlateG, kPlaceholderPlateB);
		blend_rounded_rect(x, y, size, radius, plate, 255);

		// Bigger box than the switch tile's glyph gets: this is the tile's only
		// content, so a mark sized to sit alongside artwork looks lost here.
		blend_mask_glyph(x, y, size, radius,
			Icons::kPlaceholderMask, Icons::kPlaceholderSide,
			kPlaceholderGlyphNum, kPlaceholderGlyphDen,
			VM_COLOR_888_TO_565(kPlaceholderInkR, kPlaceholderInkG, kPlaceholderInkB));
	}

	// A coverage-mask glyph (see res/icons) drawn centred in a box [num]/[den] of
	// the tile, in [ink], clipped by the tile's squircle.
	//
	// Downsampling is a box average rather than a point sample: the masks are
	// 64x64 and the tile is about 48, and point sampling turns the glyphs into
	// dashed lines. The per-pixel coverage is multiplied by the tile's own
	// squircle coverage so a glyph is clipped by the tile rather than sitting on
	// top of its corners.
	void blend_mask_glyph(int x, int y, int size, int radius,
		const unsigned char* mask, int side, int num, int den, VMUINT16 ink) {
		const int box = std::max(4, size * num / den);
		const int ox = x + (size - box) / 2;
		const int oy = y + (size - box) / 2;
		if (box < 3)
			return;

		for (int py = 0; py < box; ++py) {
			int sy0 = py * side / box;
			int sy1 = (py + 1) * side / box;
			if (sy1 <= sy0)
				sy1 = sy0 + 1;

			for (int px = 0; px < box; ++px) {
				int sx0 = px * side / box;
				int sx1 = (px + 1) * side / box;
				if (sx1 <= sx0)
					sx1 = sx0 + 1;

				int sum = 0, n = 0;
				for (int sy = sy0; sy < sy1; ++sy) {
					const unsigned char* row = mask + (size_t)sy * side;
					for (int sx = sx0; sx < sx1; ++sx) {
						sum += row[sx];
						++n;
					}
				}

				int alpha = sum / n;
				if (!alpha)
					continue;

				int cov = tile_coverage(ox - x + px, oy - y + py, size, radius);
				if (cov)
					blend_at(ox + px, oy + py, ink, alpha * cov / 255);
			}
		}
	}

	// Glyph on the grid's "Switch to List View" tile: the list icon from
	// res/icons/source/list-ul.png.
	void draw_list_glyph(int x, int y, int size, int radius) {
		blend_mask_glyph(x, y, size, radius,
			Icons::kListMask, Icons::kListSide,
			kGlyphBoxNum, kGlyphBoxDen,
			VM_COLOR_888_TO_565(kInkR, kInkG, kInkB));
	}

	// One grid tile. The icon is composited by hand rather than with
	// vm_graphic_blt because blt has no clip shape, and the squircle corners are
	// most of what makes this read as a set of icons rather than a contact sheet
	// of squares.
	void draw_tile(int index, int x, int y) {
		int size = tile;
		int radius = std::max(kMinRadiusPx, size * kCornerRadiusPct / 100);

		if (index == m_i) {
			blend_rounded_rect(x - kFocusRimPx, y - kFocusRimPx,
				size + 2 * kFocusRimPx, radius + kFocusRimPx,
				accent_blue, kFocusAlpha);
		}

		if (is_switch_item(index)) {
			blend_rounded_rect(x, y, size, radius,
				VM_COLOR_888_TO_565(kSwitchPlateR, kSwitchPlateG, kSwitchPlateB),
				kSwitchPlateAlpha);
			draw_list_glyph(x, y, size, radius);
		}
		else if (vxps[index].is_dir) {
			draw_folder_tile(x, y, size, radius);
		}
		else {
			vxp& v = vxps[index];
			ensure_big_icon(v);

			if (v.img_big) {
				// Read the decoded size rather than assuming the loader honoured
				// the request, so a mismatch cannot walk off the end of the buffer.
				MREngine::canvas_signature* cs = find_canvas_signature(
					vm_graphic_get_canvas_buffer_FIX(v.img_big));
				if (!cs)
					return;
				MREngine::canvas_frame_property* cfp = get_canvas_frame_by_ind(cs, 0);
				if (!cfp || cfp->width < 1 || cfp->height < 1)
					return;
				const uint16_t* src = (const uint16_t*)(cfp + 1);
				int sw = cfp->width, sh = cfp->height;

				// Overdraw: the icon is drawn larger than the tile and centred, so
				// the clip throws away its outermost band.
				//
				// This is for the frames the packages bake in. A VXP icon is its
				// own rounded-square artwork with a pale border drawn around it -
				// Gameloft Live's is a 45x45 GIF with a light frame a couple of
				// pixels in. Drawing it edge to edge showed that frame on all four
				// sides, so every tile looked outlined in grey no matter what the
				// squircle did, and our clip only cut the corners where the frame
				// and the clip happened to overlap.
				//
				// Scaling past the clip is what removes it: the frame ends up
				// outside the tile, and the artwork fills the shape the grid is
				// actually drawing. Nothing else is drawn on top, so the tile's
				// edge is purely the clip.
				const int draw_size = size * kIconOverdrawNum / kIconOverdrawDen;
				const int o = (size - draw_size) / 2;

				for (int py = 0; py < draw_size; ++py) {
					int ly = o + py;
					if (ly < 0 || ly >= size)
						continue;

					const uint16_t* srow = src + (size_t)(py * sh / draw_size) * sw;
					for (int px = 0; px < draw_size; ++px) {
						int lx = o + px;
						if (lx < 0 || lx >= size)
							continue;

						int cov = tile_coverage(lx, ly, size, radius);
						if (cov)
							blend_at(x + lx, y + ly, srow[px * sw / draw_size], cov);
					}
				}
			}
			else {
				draw_placeholder_tile(x, y, size, radius);
			}
		}
	}

	// Selected entry's name along the top. Dimmed band behind it so the text
	// stays readable whatever the wallpaper is doing underneath.
	void draw_title_bar() {
		// A flat dark band rather than a dim over the wallpaper. The real menu
		// has no rule here, but its bar is opaque, and a translucent one lets the
		// wallpaper gradient show through the text.
		VMUINT16 bg = VM_COLOR_888_TO_565(kBarBgR, kBarBgG, kBarBgB);
		for (int y = 0; y < title_h; ++y) {
			uint16_t* row = (uint16_t*)layer_buf + (size_t)y * w;
			for (int x = 0; x < w; ++x)
				row[x] = bg;
		}

		std::u16string label = item_label(m_i);
		if (label.empty())
			label = current_dir;
		draw_marquee_text(kBarMarginPx, (title_h - c_h) / 2, w - 2 * kBarMarginPx,
			title_h, label, accent_blue);
	}

	// The centre softkey is set larger than the outer two.
	//
	// There is no way to scale it properly: vm_graphic_set_font is a no-op and
	// vm_font_set_font_style returns success without doing anything, so the only
	// weight and size available is drawing the 15px bitmap glyphs again at small
	// offsets. A 2x2 dilation thickens the strokes in both axes, which reads as a
	// larger label rather than merely a fatter one; the softkey bar is only ~22px
	// tall, so a real 2x scale would not fit.
	void draw_softkey_label(bool pressed, bool big, int x, int y, const std::u16string& text) {
		VMUINT16 fg = pressed ? VM_COLOR_888_TO_565(kPressInkR, kPressInkG, kPressInkB) : 0xFFFF;

		if (pressed) {
			int tw = vm_graphic_get_string_width((VMWSTR)text.c_str()) + (big ? 1 : 0);
			VMUINT16 block = VM_COLOR_888_TO_565(kPressR, kPressG, kPressB);
			vm_graphic_fill_rect(layer_buf, x - kPressPadXPx, y - kPressPadYPx,
				tw + 2 * kPressPadXPx, c_h + kPressPadYPx, block, block);
		}

		vm_graphic_textout(layer_buf, x, y, (VMWSTR)text.c_str(), 100, fg);
		if (big) {
			vm_graphic_textout(layer_buf, x + 1, y, (VMWSTR)text.c_str(), 100, fg);
			vm_graphic_textout(layer_buf, x, y + 1, (VMWSTR)text.c_str(), 100, fg);
			vm_graphic_textout(layer_buf, x + 1, y + 1, (VMWSTR)text.c_str(), 100, fg);
		}
	}

	void draw_softkeys() {
		int y0 = h - softkey_h;
		VMUINT16 bg = VM_COLOR_888_TO_565(kBarBgR, kBarBgG, kBarBgB);
		for (int y = y0; y < h; ++y) {
			uint16_t* row = (uint16_t*)layer_buf + (size_t)y * w;
			for (int x = 0; x < w; ++x)
				row[x] = bg;
		}

		static const std::u16string left = u"Options";
		static const std::u16string mid = u"Choose";
		static const std::u16string right = u"Back";

		// The dilation adds a pixel right and a pixel down, so the centre label is
		// measured as one pixel wider and taller or it would sit half a pixel left
		// of true centre.
		int ty = y0 + (softkey_h - c_h) / 2 - (softkey_press == 2 ? 1 : 0);
		int lw = vm_graphic_get_string_width((VMWSTR)left.c_str());
		int mw = vm_graphic_get_string_width((VMWSTR)mid.c_str()) + 1;
		int rw = vm_graphic_get_string_width((VMWSTR)right.c_str());

		draw_softkey_label(softkey_press == 1, false, kBarMarginPx, ty, left);
		draw_softkey_label(softkey_press == 2, true, (w - mw) / 2, ty, mid);
		draw_softkey_label(softkey_press == 3, false, w - rw - kBarMarginPx, ty, right);
	}

	void draw_grid() {
		if (wallpaper.size() == (size_t)w * h)
			memcpy(layer_buf, wallpaper.data(), (size_t)w * h * 2);
		else
			memset(layer_buf, 0, (size_t)w * h * 2);

		// Tiles only paint inside the grid band. Without this a row scrolled
		// halfway under the title bar is drawn, then dimmed by it, and the icon
		// ends up sliced across the top of the screen.
		blit_clip_top = grid_top;
		blit_clip_bottom = grid_top + grid_h;

		// Exactly the page's worth of slots: four rows of four. Iterating rows
		// from a page index rather than subtracting a free-running offset is what
		// makes the page boundary structural, instead of something the drawing
		// has to be careful never to land between.
		int page_h = std::max(1, grid_page_h());
		int page = scroll_pos / page_h;
		int inset = (cell - tile) / 2;
		int ty = grid_y0() + inset;

		for (int r = 0; r < grid_rows_per_page; ++r) {
			for (int c = 0; c < grid_cols; ++c) {
				int i = (page * grid_rows_per_page + r) * grid_cols + c;
				if (i >= count())
					break;
				// The tile is centred in its cell on both axes. Drawing it at the
				// cell origin would put a full gutter on its right and none on
				// its left, so the row would look shifted.
				draw_tile(i, c * cell + inset, ty);
			}
			ty += cell;
		}

		blit_clip_top = 0;
		blit_clip_bottom = 0;

		draw_title_bar();
		draw_softkeys();
	}

	void draw_list() {
		// Same wallpaper as the grid, pushed further back. The rows keep their
		// white rules and their opaque grey selection fill, so the text stays
		// legible; what changes is that the list is no longer black behind them.
		if (wallpaper_list.size() == (size_t)w * h)
			memcpy(layer_buf, wallpaper_list.data(), (size_t)w * h * 2);
		else
			vm_graphic_fill_rect(layer_buf, 0, 0, w, h, 0x0000, 0x0000);

		int n = count();
		for (int i = 0; i < n; ++i) {
			int y = b_h * i - scroll_pos;
			if (y + b_h < 0 || y >= h)
				continue;

			if (i == m_i)
				vm_graphic_fill_rect(layer_buf, 0, y, w, b_h, gray, gray);

			if (is_switch_item(i)) {
				// Centred and without an icon: this row is a control, not an
				// entry, and it should not read as another app.
				std::u16string label = item_label(i);
				int tw = vm_graphic_get_string_width((VMWSTR)label.c_str());
				vm_graphic_textout(layer_buf, (w - tw) / 2, y + (b_h - c_h) / 2,
					(VMWSTR)label.c_str(), 100, 0xFFFF);
			}
			else {
				if (vxps[i].is_dir) {
					// Render folder icon
					int fx = 3;
					int fy = y + (b_h - img_wh) / 2;
					int fw = img_wh - 2;
					int fh = img_wh - 6;
					if (fw > 6 && fh > 6) {
						VMUINT16 tab_color = VM_COLOR_888_TO_565(240, 195, 60);
						VMUINT16 body_color = VM_COLOR_888_TO_565(255, 210, 80);
						VMUINT16 line_color = VM_COLOR_888_TO_565(200, 150, 30);

						// Tab: top-left small rect
						int tab_w = fw / 3 + 2;
						int tab_h = 4;
						vm_graphic_fill_rect(layer_buf, fx + 1, fy + 2, tab_w, tab_h, tab_color, tab_color);

						// Folder body
						vm_graphic_fill_rect(layer_buf, fx + 1, fy + 4, fw, fh - 2, body_color, body_color);

						// Outline / accents
						vm_graphic_line(layer_buf, fx, fy + 4, fx, fy + 2 + fh, line_color);
						vm_graphic_line(layer_buf, fx, fy + 2 + fh, fx + fw, fy + 2 + fh, line_color);
						vm_graphic_line(layer_buf, fx + fw, fy + 4, fx + fw, fy + 2 + fh, line_color);
						vm_graphic_line(layer_buf, fx + tab_w, fy + 4, fx + fw, fy + 4, line_color);
						vm_graphic_line(layer_buf, fx, fy + 2, fx + tab_w, fy + 2, line_color);

						// If ".." entry, draw a small up/back arrow inside folder
						if (vxps[i].name.find(u"..") == 0) {
							int cx = fx + fw / 2;
							int cy = fy + 4 + (fh - 2) / 2;
							VMUINT16 arrow_color = VM_COLOR_888_TO_565(30, 30, 30);
							vm_graphic_line(layer_buf, cx, cy - 2, cx, cy + 3, arrow_color);
							vm_graphic_line(layer_buf, cx - 2, cy, cx, cy - 2, arrow_color);
							vm_graphic_line(layer_buf, cx + 2, cy, cx, cy - 2, arrow_color);
						}
					}
				}
				else if (vxps[i].img) {
					vm_graphic_blt(layer_buf, 1, y, (VMBYTE*)vxps[i].img, 0, 0, img_wh, img_wh, 1);
				}

				int text_x = 2 + b_h;
				int text_y = y + (b_h - c_h) / 2;
				int max_text_w = w - text_x - 4;

				if (i == m_i && !details_sheet.is_open() && !confirm_sheet.is_open())
					draw_marquee_text(text_x, text_y, max_text_w, b_h, vxps[i].name, 0xFFFF);
				else
					vm_graphic_textout(layer_buf, text_x, text_y, (VMWSTR)vxps[i].name.c_str(), 100, 0xFFFF);
			}

			vm_graphic_line(layer_buf, 0, y + b_h - 1, w, y + b_h - 1, 0xFFFF);
		}
	}

	// Thumb over the visible area of whichever layout is up.
	void draw_scrollbar() {
		int total = (int)content_h();
		int vh = view_h();
		if (total <= vh || vm_get_tick_count() - last_scroll_time >= 2000) {
			scrollbar_visible = false;
			return;
		}
		scrollbar_visible = true;

		int top = view == View::Grid ? grid_y0() : 0;
		int max_scroll = total - vh;
		int thumb_h = (vh * vh) / total;
		if (thumb_h < 16) thumb_h = 16;
		if (thumb_h > vh) thumb_h = vh;
		int thumb_y = top + ((max_scroll > 0) ? (scroll_pos * (vh - thumb_h)) / max_scroll : 0);
		if (thumb_y < top) thumb_y = top;
		if (thumb_y + thumb_h > top + vh) thumb_y = top + vh - thumb_h;

		VMUINT16 sb_col = VM_COLOR_888_TO_565(190, 190, 190);
		int sb_w = 3;
		for (int sy = thumb_y; sy < thumb_y + thumb_h; ++sy) {
			if (sy < 0 || sy >= h)
				continue;
			for (int sx = w - sb_w; sx < w; ++sx) {
				VMUINT16* ptr = (VMUINT16*)layer_buf + (size_t)sy * w + sx;
				*ptr = ((*ptr & 0xF7DE) >> 1) + ((sb_col & 0xF7DE) >> 1);
			}
		}
	}

	void draw() {
		g_max_marquee_scroll = 0;

		if (view == View::Grid)
			draw_grid();
		else
			draw_list();

		draw_scrollbar();

		if (vxps.empty()) {
			int y0 = (view == View::Grid ? grid_y0() : 0) + 2;
			vm_graphic_textout(layer_buf, 0, y0, vm_ucs2_string((VMSTR)"No files in MRE folder."), 100, 0xFFFF);
			vm_graphic_textout(layer_buf, 0, y0 + c_h + 2, vm_ucs2_string((VMSTR)"Put them in fs/e/mre."), 100, 0xFFFF);
			vm_graphic_textout(layer_buf, 0, y0 + c_h * 2 + 2, vm_ucs2_string((VMSTR)"Or drag them here to import..."), 100, 0xFFFF);
		}

		// Render modal bottom sheets. They dim and cover the whole lower screen,
		// so the grid's softkey bar is hidden behind them while one is open.
		if (details_sheet.is_open())
			details_sheet.draw(layer_buf, w, h, c_h, marquee_offset, g_max_marquee_scroll);

		if (confirm_sheet.is_open()) {
			g_max_marquee_scroll = 0;
			confirm_sheet.draw(layer_buf, w, h, c_h, marquee_offset, g_max_marquee_scroll);
		}

		vm_graphic_flush_layer(&layer_h, 1);
	}

	void timer_cb(VMINT tid) {
		if (!layer_buf || vxps.empty() || touched)
			return;

		if (g_appManager) {
			App* act = g_appManager->get_active_app();
			if (act && act->system_callbacks.ph_app_id != vm_pmng_get_current_handle())
				return;
		}

		if (scrollbar_visible && vm_get_tick_count() - last_scroll_time >= 2000) {
			scrollbar_visible = false;
			draw();
			return;
		}

		if (g_max_marquee_scroll <= 0) {
			if (marquee_offset != 0) {
				marquee_offset = 0;
				draw();
			}
			return;
		}

		if (marquee_state == 0) {
			if (--marquee_pause_ticks <= 0)
				marquee_state = 1;
		}
		else if (marquee_state == 1) {
			marquee_offset++;
			if (marquee_offset >= g_max_marquee_scroll) {
				marquee_offset = g_max_marquee_scroll;
				marquee_state = 2;
				marquee_pause_ticks = 34;
			}
			draw();
		}
		else if (marquee_state == 2) {
			if (--marquee_pause_ticks <= 0) {
				marquee_offset = 0;
				marquee_state = 0;
				marquee_pause_ticks = 28;
				draw();
			}
		}
	}

	void key_handler(VMINT event, VMINT keycode) {
		if (confirm_sheet.is_open()) {
			confirm_sheet.handle_key(event, keycode);
			draw();
			return;
		}

		if (details_sheet.is_open()) {
			details_sheet.handle_key(event, keycode);
			draw();
			return;
		}

		int old_m_i = m_i;
		if (event == VM_KEY_EVENT_UP) {
			trigger_scrollbar();
			switch (keycode) {
			case VM_KEY_UP:
			case VM_KEY_DOWN:
			case VM_KEY_LEFT:
			case VM_KEY_RIGHT:
				if (view == View::Grid) {
					move_selection_grid(keycode);
				}
				else if (keycode == VM_KEY_UP) {
					if (--m_i < 0)
						m_i = count() - 1;
				}
				else if (keycode == VM_KEY_DOWN) {
					if (++m_i >= count())
						m_i = 0;
				}
				// Left and right do nothing in the list, which is the behaviour
				// this menu always had.
				break;
			case VM_KEY_LEFT_SOFTKEY:
				// Reads as "Options" on the grid's softkey bar.
				open_details_for_selection();
				draw();
				return;
			case VM_KEY_RIGHT_SOFTKEY:
				// Reads as "Back" on the grid's softkey bar.
				open_parent_dir();
				draw();
				return;
			case VM_KEY_OK:
				// Reads as "Choose" on the grid's softkey bar.
				activate(m_i);
				draw();
				return;
			case VM_KEY_NUM1:
			case VM_KEY_NUM2:
			case VM_KEY_NUM3:
			case VM_KEY_NUM4:
			case VM_KEY_NUM5:
			case VM_KEY_NUM6:
			case VM_KEY_NUM7:
			case VM_KEY_NUM8:
			case VM_KEY_NUM9:
			case VM_KEY_NUM0:
				jump_by_letter(keycode);
				break;
			}
		}

		if (m_i != old_m_i) {
			reset_marquee();
			// The switch item is not an app, so there is nothing to remember
			// about it as the last selection.
			if (m_i >= 0 && m_i < (int)vxps.size())
				DragAndDrop::set_last_selected_app(u16_to_u8(vxps[m_i].name));
		}

		// Keep the newly selected row on screen. Done on every key event so T9 jumps
		// and wrap-around land on a visible item too.
		scroll_to_selection();

		draw();
	}

	// Which softkey label a point lands on: 1 Options, 2 Choose, 3 Back, 0 for
	// anywhere above the bar.
	int softkey_zone(int px, int py) {
		if (py < h - softkey_h)
			return 0;
		if (px < w / 3) return 1;
		if (px < 2 * w / 3) return 2;
		return 3;
	}

	// Grid softkey bar. Handles press, slide between labels, and release; returns
	// true when the event belonged to the bar and has been consumed.
	bool handle_softkey_pen(VMINT event, int x, int y) {
		int zone = softkey_zone(x, y);

		switch (event) {
		case VM_PEN_EVENT_TAP:
			softkey_press = zone;
			break;
		case VM_PEN_EVENT_MOVE:
		case VM_PEN_EVENT_REPEAT:
		case VM_PEN_EVENT_DOUBLE_CLICK:
			// Sliding sideways moves the highlight, the way a real menu does.
			softkey_press = zone;
			break;
		case VM_PEN_EVENT_RELEASE: {
			int fired = softkey_press;
			softkey_press = 0;
			// Only fire if the finger is still on the label it went down on, so
			// dragging off before lifting cancels.
			if (zone == fired) {
				if (fired == 1)
					open_details_for_selection();
				else if (fired == 2)
					activate(m_i);
				else if (fired == 3)
					open_parent_dir();
			}
			break;
		}
		case VM_PEN_EVENT_ABORT:
			softkey_press = 0;
			break;
		}

		draw();
		return true;
	}

	void pen_handler(VMINT event, VMINT x, VMINT y) {
		if (confirm_sheet.is_open()) {
			confirm_sheet.handle_pen(event, x, y, w, h, c_h);
			draw();
			return;
		}

		if (details_sheet.is_open()) {
			details_sheet.handle_pen(event, x, y, w, h, c_h);
			draw();
			return;
		}

		// The grid's softkey bar is checked before any of the drag bookkeeping,
		// so tapping Options cannot be mistaken for a flick. The bar also keeps
		// ownership once pressed, otherwise sliding off it mid-gesture would drop
		// the pen into the scroll path half way through.
		if (view == View::Grid && (softkey_press || softkey_zone(x, y) != 0)) {
			handle_softkey_pen(event, x, y);
			return;
		}

		int old_m_i = m_i;
		trigger_scrollbar();
		switch (event) {
			case VM_PEN_EVENT_TAP:
				// Adopt the on-screen offset as the new raw one, but only when
				// in bounds - copying a rubber-banded display offset back into
				// scroll_raw would throw away the stretch the user is still holding.
				if (scroll_raw >= 0.f && scroll_raw <= max_scroll_f())
					scroll_raw = (float)scroll_pos;
				touch_start_y = y;
				touch_last_y = y;
				touched = true;
				grid_drag = 0;
				touch_time = vm_get_tick_count();
				break;
			case VM_PEN_EVENT_MOVE:
			case VM_PEN_EVENT_REPEAT:
			case VM_PEN_EVENT_DOUBLE_CLICK:
				if (touched) {
					int delta = touch_last_y - y;
					touch_last_y = y;
					if (view == View::Grid) {
						// A grid has no partial offsets, so a drag is turned into
						// whole pages: the view moves once the finger has covered
						// half a page, and not at all before that.
						int page_h = std::max(1, grid_page_h());
						int maxs = (int)max_scroll_f();
						grid_drag += delta;
						while (grid_drag >= page_h / 2 && scroll_pos < maxs) {
							grid_drag -= page_h;
							set_scroll_pos(std::min(scroll_pos + page_h, maxs));
						}
						while (grid_drag <= -page_h / 2 && scroll_pos > 0) {
							grid_drag += page_h;
							set_scroll_pos(std::max(scroll_pos - page_h, 0));
						}
					}
					else {
						// Unbounded here on purpose - the rubber band is applied in
						// sync_scroll() so the list resists instead of hard-stopping.
						scroll_raw += delta;
						sync_scroll();
					}
				}
				break;
			case VM_PEN_EVENT_LONG_TAP: {
				int item = index_at(x, y);
				// The switch row is a control, so there is nothing to describe.
				if (item >= 0 && !is_switch_item(item)) {
					m_i = item;
					open_details_for_selection();
					// Only a real app is worth remembering as the last selection.
					if (!vxps[m_i].is_dir)
						DragAndDrop::set_last_selected_app(u16_to_u8(vxps[m_i].name));
					draw();
				}
				break;
			}
			case VM_PEN_EVENT_RELEASE:
			case VM_PEN_EVENT_ABORT: {
				touched = false;

				// The list stops where the finger stopped. If that leaves it out of
				// range, snap back now rather than on a timer, so an overscrolled
				// list can never be left stranded. Done before the tap hit test so
				// that a tap released past the edge still resolves against the
				// corrected offset.
				snap_scroll_to_bounds();

				// A tap is a short, near-stationary press; anything else is a scroll.
				// The threshold scales with the item so it means the same thing in
				// both layouts - a tile is much taller than a row.
				if (std::abs(touch_start_y - y) < item_h() / 2 && vm_get_tick_count() - touch_time < 250) {
					int tapped = index_at(x, touch_start_y);
					if (tapped >= 0) {
						m_i = tapped;
						// Covers opening a folder, launching an app, and flipping
						// the layout when the switch item is the one tapped.
						activate(tapped);
						// activate() deliberately does not draw, and nothing else
						// will: the marquee timer only redraws while a long name is
						// scrolling, so skipping this left a layout switch invisible
						// until the next key press.
						draw();
						return;
					}
					break;
				}
				break;
			}
		}

		if (m_i < 0)
			m_i = count() - 1;

		if (m_i >= count())
			m_i = 0;

		if (m_i != old_m_i) {
			reset_marquee();
			if (m_i >= 0 && m_i < (int)vxps.size())
				DragAndDrop::set_last_selected_app(u16_to_u8(vxps[m_i].name));
		}

		// Keep the selected row on screen after a tap, the same way the keyboard
		// path does. Without this a tap on a partially visible row left the
		// view where it was and the highlight scrolled out of sight.
		scroll_to_selection();

		draw();
	}
}