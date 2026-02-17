#include "compositor_internal.h"

#include <stdlib.h>

#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

/* --- Keyboard handling --- */

static void handle_keyboard_key(struct wl_listener* listener, void* data) {
    (void)listener;
    /* The seat automatically forwards key events to the focused client when
     * a keyboard is attached via wlr_seat_set_keyboard(). */
    (void)data;
}

static void handle_keyboard_modifiers(struct wl_listener* listener, void* data) {
    (void)listener;
    (void)data;
}

static void setup_keyboard(struct coder_dlp_compositor* comp, struct wlr_input_device* device) {
    struct wlr_keyboard* keyboard = wlr_keyboard_from_input_device(device);

    /* Set up XKB keymap from environment defaults */
    struct xkb_context* ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!ctx) {
        wlr_log(WLR_ERROR, "failed to create xkb_context");
        return;
    }
    struct xkb_keymap* keymap = xkb_keymap_new_from_names(ctx, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!keymap) {
        wlr_log(WLR_ERROR, "failed to create xkb_keymap");
        xkb_context_unref(ctx);
        return;
    }

    wlr_keyboard_set_keymap(keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(ctx);

    wlr_keyboard_set_repeat_info(keyboard, 25, 600);

    /* Wire key/modifier listeners (currently no-ops — seat forwarding is
     * automatic once the keyboard is set on the seat). */
    static struct wl_listener key_listener = {.notify = handle_keyboard_key};
    static struct wl_listener mod_listener = {.notify = handle_keyboard_modifiers};
    wl_signal_add(&keyboard->events.key, &key_listener);
    wl_signal_add(&keyboard->events.modifiers, &mod_listener);

    wlr_seat_set_keyboard(comp->seat, keyboard);
    wlr_log(WLR_INFO, "keyboard configured");
}

/* --- Pointer handling --- */

static void handle_pointer_motion(struct wl_listener* listener, void* data) {
    (void)listener;
    (void)data;
    /* Pointer motion is forwarded automatically by the seat when a pointer
     * capability is advertised.  Explicit notify calls are only needed for
     * compositors that do cursor-image management, which the nested
     * compositor delegates to the parent. */
}

static void handle_pointer_button(struct wl_listener* listener, void* data) {
    (void)listener;
    (void)data;
}

static void handle_pointer_axis(struct wl_listener* listener, void* data) {
    (void)listener;
    (void)data;
}

static void handle_pointer_frame(struct wl_listener* listener, void* data) {
    (void)listener;
    (void)data;
}

static void setup_pointer(struct coder_dlp_compositor* comp, struct wlr_input_device* device) {
    struct wlr_pointer* pointer = wlr_pointer_from_input_device(device);

    /* Wire pointer event listeners */
    static struct wl_listener motion_listener = {.notify = handle_pointer_motion};
    static struct wl_listener button_listener = {.notify = handle_pointer_button};
    static struct wl_listener axis_listener = {.notify = handle_pointer_axis};
    static struct wl_listener frame_listener = {.notify = handle_pointer_frame};

    wl_signal_add(&pointer->events.motion, &motion_listener);
    wl_signal_add(&pointer->events.button, &button_listener);
    wl_signal_add(&pointer->events.axis, &axis_listener);
    wl_signal_add(&pointer->events.frame, &frame_listener);

    wlr_log(WLR_INFO, "pointer configured");

    (void)comp; /* seat capabilities updated below in new_input handler */
}

/* --- Backend new_input handler --- */

void compositor_handle_new_input(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, new_input);
    struct wlr_input_device* device = data;

    switch (device->type) {
        case WLR_INPUT_DEVICE_KEYBOARD:
            setup_keyboard(comp, device);
            break;
        case WLR_INPUT_DEVICE_POINTER:
            setup_pointer(comp, device);
            break;
        default:
            wlr_log(WLR_DEBUG, "unhandled input device type: %d", device->type);
            break;
    }

    /* Update seat capabilities — accumulate from the current set plus the
     * newly added device. */
    uint32_t caps = comp->seat->capabilities;
    if (device->type == WLR_INPUT_DEVICE_KEYBOARD) {
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    if (device->type == WLR_INPUT_DEVICE_POINTER) {
        caps |= WL_SEAT_CAPABILITY_POINTER;
    }
    wlr_seat_set_capabilities(comp->seat, caps);
}
