#include "watermark.h"

#include <string.h>

#include <drm_fourcc.h>
#include <wlr/interfaces/wlr_buffer.h>

/* --- FNV-1a 64-bit hash --- */

static uint64_t fnv1a_64(const uint8_t* data, size_t len) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= data[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

void dlp_watermark_set_identity(struct dlp_watermark_state* state, const char* identity) {
    if (!state || !identity) {
        return;
    }
    size_t len = strlen(identity);
    /* Generate 256 bits by hashing with 4 different prefixes */
    for (int i = 0; i < 4; i++) {
        uint8_t buf[256];
        buf[0] = (uint8_t)i;
        size_t copy = len < 255 ? len : 255;
        memcpy(buf + 1, identity, copy);
        uint64_t h = fnv1a_64(buf, copy + 1);
        memcpy(state->fingerprint + i * 8, &h, 8);
    }
    state->has_identity = true;
}

/* --- xorshift128 PRNG --- */

struct xorshift128 {
    uint32_t s[4];
};

static uint32_t xorshift128_next(struct xorshift128* rng) {
    uint32_t t = rng->s[3];
    t ^= t << 11;
    t ^= t >> 8;
    rng->s[3] = rng->s[2];
    rng->s[2] = rng->s[1];
    rng->s[1] = rng->s[0];
    t ^= rng->s[0];
    t ^= rng->s[0] >> 19;
    rng->s[0] = t;
    return t;
}

/* Byte offset of blue channel for supported DRM formats.
 * ARGB8888/XRGB8888 layout: [B G R A] in memory (little-endian).
 * ABGR8888/XBGR8888 layout: [R G B A] in memory (little-endian). */
static int blue_offset(uint32_t fmt) {
    switch (fmt) {
        case DRM_FORMAT_ARGB8888:
        case DRM_FORMAT_XRGB8888:
            return 0;
        case DRM_FORMAT_ABGR8888:
        case DRM_FORMAT_XBGR8888:
            return 2;
        default:
            return -1;
    }
}

bool dlp_watermark_apply(struct wlr_buffer* buffer, const struct dlp_watermark_state* state) {
    if (!buffer || !state || !state->has_identity) {
        return false;
    }

    void* data;
    uint32_t format;
    size_t stride;
    if (!wlr_buffer_begin_data_ptr_access(
            buffer, WLR_BUFFER_DATA_PTR_ACCESS_READ | WLR_BUFFER_DATA_PTR_ACCESS_WRITE, &data,
            &format, &stride)) {
        return false;
    }

    int boff = blue_offset(format);
    if (boff < 0) {
        wlr_buffer_end_data_ptr_access(buffer);
        return false;
    }

    int width = buffer->width;
    int height = buffer->height;

    /* Seed PRNG from fingerprint */
    struct xorshift128 rng;
    memcpy(rng.s, state->fingerprint, 16);
    /* Ensure non-zero state */
    if (!rng.s[0] && !rng.s[1] && !rng.s[2] && !rng.s[3]) {
        rng.s[0] = 1;
    }

    int bit_idx = 0;
    uint8_t* pixels = data;
    for (int y = 0; y < height; y++) {
        uint8_t* row = pixels + y * stride;
        for (int x = 0; x < width; x++) {
            uint32_t r = xorshift128_next(&rng);
            if ((r & 0xF) != 0) {
                continue; /* ~1/16 pixel density */
            }

            int fp_bit = (state->fingerprint[bit_idx / 8] >> (bit_idx % 8)) & 1;
            bit_idx = (bit_idx + 1) % 256;

            uint8_t* pixel = row + x * 4;
            pixel[boff] = (pixel[boff] & 0xFE) | fp_bit;
        }
    }

    wlr_buffer_end_data_ptr_access(buffer);
    return true;
}
