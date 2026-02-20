#ifndef DLP_WATERMARK_H
#define DLP_WATERMARK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct wlr_buffer;

/* Watermark state — embedded in coder_dlp_compositor */
struct dlp_watermark_state {
    bool enabled;
    uint8_t fingerprint[32]; /* FNV-1a based 256-bit fingerprint */
    bool has_identity;
};

/* Compute fingerprint from identity string, store in state */
void dlp_watermark_set_identity(struct dlp_watermark_state* state, const char* identity);

/* Apply watermark to a wlr_buffer (modifies blue channel LSBs).
 * Returns true on success, false if buffer format unsupported. */
bool dlp_watermark_apply(struct wlr_buffer* buffer, const struct dlp_watermark_state* state);

#endif /* DLP_WATERMARK_H */
