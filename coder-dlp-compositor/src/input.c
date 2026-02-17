#include "compositor_internal.h"

#include <stdlib.h>

#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

/* --- Keyboard handling --- */

static void handle_keyboard_key(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, keyboard_key);
    struct wlr_keyboard_key_event* event = data;
    struct wlr_keyboard* keyboard = comp->keyboard;

    wlr_seat_set_keyboard(comp->seat, keyboard);
    wlr_seat_keyboard_notify_key(comp->seat, event->time_msec, event->keycode, event->state);
}

static void handle_keyboard_modifiers(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, keyboard_modifiers);
    struct wlr_keyboard* keyboard = comp->keyboard;

    wlr_seat_set_keyboard(comp->seat, keyboard);
    wlr_seat_keyboard_notify_modifiers(comp->seat, &keyboard->modifiers);
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

    /* Store keyboard on comp and wire listeners through the struct so
     * wl_container_of can recover comp in the handlers. */
    comp->keyboard = keyboard;

    comp->keyboard_key.notify = handle_keyboard_key;
    wl_signal_add(&keyboard->events.key, &comp->keyboard_key);

    comp->keyboard_modifiers.notify = handle_keyboard_modifiers;
    wl_signal_add(&keyboard->events.modifiers, &comp->keyboard_modifiers);

    wlr_seat_set_keyboard(comp->seat, keyboard);
    wlr_log(WLR_INFO, "keyboard configured");
}

/* --- Pointer handling --- */

static void handle_pointer_motion(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, pointer_motion);
    struct wlr_pointer_motion_event* event = data;

    /* Accumulate delta into absolute cursor position */
    comp->cursor_x += event->delta_x;
    comp->cursor_y += event->delta_y;

    /* Clamp to output bounds */
    if (comp->output) {
        int width, height;
        wlr_output_effective_resolution(comp->output, &width, &height);
        if (comp->cursor_x < 0) comp->cursor_x = 0;
        if (comp->cursor_y < 0) comp->cursor_y = 0;
        if (comp->cursor_x >= width) comp->cursor_x = width - 1;
        if (comp->cursor_y >= height) comp->cursor_y = height - 1;
    }

    /* Find the surface under the cursor via the scene graph */
    double sx, sy;
    struct wlr_scene_node* node =
        wlr_scene_node_at(&comp->scene->tree.node, comp->cursor_x, comp->cursor_y, &sx, &sy);

    if (node && node->type == WLR_SCENE_NODE_BUFFER) {
        struct wlr_scene_buffer* buffer = wlr_scene_buffer_from_node(node);
        struct wlr_scene_surface* scene_surface = wlr_scene_surface_try_from_buffer(buffer);
        if (scene_surface) {
            wlr_seat_pointer_notify_enter(comp->seat, scene_surface->surface, sx, sy);
            wlr_seat_pointer_notify_motion(comp->seat, event->time_msec, sx, sy);
        }
    } else {
        wlr_seat_pointer_clear_focus(comp->seat);
    }
}

static void handle_pointer_button(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, pointer_button);
    struct wlr_pointer_button_event* event = data;

    wlr_seat_pointer_notify_button(comp->seat, event->time_msec, event->button, event->state);

    /* On click, focus the toplevel under the cursor */
    if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
        double sx, sy;
        struct wlr_scene_node* node =
            wlr_scene_node_at(&comp->scene->tree.node, comp->cursor_x, comp->cursor_y, &sx, &sy);
        if (node) {
            /* Walk up the scene tree to find the toplevel */
            struct wlr_scene_tree* tree = node->parent;
            while (tree && !tree->node.data) {
                tree = tree->node.parent;
            }
            if (tree && tree->node.data) {
                struct coder_dlp_toplevel* toplevel = tree->node.data;
                wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
                struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(comp->seat);
                if (keyboard) {
                    wlr_seat_keyboard_notify_enter(
                        comp->seat, toplevel->xdg_toplevel->base->surface, keyboard->keycodes,
                        keyboard->num_keycodes, &keyboard->modifiers);
                }
            }
        }
    }
}

static void handle_pointer_axis(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, pointer_axis);
    struct wlr_pointer_axis_event* event = data;

    wlr_seat_pointer_notify_axis(comp->seat, event->time_msec, event->orientation, event->delta,
                                 event->delta_discrete, event->source, event->relative_direction);
}

static void handle_pointer_frame(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, pointer_frame);
    (void)data;

    wlr_seat_pointer_notify_frame(comp->seat);
}

static void setup_pointer(struct coder_dlp_compositor* comp, struct wlr_input_device* device) {
    struct wlr_pointer* pointer = wlr_pointer_from_input_device(device);

    /* Wire pointer event listeners through the struct */
    comp->pointer_motion.notify = handle_pointer_motion;
    wl_signal_add(&pointer->events.motion, &comp->pointer_motion);

    comp->pointer_button.notify = handle_pointer_button;
    wl_signal_add(&pointer->events.button, &comp->pointer_button);

    comp->pointer_axis.notify = handle_pointer_axis;
    wl_signal_add(&pointer->events.axis, &comp->pointer_axis);

    comp->pointer_frame.notify = handle_pointer_frame;
    wl_signal_add(&pointer->events.frame, &comp->pointer_frame);

    wlr_log(WLR_INFO, "pointer configured");
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
