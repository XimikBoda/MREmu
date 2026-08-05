// GIF.H
// HEADER-ONLY GIF DECODER
//
// DESIGN
// - *one* decoder core
// - output format selectable by macro
// - renderer callbacks optional
// - canvas compositing is persistent between frames
// - disposal 3 supported only if user provides backup buffer
// - *no allocations* ... caller provides scratch + optional backup
//
// COMPAT
// - provides helper functions compatible with older API style
// - keeps the newer API as the core (no behavior hidden)
//
// NOTE
// - this file intentionally avoids libc headers
// - mini runtime provides memset/memcpy/memcmp only

#if !defined(GIF_H) || defined(GIF_USE_PREFIX)
#define GIF_H

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------
// CONFIG
// -----------------------------

// output formats
#define GIF_OUTPUT_RGB888   1
#define GIF_OUTPUT_RGB565LE 2
#define GIF_OUTPUT_RGBA8888 3

#ifndef GIF_OUTPUT_FORMAT
  #define GIF_OUTPUT_FORMAT GIF_OUTPUT_RGB888
#endif

#if (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGB888)
  #define GIF_OUTPUT_BPP 3u
#elif (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGB565LE)
  #define GIF_OUTPUT_BPP 2u
#elif (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGBA8888)
  #define GIF_OUTPUT_BPP 4u
#else
  #error Unsupported GIF_OUTPUT_FORMAT
#endif

// turbo writer
// - spans reduce per-pixel branching
#ifndef GIF_TURBO_BLIT
  #define GIF_TURBO_BLIT 0
#endif

// turbo map2 for rgb565
// - 256 KiB lookup table
#ifndef GIF_TURBO_MAP2
  #define GIF_TURBO_MAP2 0
#endif

// minimal guards
#ifndef GIF_MINIMAL_GUARDS
  #define GIF_MINIMAL_GUARDS 1
#endif

// unsafe toggles
#ifndef GIF_NO_CLAMP_INDEX
  #define GIF_NO_CLAMP_INDEX 0
#endif

#ifndef GIF_NO_DIM_CHECKS
  #define GIF_NO_DIM_CHECKS 0
#endif

// limits
#ifndef GIF_MAX_WIDTH
  #define GIF_MAX_WIDTH 480
#endif

#ifndef GIF_MAX_COLORS
  #define GIF_MAX_COLORS 256
#endif

#ifndef GIF_MAX_CODE_SIZE
  #define GIF_MAX_CODE_SIZE 12
#endif

// LZW backend selector
#define GIF_LZW_SAFE         1
#define GIF_LZW_TURBO_UNSAFE 2

#ifndef GIF_LZW_BACKEND
  // *best default* ... safe stream decoder
  #define GIF_LZW_BACKEND GIF_LZW_SAFE
#endif

// legacy macro mapping
// - old code used GIF_MODE_TURBO to select a faster decoder
// - now it selects the unsafe backend, but *risk gate* still applies
#if defined(GIF_MODE_TURBO) && !defined(GIF_LZW_BACKEND)
  #define GIF_LZW_BACKEND GIF_LZW_TURBO_UNSAFE
#endif

// micro-cache
// - *90s trick* ... remember expanded codes
// - avoids repeated prefix-walk + stack churn
#ifndef GIF_LZW_MICROCACHE
  #define GIF_LZW_MICROCACHE 1
#endif

// slots must be power of two
#ifndef GIF_LZW_MICROCACHE_SLOTS
  #define GIF_LZW_MICROCACHE_SLOTS 128
#endif

// max cached span length
#ifndef GIF_LZW_MICROCACHE_MAXLEN
  #define GIF_LZW_MICROCACHE_MAXLEN 64
#endif

// arena for cached spans
#ifndef GIF_LZW_MICROCACHE_ARENA_SIZE
  #define GIF_LZW_MICROCACHE_ARENA_SIZE 16384
#endif

// risk gate for unsafe backend
#ifndef GIF_TURBO_RISK_ACCEPTED
  #define GIF_TURBO_RISK_ACCEPTED 0
#endif

// -----------------------------
// TYPES
// -----------------------------

#ifndef GIF_BASE_TYPES_DEFINED
#define GIF_BASE_TYPES_DEFINED
#if defined(__SIZE_TYPE__)
typedef __SIZE_TYPE__ gif_usize;
#else
typedef unsigned long gif_usize;
#endif

#if defined(__UINTPTR_TYPE__)
typedef __UINTPTR_TYPE__ gif_uptr;
#else
typedef unsigned long gif_uptr;
#endif

typedef unsigned char  gif_u8;
typedef unsigned short gif_u16;
typedef unsigned int   gif_u32;
typedef signed short   gif_i16;
#endif

// -----------------------------
// CONSTANTS
// -----------------------------

#define GIF_LZW_TABLE_ENTRIES (1u << GIF_MAX_CODE_SIZE) // 4096
#define GIF_ALIGN_SLOP        32u

#define GIF_TRAILER   0x3Bu
#define GIF_EXT_INTRO 0x21u
#define GIF_IMAGE_SEP 0x2Cu

#define GIF_EXT_GCE   0xF9u
#define GIF_EXT_APP   0xFFu

#define GIF_GCE_BLOCK_SIZE 0x04u

// -----------------------------
// SCRATCH SIZE
// -----------------------------

#define GIF_SCRATCH_PREFIX_SIZE ((gif_u32)GIF_LZW_TABLE_ENTRIES * (gif_u32)sizeof(gif_u16))
#define GIF_SCRATCH_SUFFIX_SIZE ((gif_u32)GIF_LZW_TABLE_ENTRIES * (gif_u32)sizeof(gif_u8))
#define GIF_SCRATCH_STACK_SIZE  ((gif_u32)GIF_LZW_TABLE_ENTRIES * (gif_u32)sizeof(gif_u8))
#define GIF_SCRATCH_LINE_SIZE   ((gif_u32)GIF_MAX_WIDTH * (gif_u32)sizeof(gif_u8))

#if (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGB565LE) && (GIF_TURBO_MAP2 != 0)
  #define GIF_SCRATCH_MAP2_SIZE ((gif_u32)65536u * (gif_u32)sizeof(gif_u32))
#else
  #define GIF_SCRATCH_MAP2_SIZE 0u
#endif

#if (GIF_LZW_MICROCACHE != 0)
  #define GIF_SCRATCH_MC_ARENA_SIZE ((gif_u32)GIF_LZW_MICROCACHE_ARENA_SIZE * (gif_u32)sizeof(gif_u8))
  #define GIF_SCRATCH_MC_META_SIZE  ((gif_u32)GIF_LZW_MICROCACHE_SLOTS * (gif_u32)(sizeof(gif_u16) + sizeof(gif_u16) + sizeof(gif_u32) + sizeof(gif_u8)))
#else
  #define GIF_SCRATCH_MC_ARENA_SIZE 0u
  #define GIF_SCRATCH_MC_META_SIZE  0u
#endif

#define GIF_SCRATCH_BUFFER_REQUIRED_SIZE \
  ((gif_usize)GIF_ALIGN_SLOP + \
   (gif_usize)GIF_SCRATCH_PREFIX_SIZE + (gif_usize)GIF_SCRATCH_SUFFIX_SIZE + (gif_usize)GIF_SCRATCH_STACK_SIZE + \
   (gif_usize)GIF_SCRATCH_LINE_SIZE + (gif_usize)GIF_SCRATCH_MAP2_SIZE + \
   (gif_usize)GIF_SCRATCH_MC_ARENA_SIZE + (gif_usize)GIF_SCRATCH_MC_META_SIZE)

// -----------------------------
// ERRORS
// -----------------------------

#ifndef GIF_PUBLIC_SHARED_DEFINED
#define GIF_PUBLIC_SHARED_DEFINED
typedef enum {
  GIF_SUCCESS = 0,
  GIF_ERROR_DECODE = 1,
  GIF_ERROR_INVALID_PARAM = 2,
  GIF_ERROR_BAD_FILE = 3,
  GIF_ERROR_EARLY_EOF = 4,
  GIF_ERROR_NO_FRAME = 5,
  GIF_ERROR_BUFFER_TOO_SMALL = 6,
  GIF_ERROR_INVALID_FRAME_DIMENSIONS = 7,
  GIF_ERROR_UNSUPPORTED_COLOR_DEPTH = 8,
  GIF_ERROR_BUFFER_OVERFLOW = 9,
  GIF_ERROR_INVALID_LZW_CODE = 10,
  GIF_ERROR_UNSUPPORTED_DIMENSIONS = 11
} GIF_Result;

#ifndef GIF_OK
#define GIF_OK GIF_SUCCESS
#endif

typedef void (*GIF_ErrorCallback)(int error_code, const char *message);


// -----------------------------
// RENDERER
// -----------------------------

typedef struct {
  void *user;

  // begin called once per playback
  void (*begin)(void *user, int canvas_w, int canvas_h);

  // patch callback ... indexed row(s)
  void (*blit_indexed)(
      void *user,
      int x, int y, int w, int h,
      const gif_u8 *idx, int idx_stride,
      const gif_u8 *pal_rgb, int pal_colors,
      int transparent_index, int has_transparency);

  // end called after a frame
  void (*end)(void *user, int delay_ms);
} GIF_Renderer;

// frame rectangle (modern helper)
typedef struct {
  int x;
  int y;
  int w;
  int h;
} GIF_FrameRect;

#endif

// -----------------------------
// CONTEXT
// -----------------------------

#if defined(GIF_USE_PREFIX)
// PREFIX MODE
// *важливе* define GIF_PREFIX as a token prefix like SAFE_ or TURBO_
#ifndef GIF_PREFIX
#error GIF_PREFIX must be defined when GIF_USE_PREFIX is set
#endif
#define GIF_CAT2(a,b) a##b
#define GIF_CAT(a,b) GIF_CAT2(a,b)
#define GIF_SYM(x) GIF_CAT(GIF_PREFIX,x)

#define GIF_Context GIF_SYM(GIF_Context)
#define GIF_Pump GIF_SYM(GIF_Pump)

#define gif_align_ptr GIF_SYM(gif_align_ptr)
#define gif_apply_prev_disposal GIF_SYM(gif_apply_prev_disposal)
#define gif_backup_rect_if_needed GIF_SYM(gif_backup_rect_if_needed)
#define gif_blit_map2_565_row GIF_SYM(gif_blit_map2_565_row)
#define gif_blit_plain_row GIF_SYM(gif_blit_plain_row)
#define gif_blit_turbo_row GIF_SYM(gif_blit_turbo_row)
#define gif_build_map2_565 GIF_SYM(gif_build_map2_565)
#define gif_canvas_clear GIF_SYM(gif_canvas_clear)
#define gif_close GIF_SYM(gif_close)
#define gif_close_compat GIF_SYM(gif_close_compat)
#define gif_decode_image_data_safe GIF_SYM(gif_decode_image_data_safe)
#define gif_decode_image_data_unsafe GIF_SYM(gif_decode_image_data_unsafe)
#define gif_discard_sub_blocks GIF_SYM(gif_discard_sub_blocks)
#define gif_emit_row GIF_SYM(gif_emit_row)
#define gif_emit_row_canvas GIF_SYM(gif_emit_row_canvas)
#define gif_emit_row_renderer GIF_SYM(gif_emit_row_renderer)
#define gif_error_strings GIF_SYM(gif_error_strings)
#define gif_fill_rect_bg GIF_SYM(gif_fill_rect_bg)
#define gif_find_next_image GIF_SYM(gif_find_next_image)
#define gif_get_error_string GIF_SYM(gif_get_error_string)
#define gif_get_info GIF_SYM(gif_get_info)
#define gif_get_required_scratch_size GIF_SYM(gif_get_required_scratch_size)
#define gif_init GIF_SYM(gif_init)
#define gif_is_netscape_id GIF_SYM(gif_is_netscape_id)
#define gif_lzw_discard_rest GIF_SYM(gif_lzw_discard_rest)
#define gif_lzw_expand_to_stack_safe GIF_SYM(gif_lzw_expand_to_stack_safe)
#define gif_lzw_expand_to_stack_unsafe GIF_SYM(gif_lzw_expand_to_stack_unsafe)
#define gif_lzw_read_code GIF_SYM(gif_lzw_read_code)
#define gif_lzw_stream_begin GIF_SYM(gif_lzw_stream_begin)
#define gif_lzw_stream_read_byte GIF_SYM(gif_lzw_stream_read_byte)
#define gif_map_interlace_y GIF_SYM(gif_map_interlace_y)
#define gif_mc_insert GIF_SYM(gif_mc_insert)
#define gif_mc_lookup GIF_SYM(gif_mc_lookup)
#define gif_mc_reset GIF_SYM(gif_mc_reset)
#define gif_mc_slot GIF_SYM(gif_mc_slot)
#define gif_memcmp GIF_SYM(gif_memcmp)
#define gif_memcpy GIF_SYM(gif_memcpy)
#define gif_memset GIF_SYM(gif_memset)
#define gif_next_frame GIF_SYM(gif_next_frame)
#define gif_next_frame_compat GIF_SYM(gif_next_frame_compat)
#define gif_next_frame_rect GIF_SYM(gif_next_frame_rect)
#define gif_next_frame_rect_ex GIF_SYM(gif_next_frame_rect_ex)
#define gif_next_frame_render GIF_SYM(gif_next_frame_render)
#define gif_prepare_palette_cache GIF_SYM(gif_prepare_palette_cache)
#define gif_pump_push_span GIF_SYM(gif_pump_push_span)
#define gif_read_app_ext GIF_SYM(gif_read_app_ext)
#define gif_read_bytes GIF_SYM(gif_read_bytes)
#define gif_read_color_table GIF_SYM(gif_read_color_table)
#define gif_read_extension GIF_SYM(gif_read_extension)
#define gif_read_gce GIF_SYM(gif_read_gce)
#define gif_read_image_descriptor GIF_SYM(gif_read_image_descriptor)
#define gif_read_u16_le GIF_SYM(gif_read_u16_le)
#define gif_read_u8 GIF_SYM(gif_read_u8)
#define gif_report GIF_SYM(gif_report)
#define gif_restart_animation GIF_SYM(gif_restart_animation)
#define gif_restore_backup_if_needed GIF_SYM(gif_restore_backup_if_needed)
#define gif_rewind GIF_SYM(gif_rewind)
#define gif_rewind_compat GIF_SYM(gif_rewind_compat)
#define gif_rgb_to_565 GIF_SYM(gif_rgb_to_565)
#define gif_set_disposal3_buffer GIF_SYM(gif_set_disposal3_buffer)
#define gif_set_error_callback GIF_SYM(gif_set_error_callback)
#define gif_skip GIF_SYM(gif_skip)
#define gif_write_rgb_pixel GIF_SYM(gif_write_rgb_pixel)
#endif

// *compat* ... human readable string for rc
const char *gif_get_error_string(int error_code);

typedef struct {
  // input
  const gif_u8 *data;
  gif_usize size;
  gif_usize pos;

  // canvas
  gif_u32 canvas_w;
  gif_u32 canvas_h;

  // palettes
  gif_u8  gpal[GIF_MAX_COLORS * 3u];
  gif_u8  lpal[GIF_MAX_COLORS * 3u];
  gif_u16 gpal_n;
  gif_u16 lpal_n;

  const gif_u8 *apal;
  gif_u16 apal_n;

  // bg
  gif_u8 bg_index;
  gif_u8 bg_rgb[3];

  // frame rect
  gif_u16 fx;
  gif_u16 fy;
  gif_u16 fw;
  gif_u16 fh;

  // frame flags
  gif_u8 packed_img;
  gif_u8 has_lpal;
  gif_u8 lzw_min_code_size;

  gif_u8 has_transparency;
  gif_u8 transparent_index;
  gif_u8 disposal_method;

  gif_u16 delay_ms;

  // looping
  // -1 infinite
  //  0 play once
  // >0 repeats remaining
  gif_i16 loop_count;
  gif_i16 loop_count_init;

  // anim start
  gif_usize anim_start_pos;

  // scratch pointers
  gif_u16 *lzw_prefix;
  gif_u8  *lzw_suffix;
  gif_u8  *lzw_stack;
  gif_u8  *line_idx;

  // palette caches
#if (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGB565LE)
  gif_u16 pal565[GIF_MAX_COLORS];
#endif
#if (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGBA8888)
  gif_u32 pal32[GIF_MAX_COLORS];
#endif

  // map2
#if (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGB565LE) && (GIF_TURBO_MAP2 != 0)
  gif_u32 *map2_565;
  gif_u8   map2_ready;
#endif

  // micro-cache pointers
#if (GIF_LZW_MICROCACHE != 0)
  gif_u8  *mc_arena;
  gif_u32  mc_arena_pos;

  gif_u16 *mc_code;
  gif_u16 *mc_len;
  gif_u32 *mc_off;
  gif_u8  *mc_valid;
#endif

  // disposal 3 backup
  gif_u8   *disp3_buf;
  gif_usize disp3_size;
  gif_u8    disp3_valid;
  gif_u16   disp3_x;
  gif_u16   disp3_y;
  gif_u16   disp3_w;
  gif_u16   disp3_h;

  // prev frame
  gif_u8  prev_disposal;
  gif_u16 prev_x;
  gif_u16 prev_y;
  gif_u16 prev_w;
  gif_u16 prev_h;

  // canvas state
  gif_u8 canvas_inited;

  // LZW sub-block stream
  gif_u32 bitbuf;
  int     bitcount;
  gif_u8  sub_left;
  gif_u8  sub_ended;

  // renderer state
  gif_u8 renderer_started;

  // callback
  GIF_ErrorCallback on_error;
} GIF_Context;

// -----------------------------
// API
// -----------------------------

// *helper* ... avoid repeating macro in user code
gif_usize gif_get_required_scratch_size(void);

int gif_init(GIF_Context *ctx,
             const gif_u8 *data, gif_usize size,
             gif_u8 *scratch, gif_usize scratch_size);

int gif_get_info(GIF_Context *ctx, int *w, int *h);

int gif_set_error_callback(GIF_Context *ctx, GIF_ErrorCallback cb);

int gif_set_disposal3_buffer(GIF_Context *ctx, void *buf, gif_usize size);

int gif_rewind(GIF_Context *ctx);

int gif_close(GIF_Context *ctx);

int gif_next_frame(GIF_Context *ctx, void *canvas, int *delay_ms);

int gif_next_frame_rect(GIF_Context *ctx, void *canvas, int *delay_ms,
                        int *x, int *y, int *w, int *h);

// wrapper that returns a packed rect struct
static inline int gif_next_frame_rect_ex(GIF_Context *ctx, void *canvas, int *delay_ms, GIF_FrameRect *r)
{
  int rc;

  if (r == 0)
    return gif_next_frame(ctx, canvas, delay_ms);

  rc = gif_next_frame_rect(ctx, canvas, delay_ms, &r->x, &r->y, &r->w, &r->h);
  return rc;
}


int gif_next_frame_render(GIF_Context *ctx, const GIF_Renderer *r, int *delay_ms);

// -----------------------------
// COMPAT HELPERS
// -----------------------------

// *old return convention*
// - 1 => frame decoded
// - 0 => finished / no frame
// - -1 => error
int gif_next_frame_compat(GIF_Context *ctx, void *canvas, int *delay_ms);

// *old void api* ... thin wrappers
void gif_rewind_compat(GIF_Context *ctx);
void gif_close_compat(GIF_Context *ctx);

// -----------------------------
// IMPLEMENTATION
// -----------------------------

#ifdef GIF_IMPLEMENTATION

// -----------------------------
// ERROR STRINGS
// -----------------------------

// *compat* ... stable strings for rc
static const char *gif_error_strings[] = {
  "Success",
  "Decode error",
  "Invalid parameter",
  "Bad file",
  "Early EOF",
  "No frame",
  "Buffer too small",
  "Invalid frame dimensions",
  "Unsupported color depth",
  "Buffer overflow",
  "Invalid LZW code",
  "Unsupported dimensions"
};

const char *gif_get_error_string(int error_code)
{
  const char *s;
  int n;
  int idx;

  s = "Unknown error";
  n = (int)(sizeof(gif_error_strings) / sizeof(gif_error_strings[0]));
  idx = error_code;

  if (idx >= 0 && idx < n) {
    s = gif_error_strings[idx];
  }

  return s;
}

gif_usize gif_get_required_scratch_size(void)
{
  gif_usize v;
  v = (gif_usize)GIF_SCRATCH_BUFFER_REQUIRED_SIZE;
  return v;
}

// -----------------------------
// MINI RUNTIME
// -----------------------------

static void *gif_memset(void *dst, int v, gif_usize n)
{
  gif_u8 *d;
  gif_usize i;

  d = (gif_u8 *)dst;
  i = 0;

  while (i < n) {
    d[i] = (gif_u8)v;
    i++;
  }

  return dst;
}

static void *gif_memcpy(void *dst, const void *src, gif_usize n)
{
  gif_u8 *d;
  const gif_u8 *s;
  gif_usize i;

  d = (gif_u8 *)dst;
  s = (const gif_u8 *)src;
  i = 0;

  while (i < n) {
    d[i] = s[i];
    i++;
  }

  return dst;
}

static int gif_memcmp(const void *a, const void *b, gif_usize n)
{
  const gif_u8 *pa;
  const gif_u8 *pb;
  gif_usize i;
  int rc;

  pa = (const gif_u8 *)a;
  pb = (const gif_u8 *)b;

  i = 0;
  rc = 0;

  while (i < n && rc == 0) {
    if (pa[i] != pb[i]) {
      if (pa[i] < pb[i]) {
        rc = -1;
      } else {
        rc = 1;
      }
    }
    i++;
  }

  return rc;
}

static gif_u16 gif_read_u16_le(const gif_u8 *p)
{
  gif_u16 v;
  v = (gif_u16)p[0] | (gif_u16)((gif_u16)p[1] << 8);
  return v;
}

// *alignment* ... avoids ARM bus errors
static gif_u8 *gif_align_ptr(gif_u8 *p, gif_usize a)
{
  gif_uptr u;
  gif_uptr m;

  u = (gif_uptr)p;
  m = (gif_uptr)(a - 1u);
  u = (u + m) & ~m;

  return (gif_u8 *)u;
}

// *error hook* ... user decides what to do
static void gif_report(GIF_Context *ctx, int code, const char *msg)
{
  if (ctx != 0 && ctx->on_error != 0) {
    ctx->on_error(code, msg);
  }
}

// -----------------------------
// INPUT
// -----------------------------

static int gif_read_u8(GIF_Context *ctx, gif_u8 *out)
{
  int rc;

  rc = GIF_SUCCESS;

  if (ctx == 0 || out == 0) {
    rc = GIF_ERROR_INVALID_PARAM;
    goto out;
  }

  if (ctx->pos >= ctx->size) {
    rc = GIF_ERROR_EARLY_EOF;
    gif_report(ctx, rc, "read_u8 eof");
    goto out;
  }

  *out = ctx->data[ctx->pos];
  ctx->pos++;

out:
  return rc;
}

static int gif_read_bytes(GIF_Context *ctx, gif_u8 *dst, gif_usize n)
{
  int rc;

  rc = GIF_SUCCESS;

  if (ctx == 0 || dst == 0) {
    rc = GIF_ERROR_INVALID_PARAM;
    goto out;
  }

  if (ctx->pos + n > ctx->size) {
    rc = GIF_ERROR_EARLY_EOF;
    gif_report(ctx, rc, "read_bytes eof");
    goto out;
  }

  gif_memcpy(dst, ctx->data + ctx->pos, n);
  ctx->pos += n;

out:
  return rc;
}

static int gif_skip(GIF_Context *ctx, gif_usize n)
{
  int rc;

  rc = GIF_SUCCESS;

  if (ctx == 0) {
    rc = GIF_ERROR_INVALID_PARAM;
    goto out;
  }

  if (ctx->pos + n > ctx->size) {
    ctx->pos = ctx->size;
    rc = GIF_ERROR_EARLY_EOF;
    gif_report(ctx, rc, "skip eof");
    goto out;
  }

  ctx->pos += n;

out:
  return rc;
}

// -----------------------------
// SUB-BLOCKS
// -----------------------------

// *extension skip* ... terminator at EOF is ok
static int gif_discard_sub_blocks(GIF_Context *ctx)
{
  int rc;
  gif_u8 sz;

  rc = GIF_SUCCESS;
  sz = 0;

  while (rc == GIF_SUCCESS) {
    rc = gif_read_u8(ctx, &sz);
    if (rc != GIF_SUCCESS) {
      goto out;
    }

    if (sz == 0u) {
      goto out;
    }

    rc = gif_skip(ctx, (gif_usize)sz);
  }

out:
  return rc;
}

// -----------------------------
// EXTENSIONS
// -----------------------------

// *GCE* ... transparency + delay + disposal
static int gif_read_gce(GIF_Context *ctx)
{
  int rc;
  gif_u8 block_size;
  gif_u8 packed;
  gif_u8 dly[2];
  gif_u8 tindex;
  gif_u8 term;

  rc = GIF_SUCCESS;

  block_size = 0;
  packed = 0;
  tindex = 0;
  term = 0;

  rc = gif_read_u8(ctx, &block_size);
  if (rc != GIF_SUCCESS) goto out;

  if (block_size != GIF_GCE_BLOCK_SIZE) {
    rc = GIF_ERROR_BAD_FILE;
    gif_report(ctx, rc, "gce bad size");
    // skip payload + terminator
    (void)gif_skip(ctx, (gif_usize)block_size + 1u);
    goto out;
  }

  rc = gif_read_u8(ctx, &packed);
  if (rc != GIF_SUCCESS) goto out;

  rc = gif_read_bytes(ctx, dly, 2u);
  if (rc != GIF_SUCCESS) goto out;

  rc = gif_read_u8(ctx, &tindex);
  if (rc != GIF_SUCCESS) goto out;

  rc = gif_read_u8(ctx, &term);
  if (rc != GIF_SUCCESS) goto out;

  ctx->disposal_method = (gif_u8)((packed >> 2) & 0x07u);
  ctx->has_transparency = (gif_u8)(packed & 0x01u);
  ctx->transparent_index = tindex;
  ctx->delay_ms = (gif_u16)(gif_read_u16_le(dly) * 10u);

out:
  return rc;
}

// *netscape ids* ... only these carry loop semantics
static int gif_is_netscape_id(const gif_u8 *id11)
{
  int ok;

  ok = 0;

  if (gif_memcmp(id11, (const void *)"NETSCAPE2.0", 11u) == 0) {
    ok = 1;
  } else {
    if (gif_memcmp(id11, (const void *)"ANIMEXTS1.0", 11u) == 0) {
      ok = 1;
    }
  }

  return ok;
}

// *APP* ... only parse netscape loop
static int gif_read_app_ext(GIF_Context *ctx)
{
  int rc;
  gif_u8 block_size;
  gif_u8 id11[11];
  int is_ns;
  gif_u8 sub_sz;

  rc = GIF_SUCCESS;

  block_size = 0;
  sub_sz = 0;
  is_ns = 0;

  rc = gif_read_u8(ctx, &block_size);
  if (rc != GIF_SUCCESS) goto out;

  if (block_size != 11u) {
    rc = gif_skip(ctx, (gif_usize)block_size);
    if (rc != GIF_SUCCESS) goto out;
    rc = gif_discard_sub_blocks(ctx);
    goto out;
  }

  rc = gif_read_bytes(ctx, id11, 11u);
  if (rc != GIF_SUCCESS) goto out;

  is_ns = gif_is_netscape_id(id11);

  rc = gif_read_u8(ctx, &sub_sz);
  if (rc != GIF_SUCCESS) goto out;

  if (sub_sz == 0u) {
    goto out;
  }

  if (is_ns != 0 && sub_sz >= 3u) {
    gif_u8 sub_id;
    gif_u8 loops_le[2];
    gif_u16 loops;

    sub_id = 0;
    loops = 0;

    rc = gif_read_u8(ctx, &sub_id);
    if (rc != GIF_SUCCESS) goto out;

    rc = gif_read_bytes(ctx, loops_le, 2u);
    if (rc != GIF_SUCCESS) goto out;

    loops = gif_read_u16_le(loops_le);

    // *netscape semantics*
    // 0 => infinite
    if (sub_id == 1u) {
      if (loops == 0u) {
        ctx->loop_count = (gif_i16)-1;
        ctx->loop_count_init = (gif_i16)-1;
      } else {
        ctx->loop_count = (gif_i16)loops;
        ctx->loop_count_init = (gif_i16)loops;
      }
    }

    if (sub_sz > 3u) {
      rc = gif_skip(ctx, (gif_usize)(sub_sz - 3u));
      if (rc != GIF_SUCCESS) goto out;
    }

    rc = gif_discard_sub_blocks(ctx);
    goto out;
  } else {
    rc = gif_skip(ctx, (gif_usize)sub_sz);
    if (rc != GIF_SUCCESS) goto out;
    rc = gif_discard_sub_blocks(ctx);
    goto out;
  }

out:
  return rc;
}

// *extensions* ... unknown is not an error
static int gif_read_extension(GIF_Context *ctx)
{
  int rc;
  gif_u8 label;

  rc = GIF_SUCCESS;
  label = 0;

  rc = gif_read_u8(ctx, &label);
  if (rc != GIF_SUCCESS) goto out;

  if (label == GIF_EXT_GCE) {
    rc = gif_read_gce(ctx);
    goto out;
  } else {
    if (label == GIF_EXT_APP) {
      rc = gif_read_app_ext(ctx);
      goto out;
    } else {
      rc = gif_discard_sub_blocks(ctx);
      goto out;
    }
  }

out:
  return rc;
}

// -----------------------------
// COLOR TABLE
// -----------------------------

static int gif_read_color_table(GIF_Context *ctx, gif_u8 *dst_rgb, gif_u16 *out_n, gif_u16 ncolors)
{
  int rc;
  gif_usize need;

  rc = GIF_SUCCESS;
  need = (gif_usize)ncolors * 3u;

  if (ncolors > (gif_u16)GIF_MAX_COLORS) {
    rc = GIF_ERROR_UNSUPPORTED_COLOR_DEPTH;
    gif_report(ctx, rc, "palette too big");
    goto out;
  }

  rc = gif_read_bytes(ctx, dst_rgb, need);
  if (rc != GIF_SUCCESS) goto out;

  *out_n = ncolors;

out:
  return rc;
}

// -----------------------------
// PALETTE CACHE
// -----------------------------

static gif_u16 gif_rgb_to_565(gif_u8 r, gif_u8 g, gif_u8 b)
{
  gif_u16 pr;
  gif_u16 pg;
  gif_u16 pb;
  gif_u16 v;

  pr = (gif_u16)(r >> 3);
  pg = (gif_u16)(g >> 2);
  pb = (gif_u16)(b >> 3);

  v = (gif_u16)((pr << 11) | (pg << 5) | pb);
  return v;
}

// *prepare caches* ... avoids per-pixel RGB math
static void gif_prepare_palette_cache(GIF_Context *ctx)
{
#if (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGB565LE)
  gif_u16 i;

  i = 0;
  while (i < ctx->apal_n) {
    const gif_u8 *p;
    p = ctx->apal + (gif_usize)i * 3u;
    ctx->pal565[i] = gif_rgb_to_565(p[0], p[1], p[2]);
    i++;
  }

  #if (GIF_TURBO_MAP2 != 0)
  ctx->map2_ready = 0;
  #endif

#elif (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGBA8888)
  gif_u16 i;

  i = 0;
  while (i < ctx->apal_n) {
    const gif_u8 *p;
    gif_u32 v;

    p = ctx->apal + (gif_usize)i * 3u;
    v = 0;
    v |= (gif_u32)p[0] << 0;
    v |= (gif_u32)p[1] << 8;
    v |= (gif_u32)p[2] << 16;
    v |= (gif_u32)0xFFu << 24;

    ctx->pal32[i] = v;
    i++;
  }
#else
  (void)ctx;
#endif
}

#if (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGB565LE) && (GIF_TURBO_MAP2 != 0)

static void gif_build_map2_565(GIF_Context *ctx)
{
  gif_u32 i;

  if (ctx->map2_ready != 0) {
    return;
  }

  i = 0;
  while (i < 65536u) {
    gif_u8 a;
    gif_u8 b;
    gif_u16 pa;
    gif_u16 pb;
    gif_u32 v;

    a = (gif_u8)(i & 0xFFu);
    b = (gif_u8)(i >> 8);

    if (GIF_NO_CLAMP_INDEX == 0) {
      if (a >= (gif_u8)ctx->apal_n) a = 0;
      if (b >= (gif_u8)ctx->apal_n) b = 0;
    }

    pa = ctx->pal565[a];
    pb = ctx->pal565[b];

    v = (gif_u32)pa | ((gif_u32)pb << 16);

    ctx->map2_565[i] = v;
    i++;
  }

  ctx->map2_ready = 1;
}

#endif

// -----------------------------
// CANVAS HELPERS
// -----------------------------

static void gif_write_rgb_pixel(gif_u8 *dst, const gif_u8 *rgb)
{
#if (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGB888)
  dst[0] = rgb[0];
  dst[1] = rgb[1];
  dst[2] = rgb[2];
#elif (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGB565LE)
  {
    gif_u16 v;
    v = gif_rgb_to_565(rgb[0], rgb[1], rgb[2]);
    dst[0] = (gif_u8)(v & 0xFFu);
    dst[1] = (gif_u8)(v >> 8);
  }
#elif (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGBA8888)
  dst[0] = rgb[0];
  dst[1] = rgb[1];
  dst[2] = rgb[2];
  dst[3] = 0xFFu;
#endif
}

// *first frame* ... clear canvas to background
static void gif_canvas_clear(GIF_Context *ctx, void *canvas)
{
  gif_u8 *p;
  gif_usize total;
  gif_usize i;

  p = (gif_u8 *)canvas;
  total = (gif_usize)ctx->canvas_w * (gif_usize)ctx->canvas_h * (gif_usize)GIF_OUTPUT_BPP;

  i = 0;
  while (i < total) {
    gif_write_rgb_pixel(p + i, ctx->bg_rgb);
    i += (gif_usize)GIF_OUTPUT_BPP;
  }
}

static void gif_fill_rect_bg(GIF_Context *ctx, void *canvas,
                             gif_u16 x, gif_u16 y, gif_u16 w, gif_u16 h)
{
  gif_u8 *base;
  gif_usize stride;
  gif_u16 row;

  base = (gif_u8 *)canvas;
  stride = (gif_usize)ctx->canvas_w * (gif_usize)GIF_OUTPUT_BPP;

  row = 0;
  while (row < h) {
    gif_u8 *dst_row;
    gif_u16 col;

    dst_row = base + ((gif_usize)(y + row) * stride) + ((gif_usize)x * (gif_usize)GIF_OUTPUT_BPP);

    col = 0;
    while (col < w) {
      gif_write_rgb_pixel(dst_row + ((gif_usize)col * (gif_usize)GIF_OUTPUT_BPP), ctx->bg_rgb);
      col++;
    }

    row++;
  }
}

// -----------------------------
// DISPOSAL 3 BACKUP
// -----------------------------

// *disposal 3* ... only correct if user provides buffer
static int gif_backup_rect_if_needed(GIF_Context *ctx, void *canvas)
{
  int rc;
  gif_usize need;
  gif_usize stride;
  gif_u8 *src;
  gif_u8 *dst;
  gif_u16 row;

  rc = GIF_SUCCESS;
  ctx->disp3_valid = 0;

  if (ctx->disposal_method != 3u) {
    goto out;
  }

  if (ctx->disp3_buf == 0 || ctx->disp3_size == 0u) {
    // downgrade ... best effort
    ctx->disposal_method = 1u;
    goto out;
  }

  need = (gif_usize)ctx->fw * (gif_usize)ctx->fh * (gif_usize)GIF_OUTPUT_BPP;
  if (need > ctx->disp3_size) {
    ctx->disposal_method = 1u;
    goto out;
  }

  stride = (gif_usize)ctx->canvas_w * (gif_usize)GIF_OUTPUT_BPP;
  src = (gif_u8 *)canvas + ((gif_usize)ctx->fy * stride) + ((gif_usize)ctx->fx * (gif_usize)GIF_OUTPUT_BPP);
  dst = ctx->disp3_buf;

  row = 0;
  while (row < ctx->fh) {
    gif_memcpy(dst + (gif_usize)row * (gif_usize)ctx->fw * (gif_usize)GIF_OUTPUT_BPP,
               src + (gif_usize)row * stride,
               (gif_usize)ctx->fw * (gif_usize)GIF_OUTPUT_BPP);
    row++;
  }

  ctx->disp3_x = ctx->fx;
  ctx->disp3_y = ctx->fy;
  ctx->disp3_w = ctx->fw;
  ctx->disp3_h = ctx->fh;
  ctx->disp3_valid = 1;

out:
  return rc;
}

static void gif_restore_backup_if_needed(GIF_Context *ctx, void *canvas)
{
  gif_usize stride;
  gif_u8 *dst;
  gif_u8 *src;
  gif_u16 row;

  if (ctx->prev_disposal != 3u) {
    return;
  }

  if (ctx->disp3_valid == 0) {
    return;
  }

  stride = (gif_usize)ctx->canvas_w * (gif_usize)GIF_OUTPUT_BPP;
  dst = (gif_u8 *)canvas + ((gif_usize)ctx->disp3_y * stride) + ((gif_usize)ctx->disp3_x * (gif_usize)GIF_OUTPUT_BPP);
  src = ctx->disp3_buf;

  row = 0;
  while (row < ctx->disp3_h) {
    gif_memcpy(dst + (gif_usize)row * stride,
               src + (gif_usize)row * (gif_usize)ctx->disp3_w * (gif_usize)GIF_OUTPUT_BPP,
               (gif_usize)ctx->disp3_w * (gif_usize)GIF_OUTPUT_BPP);
    row++;
  }

  ctx->disp3_valid = 0;
}

static void gif_apply_prev_disposal(GIF_Context *ctx, void *canvas)
{
  if (ctx->canvas_inited == 0) {
    return;
  }

  if (ctx->prev_disposal == 2u) {
    gif_fill_rect_bg(ctx, canvas, ctx->prev_x, ctx->prev_y, ctx->prev_w, ctx->prev_h);
  } else {
    if (ctx->prev_disposal == 3u) {
      gif_restore_backup_if_needed(ctx, canvas);
    }
  }
}

// -----------------------------
// LZW SUB-BLOCK STREAM
// -----------------------------

static void gif_lzw_stream_begin(GIF_Context *ctx)
{
  ctx->sub_left = 0;
  ctx->sub_ended = 0;
}

static int gif_lzw_stream_read_byte(GIF_Context *ctx, gif_u8 *out)
{
  int rc;
  gif_u8 sz;

  rc = 0;
  sz = 0;

  if (ctx->sub_ended != 0) {
    rc = 0;
    goto out;
  }

  if (ctx->sub_left == 0u) {
    rc = gif_read_u8(ctx, &sz);
    if (rc != GIF_SUCCESS) {
      rc = -1;
      goto out;
    }

    // *guard* ... if we accidentally hit a GIF block marker, stop stream
    if (sz == GIF_TRAILER || sz == GIF_IMAGE_SEP || sz == GIF_EXT_INTRO) {
      if (ctx->pos != 0u) {
        ctx->pos -= 1u;
      }
      ctx->sub_ended = 1u;
      rc = 0;
      goto out;
    }

    if (sz == 0u) {
      ctx->sub_ended = 1u;
      rc = 0;
      goto out;
    }

    ctx->sub_left = sz;
  }

  rc = gif_read_u8(ctx, out);
  if (rc != GIF_SUCCESS) {
    rc = -1;
    goto out;
  }

  ctx->sub_left--;
  rc = 1;

out:
  return rc;
}

static int gif_lzw_discard_rest(GIF_Context *ctx)
{
  int rc;
  gif_u8 b;

  rc = GIF_SUCCESS;
  b = 0;

  while (ctx->sub_left != 0u && rc == GIF_SUCCESS) {
    rc = gif_read_u8(ctx, &b);
    if (rc != GIF_SUCCESS) {
      goto out;
    }
    ctx->sub_left--;
  }

  if (ctx->sub_ended != 0u) {
    goto out;
  }

  // *guard* ... avoid eating next block when terminator already consumed
  if (ctx->pos < ctx->size) {
    gif_u8 nb;
    nb = ctx->data[ctx->pos];
    if (nb == GIF_TRAILER || nb == GIF_IMAGE_SEP || nb == GIF_EXT_INTRO) {
      goto out;
    }
  }

  while (rc == GIF_SUCCESS && ctx->sub_ended == 0u) {
    int r;
    r = gif_lzw_stream_read_byte(ctx, &b);
    if (r < 0) {
      rc = GIF_ERROR_EARLY_EOF;
      goto out;
    }
    if (r == 0) {
      goto out;
    }
  }

out:
  return rc;
}

// -----------------------------
// LZW BIT READER
// -----------------------------

static int gif_lzw_read_code(GIF_Context *ctx, gif_u16 code_size, gif_u16 code_mask, gif_u16 *out_code)
{
  int rc;

  rc = GIF_SUCCESS;

  while (ctx->bitcount < (int)code_size) {
    gif_u8 b;
    int r;

    b = 0;
    r = gif_lzw_stream_read_byte(ctx, &b);

    if (r < 0) {
      rc = GIF_ERROR_EARLY_EOF;
      goto out;
    }

    if (r == 0) {
      rc = GIF_ERROR_EARLY_EOF;
      goto out;
    }

    ctx->bitbuf |= ((gif_u32)b << (gif_u32)ctx->bitcount);
    ctx->bitcount += 8;
  }

  *out_code = (gif_u16)(ctx->bitbuf & (gif_u32)code_mask);

  ctx->bitbuf >>= code_size;
  ctx->bitcount -= (int)code_size;

out:
  return rc;
}

// -----------------------------
// MICRO-CACHE
// -----------------------------

#if (GIF_LZW_MICROCACHE != 0)

static void gif_mc_reset(GIF_Context *ctx)
{
  gif_u16 i;

  ctx->mc_arena_pos = 0;

  i = 0;
  while (i < (gif_u16)GIF_LZW_MICROCACHE_SLOTS) {
    ctx->mc_valid[i] = 0;
    i++;
  }
}

static gif_u16 gif_mc_slot(gif_u16 code)
{
  gif_u16 s;
  s = (gif_u16)(code & (gif_u16)(GIF_LZW_MICROCACHE_SLOTS - 1));
  return s;
}

static int gif_mc_lookup(GIF_Context *ctx, gif_u16 code, const gif_u8 **out_ptr, gif_u16 *out_len)
{
  gif_u16 s;

  s = gif_mc_slot(code);

  if (ctx->mc_valid[s] == 0) {
    return 0;
  }

  if (ctx->mc_code[s] != code) {
    return 0;
  }

  *out_ptr = ctx->mc_arena + ctx->mc_off[s];
  *out_len = ctx->mc_len[s];

  return 1;
}

static void gif_mc_insert(GIF_Context *ctx, gif_u16 code, const gif_u8 *span, gif_u16 len)
{
  gif_u16 s;
  gif_u32 need;

  if (len == 0) {
    return;
  }

  if (len > (gif_u16)GIF_LZW_MICROCACHE_MAXLEN) {
    return;
  }

  need = (gif_u32)len;

  // *arena wrap* ... cheap and blunt
  if (ctx->mc_arena_pos + need > (gif_u32)GIF_LZW_MICROCACHE_ARENA_SIZE) {
    gif_mc_reset(ctx);
  }

  s = gif_mc_slot(code);

  ctx->mc_code[s] = code;
  ctx->mc_len[s] = len;
  ctx->mc_off[s] = ctx->mc_arena_pos;

  gif_memcpy(ctx->mc_arena + ctx->mc_arena_pos, span, (gif_usize)len);
  ctx->mc_arena_pos += need;

  ctx->mc_valid[s] = 1;
}

#endif

// -----------------------------
// WRITERS
// -----------------------------

static void gif_blit_plain_row(GIF_Context *ctx, gif_u8 *dst_row, const gif_u8 *idx_row)
{
  gif_u16 col;

  col = 0;
  while (col < ctx->fw) {
    gif_u8 idx;

    idx = idx_row[col];

    if (ctx->has_transparency != 0u && idx == ctx->transparent_index) {
      // keep destination
    } else {
      if (GIF_NO_CLAMP_INDEX == 0) {
        if (idx >= (gif_u8)ctx->apal_n) {
          idx = 0;
        }
      }

#if (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGB888)
      {
        const gif_u8 *p;
        gif_u8 *d;

        p = ctx->apal + (gif_usize)idx * 3u;
        d = dst_row + (gif_usize)col * 3u;

        d[0] = p[0];
        d[1] = p[1];
        d[2] = p[2];
      }
#elif (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGB565LE)
      {
        gif_u16 v;
        gif_u8 *d;

        v = ctx->pal565[idx];
        d = dst_row + (gif_usize)col * 2u;

        d[0] = (gif_u8)(v & 0xFFu);
        d[1] = (gif_u8)(v >> 8);
      }
#elif (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGBA8888)
      {
        gif_u32 v;
        gif_u8 *d;

        v = ctx->pal32[idx];
        d = dst_row + (gif_usize)col * 4u;

        d[0] = (gif_u8)(v >> 0);
        d[1] = (gif_u8)(v >> 8);
        d[2] = (gif_u8)(v >> 16);
        d[3] = (gif_u8)(v >> 24);
      }
#endif
    }

    col++;
  }
}

#if (GIF_TURBO_BLIT != 0)

static void gif_blit_turbo_row(GIF_Context *ctx, gif_u8 *dst_row, const gif_u8 *idx_row)
{
  gif_u16 col;

  col = 0;
  while (col < ctx->fw) {
    gif_u8 idx;
    gif_u16 run;
    gif_u16 j;

    idx = idx_row[col];

    // *span find* ... reduce per-pixel branching
    run = 1;
    j = (gif_u16)(col + 1u);
    while (j < ctx->fw) {
      if (idx_row[j] != idx) {
        j = ctx->fw;
      } else {
        run++;
        j++;
      }
    }

    if (ctx->has_transparency != 0u && idx == ctx->transparent_index) {
      // transparent span ... keep destination
    } else {
      if (GIF_NO_CLAMP_INDEX == 0) {
        if (idx >= (gif_u8)ctx->apal_n) {
          idx = 0;
        }
      }

#if (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGB888)
      {
        const gif_u8 *p;
        gif_u8 *d;
        gif_u16 k;

        p = ctx->apal + (gif_usize)idx * 3u;
        d = dst_row + (gif_usize)col * 3u;

        k = 0;
        while (k < run) {
          d[0] = p[0];
          d[1] = p[1];
          d[2] = p[2];
          d += 3;
          k++;
        }
      }
#elif (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGBA8888)
      {
        gif_u32 v;
        gif_u8 *d;
        gif_u16 k;

        v = ctx->pal32[idx];
        d = dst_row + (gif_usize)col * 4u;

        k = 0;
        while (k < run) {
          d[0] = (gif_u8)(v >> 0);
          d[1] = (gif_u8)(v >> 8);
          d[2] = (gif_u8)(v >> 16);
          d[3] = (gif_u8)(v >> 24);
          d += 4;
          k++;
        }
      }
#elif (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGB565LE)
      {
        gif_u16 v;
        gif_u8 *d;
        gif_u16 k;

        v = ctx->pal565[idx];
        d = dst_row + (gif_usize)col * 2u;

        k = 0;
        while (k < run) {
          d[0] = (gif_u8)(v & 0xFFu);
          d[1] = (gif_u8)(v >> 8);
          d += 2;
          k++;
        }
      }
#endif
    }

    col = (gif_u16)(col + run);
  }
}

#endif

#if (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGB565LE) && (GIF_TURBO_BLIT != 0) && (GIF_TURBO_MAP2 != 0)

static void gif_blit_map2_565_row(GIF_Context *ctx, gif_u8 *dst_row, const gif_u8 *idx_row)
{
  gif_u16 col;

  gif_build_map2_565(ctx);

  col = 0;
  while (col + 1u < ctx->fw) {
    gif_u8 a;
    gif_u8 b;

    a = idx_row[col + 0u];
    b = idx_row[col + 1u];

    if (ctx->has_transparency != 0u) {
      if (a == ctx->transparent_index || b == ctx->transparent_index) {
        gif_blit_plain_row(ctx, dst_row + (gif_usize)col * 2u, idx_row + col);
        col = (gif_u16)(col + 2u);
      } else {
        gif_u32 key;
        gif_u32 v;
        gif_u8 *d;

        key = (gif_u32)a | ((gif_u32)b << 8);
        v = ctx->map2_565[key];

        d = dst_row + (gif_usize)col * 2u;
        d[0] = (gif_u8)(v >> 0);
        d[1] = (gif_u8)(v >> 8);
        d[2] = (gif_u8)(v >> 16);
        d[3] = (gif_u8)(v >> 24);

        col = (gif_u16)(col + 2u);
      }
    } else {
      gif_u32 key;
      gif_u32 v;
      gif_u8 *d;

      key = (gif_u32)a | ((gif_u32)b << 8);
      v = ctx->map2_565[key];

      d = dst_row + (gif_usize)col * 2u;
      d[0] = (gif_u8)(v >> 0);
      d[1] = (gif_u8)(v >> 8);
      d[2] = (gif_u8)(v >> 16);
      d[3] = (gif_u8)(v >> 24);

      col = (gif_u16)(col + 2u);
    }
  }

  if (col < ctx->fw) {
    gif_blit_plain_row(ctx, dst_row + (gif_usize)col * 2u, idx_row + col);
  }
}

#endif

// -----------------------------
// ROW EMIT
// -----------------------------

static int gif_emit_row_canvas(GIF_Context *ctx, void *canvas, int y_draw)
{
  gif_u8 *base;
  gif_usize stride;
  gif_u8 *dst_row;
  gif_u16 save_fw;
  gif_u16 safe_fw;
  gif_u32 avail;
  gif_u32 y;

  base = (gif_u8 *)canvas;

  // *guard* ... clamp frame row to canvas bounds
  y = (gif_u32)ctx->fy + (gif_u32)(gif_u16)y_draw;
  if (y >= ctx->canvas_h) {
    return GIF_SUCCESS;
  }

  if ((gif_u32)ctx->fx >= ctx->canvas_w) {
    return GIF_SUCCESS;
  }

  avail = ctx->canvas_w - (gif_u32)ctx->fx;
  safe_fw = ctx->fw;
  if ((gif_u32)safe_fw > avail) {
    safe_fw = (gif_u16)avail;
  }

  if (safe_fw == 0u) {
    return GIF_SUCCESS;
  }

  stride = (gif_usize)ctx->canvas_w * (gif_usize)GIF_OUTPUT_BPP;

  dst_row = base + ((gif_usize)y * stride) +
            ((gif_usize)ctx->fx * (gif_usize)GIF_OUTPUT_BPP);

  save_fw = ctx->fw;
  ctx->fw = safe_fw;

#if (GIF_TURBO_BLIT != 0)
  #if (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGB565LE) && (GIF_TURBO_MAP2 != 0)
    gif_blit_map2_565_row(ctx, dst_row, ctx->line_idx);
  #else
    gif_blit_turbo_row(ctx, dst_row, ctx->line_idx);
  #endif
#else
  gif_blit_plain_row(ctx, dst_row, ctx->line_idx);
#endif

  ctx->fw = save_fw;

  return GIF_SUCCESS;
}

static int gif_emit_row_renderer(GIF_Context *ctx, const GIF_Renderer *r, int y_draw)
{
  if (r != 0 && r->blit_indexed != 0) {
    r->blit_indexed(r->user,
                    (int)ctx->fx,
                    (int)(ctx->fy + (gif_u16)y_draw),
                    (int)ctx->fw,
                    1,
                    ctx->line_idx,
                    (int)ctx->fw,
                    ctx->apal,
                    (int)ctx->apal_n,
                    (int)ctx->transparent_index,
                    (int)ctx->has_transparency);
  }

  return GIF_SUCCESS;
}

static int gif_emit_row(GIF_Context *ctx, void *canvas, const GIF_Renderer *r, int y_draw)
{
  int rc;

  rc = GIF_SUCCESS;

  if (r != 0) {
    rc = gif_emit_row_renderer(ctx, r, y_draw);
    goto out;
  }

  rc = gif_emit_row_canvas(ctx, canvas, y_draw);

out:
  return rc;
}

// -----------------------------
// INTERLACE MAP
// -----------------------------

static int gif_map_interlace_y(gif_u16 fh, int *pass, int *pass_row, int *out_y)
{
  int y;
  int start;
  int step;

  y = 0;
  start = 0;
  step = 8;

  if (*pass == 0) {
    start = 0;
    step = 8;
  } else {
    if (*pass == 1) {
      start = 4;
      step = 8;
    } else {
      if (*pass == 2) {
        start = 2;
        step = 4;
      } else {
        start = 1;
        step = 2;
      }
    }
  }

  y = start + (*pass_row) * step;

  while (y >= (int)fh && *pass < 3) {
    *pass += 1;
    *pass_row = 0;

    if (*pass == 0) {
      start = 0;
      step = 8;
    } else {
      if (*pass == 1) {
        start = 4;
        step = 8;
      } else {
        if (*pass == 2) {
          start = 2;
          step = 4;
        } else {
          start = 1;
          step = 2;
        }
      }
    }

    y = start + (*pass_row) * step;
  }

  if (y >= (int)fh) {
    return 0;
  }

  *pass_row += 1;
  *out_y = y;
  return 1;
}

// -----------------------------
// LZW OUTPUT PUMP
// -----------------------------

typedef struct {
  void *canvas;
  const GIF_Renderer *r;

  gif_u32 out_x;
  int out_lines;

  int interlaced;
  int pass;
  int pass_row;

  int done;
} GIF_Pump;

static int gif_pump_push_span(GIF_Context *ctx, GIF_Pump *p, const gif_u8 *src, gif_u16 len)
{
  int rc;
  gif_u16 i;

  rc = GIF_SUCCESS;
  i = 0;

  if (p->done != 0) {
    goto out;
  }

  while (i < len && rc == GIF_SUCCESS) {
    ctx->line_idx[p->out_x] = src[i];
    p->out_x += 1;
    i++;

    if (p->out_x == (gif_u32)ctx->fw) {
      int y_draw;

      y_draw = p->out_lines;

      if (p->interlaced != 0) {
        int ok;
        ok = gif_map_interlace_y(ctx->fh, &p->pass, &p->pass_row, &y_draw);
        if (ok == 0) {
          rc = GIF_ERROR_DECODE;
          goto out;
        }
      }

      rc = gif_emit_row(ctx, p->canvas, p->r, y_draw);
      if (rc != GIF_SUCCESS) goto out;

      p->out_x = 0;
      p->out_lines += 1;

      // *done* ... stop after expected frame pixels
      if (p->out_lines >= (int)ctx->fh) {
        p->done = 1;
        goto out;
      }
    }
  }

out:
  return rc;
}

// -----------------------------
// LZW EXPAND
// -----------------------------

static int gif_lzw_expand_to_stack_safe(GIF_Context *ctx, gif_u16 clear, gif_u16 code, gif_u32 *out_sp)
{
  int rc;
  gif_u16 c;
  gif_u32 sp;

  rc = GIF_SUCCESS;
  c = code;
  sp = 0;

  // *prefix walk* ... worst case long chain
  while (c != 0xFFFFu && c >= clear) {
    ctx->lzw_stack[sp] = ctx->lzw_suffix[c];
    sp += 1;
    c = ctx->lzw_prefix[c];

    if (sp >= (gif_u32)GIF_LZW_TABLE_ENTRIES) {
      rc = GIF_ERROR_BUFFER_OVERFLOW;
      goto out;
    }
  }

  if (c == 0xFFFFu) {
    rc = GIF_ERROR_INVALID_LZW_CODE;
    goto out;
  }

  ctx->lzw_stack[sp] = (gif_u8)c;
  sp += 1;

  *out_sp = sp;

out:
  return rc;
}

// *unsafe expand* ... fewer checks
static void gif_lzw_expand_to_stack_unsafe(GIF_Context *ctx, gif_u16 clear, gif_u16 code, gif_u32 *out_sp)
{
  gif_u16 c;
  gif_u32 sp;

  c = code;
  sp = 0;

  while (c >= clear) {
    ctx->lzw_stack[sp] = ctx->lzw_suffix[c];
    sp += 1;
    c = ctx->lzw_prefix[c];
  }

  ctx->lzw_stack[sp] = (gif_u8)c;
  sp += 1;

  *out_sp = sp;
}

// -----------------------------
// LZW DECODE
// -----------------------------

static int gif_decode_image_data_safe(GIF_Context *ctx, void *canvas, const GIF_Renderer *r)
{
  int rc;

  gif_u16 clear;
  gif_u16 eoi;

  gif_u16 code_size;
  gif_u16 code_mask;
  gif_u16 next_code;
  gif_u16 max_code;

  gif_u16 old_code;
  gif_u8  old_first;

  gif_u16 code;
  gif_u16 in_code;

  GIF_Pump pump;

  rc = GIF_SUCCESS;

  gif_lzw_stream_begin(ctx);
  ctx->bitbuf = 0u;
  ctx->bitcount = 0;

  clear = (gif_u16)(1u << ctx->lzw_min_code_size);
  eoi = (gif_u16)(clear + 1u);

  code_size = (gif_u16)(ctx->lzw_min_code_size + 1u);
  code_mask = (gif_u16)((1u << code_size) - 1u);

  next_code = (gif_u16)(eoi + 1u);
  max_code = (gif_u16)(1u << code_size);

  // *base dictionary* ... codes 0..clear-1
  {
    gif_u16 i;
    i = 0;
    while (i < clear) {
      ctx->lzw_prefix[i] = 0xFFFFu;
      ctx->lzw_suffix[i] = (gif_u8)i;
      i++;
    }
  }

#if (GIF_LZW_MICROCACHE != 0)
  gif_mc_reset(ctx);
#endif

  // *pump init*
  pump.canvas = canvas;
  pump.r = r;
  pump.out_x = 0;
  pump.out_lines = 0;
  pump.interlaced = 0;
  pump.pass = 0;
  pump.pass_row = 0;
  pump.done = 0;

  if ((ctx->packed_img & 0x40u) != 0u) {
    pump.interlaced = 1;
  }

  old_code = 0;
  old_first = 0;

  // main loop ... clear/eoi handled by if/else
  {
    int running;
    running = 1;

    while (running != 0 && rc == GIF_SUCCESS) {
      rc = gif_lzw_read_code(ctx, code_size, code_mask, &code);
      if (rc != GIF_SUCCESS) goto out;

      if (code == clear) {
        code_size = (gif_u16)(ctx->lzw_min_code_size + 1u);
        code_mask = (gif_u16)((1u << code_size) - 1u);
        next_code = (gif_u16)(eoi + 1u);
        max_code = (gif_u16)(1u << code_size);

#if (GIF_LZW_MICROCACHE != 0)
        gif_mc_reset(ctx);
#endif

        rc = gif_lzw_read_code(ctx, code_size, code_mask, &code);
        if (rc != GIF_SUCCESS) goto out;

        if (code == eoi) {
          running = 0;
          goto out;
        }

        // first symbol after clear
        {
          const gif_u8 *span;
          gif_u16 slen;
          int hit;

          span = 0;
          slen = 0;
          hit = 0;

#if (GIF_LZW_MICROCACHE != 0)
          hit = gif_mc_lookup(ctx, code, &span, &slen);
#endif

          if (hit != 0) {
            old_first = span[0];
            rc = gif_pump_push_span(ctx, &pump, span, slen);
            if (rc != GIF_SUCCESS) goto out;
            if (pump.done != 0) { running = 0; goto out; }
          } else {
            gif_u32 sp;
            sp = 0;

            rc = gif_lzw_expand_to_stack_safe(ctx, clear, code, &sp);
            if (rc != GIF_SUCCESS) goto out;

            old_first = ctx->lzw_stack[sp - 1u];

            // *stack reverse* ... make forward span
            {
              gif_u8 tmp[GIF_LZW_MICROCACHE_MAXLEN];
              gif_u16 len;
              gif_u32 k;

              len = (gif_u16)sp;
              k = 0;

              if (len <= (gif_u16)GIF_LZW_MICROCACHE_MAXLEN) {
                while (k < sp) {
                  tmp[k] = ctx->lzw_stack[sp - 1u - k];
                  k++;
                }

                rc = gif_pump_push_span(ctx, &pump, tmp, len);
                if (rc != GIF_SUCCESS) goto out;
            if (pump.done != 0) { running = 0; goto out; }

#if (GIF_LZW_MICROCACHE != 0)
                gif_mc_insert(ctx, code, tmp, len);
#endif
              } else {
                // long span ... pump by popping
                while (sp != 0u && rc == GIF_SUCCESS) {
                  gif_u8 px;
                  sp--;
                  px = ctx->lzw_stack[sp];
                  rc = gif_pump_push_span(ctx, &pump, &px, 1);
                }
                if (rc != GIF_SUCCESS) goto out;
              }
            }
          }
        }

        old_code = code;

      } else {
        if (code == eoi) {
          running = 0;
          goto out;
        }

        in_code = code;

        if (code >= next_code) {
          code = old_code;
        }

        // expand code into span
        {
          const gif_u8 *span;
          gif_u16 slen;
          int hit;

          span = 0;
          slen = 0;
          hit = 0;

#if (GIF_LZW_MICROCACHE != 0)
          hit = gif_mc_lookup(ctx, code, &span, &slen);
#endif

          if (hit != 0) {
            gif_u8 first_char;
            first_char = span[0];

            if (in_code >= next_code) {
              // special ... append old_first
              gif_u8 tmp2[GIF_LZW_MICROCACHE_MAXLEN + 1u];
              gif_u16 k;

              if (slen < (gif_u16)GIF_LZW_MICROCACHE_MAXLEN) {
                k = 0;
                while (k < slen) {
                  tmp2[k] = span[k];
                  k++;
                }
                tmp2[slen] = old_first;
                rc = gif_pump_push_span(ctx, &pump, tmp2, (gif_u16)(slen + 1u));
                if (rc != GIF_SUCCESS) goto out;
            if (pump.done != 0) { running = 0; goto out; }

#if (GIF_LZW_MICROCACHE != 0)
                gif_mc_insert(ctx, in_code, tmp2, (gif_u16)(slen + 1u));
#endif
                first_char = tmp2[0];
              } else {
                rc = gif_pump_push_span(ctx, &pump, span, slen);
                if (rc != GIF_SUCCESS) goto out;
            if (pump.done != 0) { running = 0; goto out; }
                rc = gif_pump_push_span(ctx, &pump, &old_first, 1);
                if (rc != GIF_SUCCESS) goto out;
            if (pump.done != 0) { running = 0; goto out; }
              }
            } else {
              rc = gif_pump_push_span(ctx, &pump, span, slen);
              if (rc != GIF_SUCCESS) goto out;
            if (pump.done != 0) { running = 0; goto out; }
            }

            // add dictionary entry
            if (next_code < (gif_u16)GIF_LZW_TABLE_ENTRIES) {
              ctx->lzw_prefix[next_code] = old_code;
              ctx->lzw_suffix[next_code] = first_char;
              next_code++;

              if (next_code == max_code && code_size < GIF_MAX_CODE_SIZE) {
                code_size++;
                code_mask = (gif_u16)((1u << code_size) - 1u);
                max_code = (gif_u16)(1u << code_size);
              }
            }

            old_first = first_char;
            old_code = in_code;

          } else {
            // miss ... expand via prefix walk
            gif_u32 sp;
            gif_u8 first_char;

            sp = 0;
            first_char = 0;

            rc = gif_lzw_expand_to_stack_safe(ctx, clear, code, &sp);
            if (rc != GIF_SUCCESS) goto out;

            first_char = ctx->lzw_stack[sp - 1u];

            if (in_code >= next_code) {
              // special ... append old_first
              if (sp < (gif_u32)GIF_LZW_TABLE_ENTRIES) {
                ctx->lzw_stack[sp] = old_first;
                sp += 1;
              } else {
                rc = GIF_ERROR_BUFFER_OVERFLOW;
                goto out;
              }

              first_char = old_first;
            }

            // pump span and maybe cache
            {
              gif_u8 tmp[GIF_LZW_MICROCACHE_MAXLEN + 1u];
              gif_u16 len;
              gif_u32 k;

              len = (gif_u16)sp;
              k = 0;

              if (len <= (gif_u16)(GIF_LZW_MICROCACHE_MAXLEN + 1u)) {
                while (k < sp) {
                  tmp[k] = ctx->lzw_stack[sp - 1u - k];
                  k++;
                }

                rc = gif_pump_push_span(ctx, &pump, tmp, len);
                if (rc != GIF_SUCCESS) goto out;
            if (pump.done != 0) { running = 0; goto out; }

#if (GIF_LZW_MICROCACHE != 0)
                gif_mc_insert(ctx, in_code, tmp, len);
#endif
              } else {
                while (sp != 0u && rc == GIF_SUCCESS) {
                  gif_u8 px;
                  sp--;
                  px = ctx->lzw_stack[sp];
                  rc = gif_pump_push_span(ctx, &pump, &px, 1);
                }
                if (rc != GIF_SUCCESS) goto out;
              }
            }

            // add dictionary entry
            if (next_code < (gif_u16)GIF_LZW_TABLE_ENTRIES) {
              ctx->lzw_prefix[next_code] = old_code;
              ctx->lzw_suffix[next_code] = first_char;
              next_code++;

              if (next_code == max_code && code_size < GIF_MAX_CODE_SIZE) {
                code_size++;
                code_mask = (gif_u16)((1u << code_size) - 1u);
                max_code = (gif_u16)(1u << code_size);
              }
            }

            old_first = first_char;
            old_code = in_code;
          }
        }
      }
    }
  }

out:
  {
    int drc;
    drc = gif_lzw_discard_rest(ctx);
    if (rc == GIF_SUCCESS && drc != GIF_SUCCESS) {
      rc = drc;
    }
  }

  return rc;
}

// *unsafe backend* ... same stream engine, fewer checks
static int gif_decode_image_data_unsafe(GIF_Context *ctx, void *canvas, const GIF_Renderer *r)
{
  int rc;

  gif_u16 clear;
  gif_u16 eoi;

  gif_u16 code_size;
  gif_u16 code_mask;
  gif_u16 next_code;
  gif_u16 max_code;

  gif_u16 old_code;
  gif_u8  old_first;

  gif_u16 code;
  gif_u16 in_code;

  GIF_Pump pump;

  rc = GIF_SUCCESS;

  // *risk gate* ... you must accept the risks
  if (GIF_TURBO_RISK_ACCEPTED == 0) {
    rc = GIF_ERROR_INVALID_PARAM;
    gif_report(ctx, rc, "turbo risk not accepted");
    goto out;
  }

  gif_lzw_stream_begin(ctx);
  ctx->bitbuf = 0u;
  ctx->bitcount = 0;

  clear = (gif_u16)(1u << ctx->lzw_min_code_size);
  eoi = (gif_u16)(clear + 1u);

  code_size = (gif_u16)(ctx->lzw_min_code_size + 1u);
  code_mask = (gif_u16)((1u << code_size) - 1u);

  next_code = (gif_u16)(eoi + 1u);
  max_code = (gif_u16)(1u << code_size);

  // dictionary init ... no checks
  {
    gif_u16 i;
    i = 0;
    while (i < clear) {
      ctx->lzw_prefix[i] = 0xFFFFu;
      ctx->lzw_suffix[i] = (gif_u8)i;
      i++;
    }
  }

#if (GIF_LZW_MICROCACHE != 0)
  gif_mc_reset(ctx);
#endif

  pump.canvas = canvas;
  pump.r = r;
  pump.out_x = 0;
  pump.out_lines = 0;
  pump.interlaced = 0;
  pump.pass = 0;
  pump.pass_row = 0;
  pump.done = 0;

  if ((ctx->packed_img & 0x40u) != 0u) {
    pump.interlaced = 1;
  }

  old_code = 0;
  old_first = 0;

  {
    int running;
    running = 1;

    while (running != 0 && rc == GIF_SUCCESS) {
      rc = gif_lzw_read_code(ctx, code_size, code_mask, &code);
      if (rc != GIF_SUCCESS) goto out;

      if (code == clear) {
        code_size = (gif_u16)(ctx->lzw_min_code_size + 1u);
        code_mask = (gif_u16)((1u << code_size) - 1u);
        next_code = (gif_u16)(eoi + 1u);
        max_code = (gif_u16)(1u << code_size);

#if (GIF_LZW_MICROCACHE != 0)
        gif_mc_reset(ctx);
#endif

        rc = gif_lzw_read_code(ctx, code_size, code_mask, &code);
        if (rc != GIF_SUCCESS) goto out;

        if (code == eoi) {
          running = 0;
          goto out;
        }

        {
          gif_u32 sp;
          gif_u8 tmp[GIF_LZW_MICROCACHE_MAXLEN + 1u];
          gif_u16 len;
          gif_u32 k;

          sp = 0;
          gif_lzw_expand_to_stack_unsafe(ctx, clear, code, &sp);

          old_first = ctx->lzw_stack[sp - 1u];

          len = (gif_u16)sp;
          k = 0;

          if (len <= (gif_u16)(GIF_LZW_MICROCACHE_MAXLEN + 1u)) {
            while (k < sp) {
              tmp[k] = ctx->lzw_stack[sp - 1u - k];
              k++;
            }

            rc = gif_pump_push_span(ctx, &pump, tmp, len);
            if (rc != GIF_SUCCESS) goto out;
            if (pump.done != 0) { running = 0; goto out; }

#if (GIF_LZW_MICROCACHE != 0)
            gif_mc_insert(ctx, code, tmp, len);
#endif
          } else {
            while (sp != 0u && rc == GIF_SUCCESS) {
              gif_u8 px;
              sp--;
              px = ctx->lzw_stack[sp];
              rc = gif_pump_push_span(ctx, &pump, &px, 1);
            }
            if (rc != GIF_SUCCESS) goto out;
          }
        }

        old_code = code;

      } else {
        if (code == eoi) {
          running = 0;
          goto out;
        }

        in_code = code;

        if (code >= next_code) {
          code = old_code;
        }

        {
          gif_u32 sp;
          gif_u8 first_char;
          gif_u8 tmp[GIF_LZW_MICROCACHE_MAXLEN + 1u];
          gif_u16 len;
          gif_u32 k;

          sp = 0;
          gif_lzw_expand_to_stack_unsafe(ctx, clear, code, &sp);

          first_char = ctx->lzw_stack[sp - 1u];

          if (in_code >= next_code) {
            ctx->lzw_stack[sp] = old_first;
            sp += 1;
            first_char = old_first;
          }

          len = (gif_u16)sp;
          k = 0;

          if (len <= (gif_u16)(GIF_LZW_MICROCACHE_MAXLEN + 1u)) {
            while (k < sp) {
              tmp[k] = ctx->lzw_stack[sp - 1u - k];
              k++;
            }

            rc = gif_pump_push_span(ctx, &pump, tmp, len);
            if (rc != GIF_SUCCESS) goto out;
            if (pump.done != 0) { running = 0; goto out; }

#if (GIF_LZW_MICROCACHE != 0)
            gif_mc_insert(ctx, in_code, tmp, len);
#endif
          } else {
            while (sp != 0u && rc == GIF_SUCCESS) {
              gif_u8 px;
              sp--;
              px = ctx->lzw_stack[sp];
              rc = gif_pump_push_span(ctx, &pump, &px, 1);
            }
            if (rc != GIF_SUCCESS) goto out;
          }

          if (next_code < (gif_u16)GIF_LZW_TABLE_ENTRIES) {
            ctx->lzw_prefix[next_code] = old_code;
            ctx->lzw_suffix[next_code] = first_char;
            next_code++;

            if (next_code == max_code && code_size < GIF_MAX_CODE_SIZE) {
              code_size++;
              code_mask = (gif_u16)((1u << code_size) - 1u);
              max_code = (gif_u16)(1u << code_size);
            }
          }

          old_first = first_char;
          old_code = in_code;
        }
      }
    }
  }

out:
  {
    int drc;
    drc = gif_lzw_discard_rest(ctx);
    if (rc == GIF_SUCCESS && drc != GIF_SUCCESS) {
      rc = drc;
    }
  }

  return rc;
}

// -----------------------------
// FRAME PARSE
// -----------------------------

static int gif_read_image_descriptor(GIF_Context *ctx)
{
  int rc;
  gif_u8 desc[9];
  gif_u16 x;
  gif_u16 y;
  gif_u16 w;
  gif_u16 h;
  gif_u8 packed;

  rc = GIF_SUCCESS;

  rc = gif_read_bytes(ctx, desc, 9u);
  if (rc != GIF_SUCCESS) goto out;

  x = gif_read_u16_le(desc + 0);
  y = gif_read_u16_le(desc + 2);
  w = gif_read_u16_le(desc + 4);
  h = gif_read_u16_le(desc + 6);
  packed = desc[8];

  ctx->fx = x;
  ctx->fy = y;
  ctx->fw = w;
  ctx->fh = h;
  ctx->packed_img = packed;

  // *dimension guards* ... avoid line buffer overflow
  if (GIF_NO_DIM_CHECKS == 0) {
    if (ctx->fw == 0u || ctx->fh == 0u) {
      rc = GIF_ERROR_INVALID_FRAME_DIMENSIONS;
      goto out;
    }

    if (ctx->fw > (gif_u16)GIF_MAX_WIDTH) {
      rc = GIF_ERROR_UNSUPPORTED_DIMENSIONS;
      goto out;
    }

    if ((gif_u32)ctx->fx + (gif_u32)ctx->fw > ctx->canvas_w) {
      rc = GIF_ERROR_INVALID_FRAME_DIMENSIONS;
      goto out;
    }

    if ((gif_u32)ctx->fy + (gif_u32)ctx->fh > ctx->canvas_h) {
      rc = GIF_ERROR_INVALID_FRAME_DIMENSIONS;
      goto out;
    }
  }

  ctx->has_lpal = (gif_u8)((packed & 0x80u) != 0u);

  // local palette
  if (ctx->has_lpal != 0u) {
    gif_u16 n;
    gif_u8 szbits;

    szbits = (gif_u8)(packed & 0x07u);
    n = (gif_u16)(1u << ((gif_u16)szbits + 1u));

    rc = gif_read_color_table(ctx, ctx->lpal, &ctx->lpal_n, n);
    if (rc != GIF_SUCCESS) goto out;

    ctx->apal = ctx->lpal;
    ctx->apal_n = ctx->lpal_n;
  } else {
    ctx->apal = ctx->gpal;
    ctx->apal_n = ctx->gpal_n;
  }

  rc = gif_read_u8(ctx, &ctx->lzw_min_code_size);
  if (rc != GIF_SUCCESS) goto out;

  if (ctx->lzw_min_code_size > GIF_MAX_CODE_SIZE) {
    rc = GIF_ERROR_BAD_FILE;
    goto out;
  }

out:
  return rc;
}

// -----------------------------
// STREAM WALK
// -----------------------------

static void gif_restart_animation(GIF_Context *ctx)
{
  ctx->pos = ctx->anim_start_pos;

  ctx->canvas_inited = 0;

  ctx->prev_disposal = 0;
  ctx->prev_x = 0;
  ctx->prev_y = 0;
  ctx->prev_w = 0;
  ctx->prev_h = 0;

  ctx->disp3_valid = 0;
  ctx->renderer_started = 0;
}

// *find next image* ... handles extensions and trailer looping
static int gif_find_next_image(GIF_Context *ctx)
{
  int rc;
  int found;
  gif_u8 b;

  rc = GIF_SUCCESS;
  found = 0;
  b = 0;

  ctx->delay_ms = 0;
  ctx->has_transparency = 0;
  ctx->transparent_index = 0;
  ctx->disposal_method = 0;

  while (found == 0 && rc == GIF_SUCCESS) {
    rc = gif_read_u8(ctx, &b);
    if (rc != GIF_SUCCESS) goto out;

    if (b == GIF_IMAGE_SEP) {
      found = 1;
      goto out;
    } else {
      if (b == GIF_EXT_INTRO) {
        rc = gif_read_extension(ctx);
      } else {
        if (b == GIF_TRAILER) {
          if (ctx->loop_count == (gif_i16)-1) {
            gif_restart_animation(ctx);
            rc = GIF_SUCCESS;
          } else {
            if (ctx->loop_count > 0) {
              ctx->loop_count--;
              gif_restart_animation(ctx);
              rc = GIF_SUCCESS;
            } else {
              rc = GIF_ERROR_NO_FRAME;
              goto out;
            }
          }
        } else {
          rc = GIF_ERROR_BAD_FILE;
          gif_report(ctx, rc, "unexpected separator");
        }
      }
    }
  }

out:
  return rc;
}

// -----------------------------
// API
// -----------------------------

int gif_init(GIF_Context *ctx,
             const gif_u8 *data, gif_usize size,
             gif_u8 *scratch, gif_usize scratch_size)
{
  int rc;
  gif_u8 hdr[13];
  gif_u8 packed;
  gif_u16 sw;
  gif_u16 sh;
  int has_gct;
  gif_u16 gct_n;
  gif_u8 *p;

  rc = GIF_SUCCESS;

  if (ctx == 0 || data == 0 || scratch == 0) {
    rc = GIF_ERROR_INVALID_PARAM;
    goto out;
  }

  if (size < 13u) {
    rc = GIF_ERROR_EARLY_EOF;
    goto out;
  }

  if (scratch_size < GIF_SCRATCH_BUFFER_REQUIRED_SIZE) {
    rc = GIF_ERROR_BUFFER_TOO_SMALL;
    goto out;
  }

  gif_memset(ctx, 0, (gif_usize)sizeof(*ctx));

  ctx->data = data;
  ctx->size = size;
  ctx->pos = 0;

  // *default loop* ... play once
  ctx->loop_count = 0;
  ctx->loop_count_init = 0;

  // scratch layout
  p = scratch;

  // u16 align
  p = gif_align_ptr(p, 2u);
  ctx->lzw_prefix = (gif_u16 *)p;
  p += (gif_usize)GIF_SCRATCH_PREFIX_SIZE;

  ctx->lzw_suffix = (gif_u8 *)p;
  p += (gif_usize)GIF_SCRATCH_SUFFIX_SIZE;

  ctx->lzw_stack = (gif_u8 *)p;
  p += (gif_usize)GIF_SCRATCH_STACK_SIZE;

  ctx->line_idx = (gif_u8 *)p;
  p += (gif_usize)GIF_SCRATCH_LINE_SIZE;

#if (GIF_OUTPUT_FORMAT == GIF_OUTPUT_RGB565LE) && (GIF_TURBO_MAP2 != 0)
  // u32 align
  p = gif_align_ptr(p, 4u);
  ctx->map2_565 = (gif_u32 *)p;
  ctx->map2_ready = 0;
  p += (gif_usize)GIF_SCRATCH_MAP2_SIZE;
#endif

#if (GIF_LZW_MICROCACHE != 0)
  // *micro-cache arena*
  ctx->mc_arena = p;
  ctx->mc_arena_pos = 0;
  p += (gif_usize)GIF_SCRATCH_MC_ARENA_SIZE;

  // *micro-cache meta*
  p = gif_align_ptr(p, 2u);
  ctx->mc_code = (gif_u16 *)p;
  p += (gif_usize)((gif_u32)GIF_LZW_MICROCACHE_SLOTS * (gif_u32)sizeof(gif_u16));

  ctx->mc_len = (gif_u16 *)p;
  p += (gif_usize)((gif_u32)GIF_LZW_MICROCACHE_SLOTS * (gif_u32)sizeof(gif_u16));

  p = gif_align_ptr(p, 4u);
  ctx->mc_off = (gif_u32 *)p;
  p += (gif_usize)((gif_u32)GIF_LZW_MICROCACHE_SLOTS * (gif_u32)sizeof(gif_u32));

  ctx->mc_valid = (gif_u8 *)p;
  p += (gif_usize)((gif_u32)GIF_LZW_MICROCACHE_SLOTS * (gif_u32)sizeof(gif_u8));

  gif_mc_reset(ctx);
#endif

  rc = gif_read_bytes(ctx, hdr, 13u);
  if (rc != GIF_SUCCESS) goto out;

  // signature
  if (gif_memcmp(hdr + 0, (const void *)"GIF", 3u) != 0) {
    rc = GIF_ERROR_BAD_FILE;
    goto out;
  }

  // version
  if (gif_memcmp(hdr + 3, (const void *)"87a", 3u) != 0) {
    if (gif_memcmp(hdr + 3, (const void *)"89a", 3u) != 0) {
      rc = GIF_ERROR_BAD_FILE;
      goto out;
    }
  }

  sw = gif_read_u16_le(hdr + 6);
  sh = gif_read_u16_le(hdr + 8);

  ctx->canvas_w = sw;
  ctx->canvas_h = sh;

  // *width check* ... protects line buffer
  if (GIF_NO_DIM_CHECKS == 0) {
    if (ctx->canvas_w == 0u || ctx->canvas_h == 0u) {
      rc = GIF_ERROR_UNSUPPORTED_DIMENSIONS;
      goto out;
    }

    if (ctx->canvas_w > (gif_u32)GIF_MAX_WIDTH) {
      rc = GIF_ERROR_UNSUPPORTED_DIMENSIONS;
      goto out;
    }
  }

  packed = hdr[10];
  ctx->bg_index = hdr[11];

  has_gct = 0;
  if ((packed & 0x80u) != 0u) {
    has_gct = 1;
  }

  ctx->gpal_n = 0;
  ctx->apal = ctx->gpal;
  ctx->apal_n = 0;

  if (has_gct != 0) {
    gif_u8 szbits;

    szbits = (gif_u8)(packed & 0x07u);
    gct_n = (gif_u16)(1u << ((gif_u16)szbits + 1u));

    rc = gif_read_color_table(ctx, ctx->gpal, &ctx->gpal_n, gct_n);
    if (rc != GIF_SUCCESS) goto out;

    ctx->apal = ctx->gpal;
    ctx->apal_n = ctx->gpal_n;
  }

  // background rgb from gct
  ctx->bg_rgb[0] = 0;
  ctx->bg_rgb[1] = 0;
  ctx->bg_rgb[2] = 0;

  if (ctx->gpal_n != 0 && ctx->bg_index < (gif_u8)ctx->gpal_n) {
    const gif_u8 *pbg;
    pbg = ctx->gpal + (gif_usize)ctx->bg_index * 3u;
    ctx->bg_rgb[0] = pbg[0];
    ctx->bg_rgb[1] = pbg[1];
    ctx->bg_rgb[2] = pbg[2];
  }

  ctx->anim_start_pos = ctx->pos;

out:
  return rc;
}

int gif_get_info(GIF_Context *ctx, int *w, int *h)
{
  int rc;

  rc = GIF_SUCCESS;

  if (ctx == 0 || w == 0 || h == 0) {
    rc = GIF_ERROR_INVALID_PARAM;
    goto out;
  }

  *w = (int)ctx->canvas_w;
  *h = (int)ctx->canvas_h;

out:
  return rc;
}

int gif_set_error_callback(GIF_Context *ctx, GIF_ErrorCallback cb)
{
  int rc;

  rc = GIF_SUCCESS;

  if (ctx == 0) {
    rc = GIF_ERROR_INVALID_PARAM;
    goto out;
  }

  ctx->on_error = cb;

out:
  return rc;
}

int gif_set_disposal3_buffer(GIF_Context *ctx, void *buf, gif_usize size)
{
  int rc;

  rc = GIF_SUCCESS;

  if (ctx == 0) {
    rc = GIF_ERROR_INVALID_PARAM;
    goto out;
  }

  ctx->disp3_buf = (gif_u8 *)buf;
  ctx->disp3_size = size;
  ctx->disp3_valid = 0;

out:
  return rc;
}

int gif_rewind(GIF_Context *ctx)
{
  int rc;

  rc = GIF_SUCCESS;

  if (ctx == 0) {
    rc = GIF_ERROR_INVALID_PARAM;
    goto out;
  }

  ctx->pos = ctx->anim_start_pos;
  ctx->loop_count = ctx->loop_count_init;

  ctx->canvas_inited = 0;

  ctx->prev_disposal = 0;
  ctx->prev_x = 0;
  ctx->prev_y = 0;
  ctx->prev_w = 0;
  ctx->prev_h = 0;

  ctx->disp3_valid = 0;
  ctx->renderer_started = 0;

out:
  return rc;
}

int gif_close(GIF_Context *ctx)
{
  int rc;

  rc = GIF_SUCCESS;

  if (ctx == 0) {
    rc = GIF_ERROR_INVALID_PARAM;
    goto out;
  }

  gif_memset(ctx, 0, (gif_usize)sizeof(*ctx));

out:
  return rc;
}

int gif_next_frame(GIF_Context *ctx, void *canvas, int *delay_ms)
{
  int rc;

  rc = GIF_SUCCESS;

  if (ctx == 0 || canvas == 0 || delay_ms == 0) {
    rc = GIF_ERROR_INVALID_PARAM;
    goto out;
  }

  // apply previous disposal before reading new frame
  gif_apply_prev_disposal(ctx, canvas);

  rc = gif_find_next_image(ctx);
  if (rc != GIF_SUCCESS) goto out;

  rc = gif_read_image_descriptor(ctx);
  if (rc != GIF_SUCCESS) goto out;

  // init canvas once
  if (ctx->canvas_inited == 0) {
    gif_canvas_clear(ctx, canvas);
    ctx->canvas_inited = 1;
  }

  // cache palette to output format
  gif_prepare_palette_cache(ctx);

  // backup for disposal 3
  rc = gif_backup_rect_if_needed(ctx, canvas);
  if (rc != GIF_SUCCESS) goto out;

  // decode + render
#if (GIF_LZW_BACKEND == GIF_LZW_TURBO_UNSAFE)
  rc = gif_decode_image_data_unsafe(ctx, canvas, (const GIF_Renderer *)0);
#else
  rc = gif_decode_image_data_safe(ctx, canvas, (const GIF_Renderer *)0);
#endif
  if (rc != GIF_SUCCESS) goto out;

  // remember for next disposal
  ctx->prev_disposal = ctx->disposal_method;
  ctx->prev_x = ctx->fx;
  ctx->prev_y = ctx->fy;
  ctx->prev_w = ctx->fw;
  ctx->prev_h = ctx->fh;

  *delay_ms = (int)ctx->delay_ms;

out:
  return rc;
}

int gif_next_frame_rect(GIF_Context *ctx, void *canvas, int *delay_ms,
                        int *x, int *y, int *w, int *h)
{
  int rc;

  rc = GIF_SUCCESS;

  if (ctx == 0 || canvas == 0 || delay_ms == 0 || x == 0 || y == 0 || w == 0 || h == 0) {
    rc = GIF_ERROR_INVALID_PARAM;
    goto out;
  }

  rc = gif_next_frame(ctx, canvas, delay_ms);
  if (rc != GIF_SUCCESS) goto out;

  *x = (int)ctx->fx;
  *y = (int)ctx->fy;
  *w = (int)ctx->fw;
  *h = (int)ctx->fh;

out:
  return rc;
}

int gif_next_frame_render(GIF_Context *ctx, const GIF_Renderer *r, int *delay_ms)
{
  int rc;

  rc = GIF_SUCCESS;

  if (ctx == 0 || r == 0 || delay_ms == 0) {
    rc = GIF_ERROR_INVALID_PARAM;
    goto out;
  }

  // renderer begin once
  if (ctx->renderer_started == 0) {
    if (r->begin != 0) {
      r->begin(r->user, (int)ctx->canvas_w, (int)ctx->canvas_h);
    }
    ctx->renderer_started = 1;
  }

  rc = gif_find_next_image(ctx);
  if (rc != GIF_SUCCESS) goto out;

  rc = gif_read_image_descriptor(ctx);
  if (rc != GIF_SUCCESS) goto out;

  gif_prepare_palette_cache(ctx);

#if (GIF_LZW_BACKEND == GIF_LZW_TURBO_UNSAFE)
  rc = gif_decode_image_data_unsafe(ctx, (void *)0, r);
#else
  rc = gif_decode_image_data_safe(ctx, (void *)0, r);
#endif
  if (rc != GIF_SUCCESS) goto out;

  *delay_ms = (int)ctx->delay_ms;

  if (r->end != 0) {
    r->end(r->user, *delay_ms);
  }

out:
  return rc;
}

// -----------------------------
// COMPAT HELPERS
// -----------------------------

int gif_next_frame_compat(GIF_Context *ctx, void *canvas, int *delay_ms)
{
  int rc;
  int r;

  rc = gif_next_frame(ctx, canvas, delay_ms);
  r = -1;

  if (rc == GIF_SUCCESS) {
    r = 1;
  } else {
    if (rc == GIF_ERROR_NO_FRAME) {
      r = 0;
    }
  }

  return r;
}

void gif_rewind_compat(GIF_Context *ctx)
{
  (void)gif_rewind(ctx);
}

void gif_close_compat(GIF_Context *ctx)
{
  (void)gif_close(ctx);
}

#endif // GIF_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#if defined(GIF_USE_PREFIX)
// PREFIX MODE CLEANUP
#undef GIF_Context
#undef GIF_Pump
#undef gif_align_ptr
#undef gif_apply_prev_disposal
#undef gif_backup_rect_if_needed
#undef gif_blit_map2_565_row
#undef gif_blit_plain_row
#undef gif_blit_turbo_row
#undef gif_build_map2_565
#undef gif_canvas_clear
#undef gif_close
#undef gif_close_compat
#undef gif_decode_image_data_safe
#undef gif_decode_image_data_unsafe
#undef gif_discard_sub_blocks
#undef gif_emit_row
#undef gif_emit_row_canvas
#undef gif_emit_row_renderer
#undef gif_error_strings
#undef gif_fill_rect_bg
#undef gif_find_next_image
#undef gif_get_error_string
#undef gif_get_info
#undef gif_get_required_scratch_size
#undef gif_init
#undef gif_is_netscape_id
#undef gif_lzw_discard_rest
#undef gif_lzw_expand_to_stack_safe
#undef gif_lzw_expand_to_stack_unsafe
#undef gif_lzw_read_code
#undef gif_lzw_stream_begin
#undef gif_lzw_stream_read_byte
#undef gif_map_interlace_y
#undef gif_mc_insert
#undef gif_mc_lookup
#undef gif_mc_reset
#undef gif_mc_slot
#undef gif_memcmp
#undef gif_memcpy
#undef gif_memset
#undef gif_next_frame
#undef gif_next_frame_compat
#undef gif_next_frame_rect
#undef gif_next_frame_rect_ex
#undef gif_next_frame_render
#undef gif_prepare_palette_cache
#undef gif_pump_push_span
#undef gif_read_app_ext
#undef gif_read_bytes
#undef gif_read_color_table
#undef gif_read_extension
#undef gif_read_gce
#undef gif_read_image_descriptor
#undef gif_read_u16_le
#undef gif_read_u8
#undef gif_report
#undef gif_restart_animation
#undef gif_restore_backup_if_needed
#undef gif_rewind
#undef gif_rewind_compat
#undef gif_rgb_to_565
#undef gif_set_disposal3_buffer
#undef gif_set_error_callback
#undef gif_skip
#undef gif_write_rgb_pixel
#undef GIF_SYM
#undef GIF_CAT
#undef GIF_CAT2
#endif

#endif /* GIF_H || GIF_USE_PREFIX */
