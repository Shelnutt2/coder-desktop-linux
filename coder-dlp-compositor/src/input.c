#include "compositor_internal.h"

#include <stdlib.h>

#include <wlr/config.h>
#if WLR_HAS_X11_BACKEND
#include <wlr/backend/x11.h>
#include <wlr/xwayland/xwayland.h>
#endif
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

/* --- Keyboard handling ---
 *
 * Known limitation (X11 backend): when the compositor runs inside an X11
 * window (e.g. under i3/Sway on Xorg), the host window manager's key
 * grabs (Mod+Enter, Mod+d, etc.) should intercept key events before they
 * reach the DLP window.  In practice this works for most WMs because they
 * install passive grabs on the root window with GrabModeSync.  However,
 * some modifier-only shortcuts may still leak into the compositor because
 * the X11 backend receives all key events for its focused window.  This
 * is a fundamental X11 limitation — Wayland sessions are not affected
 * because the host compositor controls keyboard focus natively. */

static void handle_keyboard_key(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, keyboard_key);
    struct wlr_keyboard_key_event* event = data;
    struct wlr_keyboard* keyboard = comp->keyboard;

    /* On X11 backend: suppress key events when Super (Mod4) is held.
     * Host WM keybindings using Super may leak through the X11 backend
     * window — suppressing them here prevents dual-action in both the
     * host WM and the sandboxed app.  Wayland sessions are unaffected. */
#if WLR_HAS_X11_BACKEND
    if (comp->is_x11_backend) {
        uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard);
        if (modifiers & WLR_MODIFIER_LOGO) {
            return;
        }
    }
#endif

    /* Only forward key events when a client surface has keyboard focus.
     * This prevents stale key events from being sent to the seat when
     * no Wayland client is active (e.g. compositor just started, all
     * windows closed). */
    wlr_seat_set_keyboard(comp->seat, keyboard);
    if (comp->seat->keyboard_state.focused_surface) {
        wlr_seat_keyboard_notify_key(comp->seat, event->time_msec, event->keycode, event->state);
    }
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

/* --- Pointer / cursor handling --- */

static void process_cursor_motion(struct coder_dlp_compositor* comp, uint32_t time) {
    double sx, sy;
    struct wlr_scene_node* node =
        wlr_scene_node_at(&comp->scene->tree.node, comp->cursor->x, comp->cursor->y, &sx, &sy);

    if (node && node->type == WLR_SCENE_NODE_BUFFER) {
        struct wlr_scene_buffer* buffer = wlr_scene_buffer_from_node(node);
        struct wlr_scene_surface* scene_surface = wlr_scene_surface_try_from_buffer(buffer);
        if (scene_surface) {
            wlr_seat_pointer_notify_enter(comp->seat, scene_surface->surface, sx, sy);
            wlr_seat_pointer_notify_motion(comp->seat, time, sx, sy);
            return;
        }
    }
    wlr_cursor_set_xcursor(comp->cursor, comp->cursor_mgr, "default");
    wlr_seat_pointer_clear_focus(comp->seat);
}

void handle_cursor_motion(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, cursor_motion);
    struct wlr_pointer_motion_event* event = data;
    wlr_cursor_move(comp->cursor, &event->pointer->base, event->delta_x, event->delta_y);
    process_cursor_motion(comp, event->time_msec);
}

void handle_cursor_motion_absolute(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, cursor_motion_absolute);
    struct wlr_pointer_motion_absolute_event* event = data;
    wlr_cursor_warp_absolute(comp->cursor, &event->pointer->base, event->x, event->y);
    process_cursor_motion(comp, event->time_msec);
}

void handle_cursor_button(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, cursor_button);
    struct wlr_pointer_button_event* event = data;

    wlr_seat_pointer_notify_button(comp->seat, event->time_msec, event->button, event->state);

    if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
        double sx, sy;
        struct wlr_scene_node* node =
            wlr_scene_node_at(&comp->scene->tree.node, comp->cursor->x, comp->cursor->y, &sx, &sy);
        if (node) {
            struct wlr_scene_tree* tree = node->parent;
            while (tree && !tree->node.data) {
                tree = tree->node.parent;
            }
            if (tree && tree->node.data) {
                enum dlp_surface_type* type = tree->node.data;

                /* Deactivate previously focused toplevel */
                struct wlr_surface* prev_surface = comp->seat->keyboard_state.focused_surface;
                if (prev_surface) {
                    struct wlr_xdg_toplevel* prev =
                        wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
                    if (prev) {
                        wlr_xdg_toplevel_set_activated(prev, false);
                    }
#if WLR_HAS_X11_BACKEND
                    else {
                        struct wlr_xwayland_surface* prev_xsurface =
                            wlr_xwayland_surface_try_from_wlr_surface(prev_surface);
                        if (prev_xsurface) {
                            wlr_xwayland_surface_activate(prev_xsurface, false);
                        }
                    }
#endif
                }

                if (*type == DLP_SURFACE_XDG) {
                    struct coder_dlp_toplevel* toplevel = tree->node.data;
                    wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
                    wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);
                    struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(comp->seat);
                    if (keyboard) {
                        wlr_seat_keyboard_notify_enter(
                            comp->seat, toplevel->xdg_toplevel->base->surface, keyboard->keycodes,
                            keyboard->num_keycodes, &keyboard->modifiers);
                    }
                }
#if WLR_HAS_X11_BACKEND
                else if (*type == DLP_SURFACE_XWAYLAND) {
                    struct coder_dlp_xwayland_surface* xsurf = tree->node.data;
                    wlr_scene_node_raise_to_top(&xsurf->scene_tree->node);
                    struct wlr_xwayland_surface* xsurface = xsurf->xwayland_surface;
                    wlr_xwayland_surface_activate(xsurface, true);
                    if (xsurface->surface) {
                        struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(comp->seat);
                        if (keyboard) {
                            wlr_seat_keyboard_notify_enter(
                                comp->seat, xsurface->surface, keyboard->keycodes,
                                keyboard->num_keycodes, &keyboard->modifiers);
                        }
                    }
                }
#endif
            }
        }
    }
}

void handle_cursor_axis(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, cursor_axis);
    struct wlr_pointer_axis_event* event = data;
    wlr_seat_pointer_notify_axis(comp->seat, event->time_msec, event->orientation, event->delta,
                                 event->delta_discrete, event->source, event->relative_direction);
}

void handle_cursor_frame(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, cursor_frame);
    (void)data;
    wlr_seat_pointer_notify_frame(comp->seat);
}

void handle_request_set_cursor(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, request_set_cursor);
    struct wlr_seat_pointer_request_set_cursor_event* event = data;
    struct wlr_seat_client* focused = comp->seat->pointer_state.focused_client;
    if (focused == event->seat_client) {
        wlr_cursor_set_surface(comp->cursor, event->surface, event->hotspot_x, event->hotspot_y);
    }
}

static void setup_pointer(struct coder_dlp_compositor* comp, struct wlr_input_device* device) {
    wlr_cursor_attach_input_device(comp->cursor, device);
    wlr_log(WLR_INFO, "pointer configured");
}

/* --- Touch handling --- */

/* Helper: convert absolute touch coordinates (0.0–1.0) to layout-space
 * coordinates using the first output's dimensions. */
static void touch_coords_to_layout(struct coder_dlp_compositor* comp, double touch_x,
                                   double touch_y, double* lx, double* ly) {
    if (comp->output) {
        *lx = touch_x * comp->output->width;
        *ly = touch_y * comp->output->height;
    } else {
        *lx = touch_x;
        *ly = touch_y;
    }
}

/* Focus a toplevel under the given layout coordinates (same logic as cursor
 * button press). */
static void touch_focus_at(struct coder_dlp_compositor* comp, double lx, double ly) {
    double sx, sy;
    struct wlr_scene_node* node = wlr_scene_node_at(&comp->scene->tree.node, lx, ly, &sx, &sy);
    if (!node) {
        return;
    }
    struct wlr_scene_tree* tree = node->parent;
    while (tree && !tree->node.data) {
        tree = tree->node.parent;
    }
    if (tree && tree->node.data) {
        enum dlp_surface_type* type = tree->node.data;

        /* Deactivate previously focused toplevel */
        struct wlr_surface* prev_surface = comp->seat->keyboard_state.focused_surface;
        if (prev_surface) {
            struct wlr_xdg_toplevel* prev = wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
            if (prev) {
                wlr_xdg_toplevel_set_activated(prev, false);
            }
#if WLR_HAS_X11_BACKEND
            else {
                struct wlr_xwayland_surface* prev_xsurface =
                    wlr_xwayland_surface_try_from_wlr_surface(prev_surface);
                if (prev_xsurface) {
                    wlr_xwayland_surface_activate(prev_xsurface, false);
                }
            }
#endif
        }

        if (*type == DLP_SURFACE_XDG) {
            struct coder_dlp_toplevel* toplevel = tree->node.data;
            wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
            wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);
            struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(comp->seat);
            if (keyboard) {
                wlr_seat_keyboard_notify_enter(comp->seat, toplevel->xdg_toplevel->base->surface,
                                               keyboard->keycodes, keyboard->num_keycodes,
                                               &keyboard->modifiers);
            }
        }
#if WLR_HAS_X11_BACKEND
        else if (*type == DLP_SURFACE_XWAYLAND) {
            struct coder_dlp_xwayland_surface* xsurf = tree->node.data;
            wlr_scene_node_raise_to_top(&xsurf->scene_tree->node);
            struct wlr_xwayland_surface* xsurface = xsurf->xwayland_surface;
            wlr_xwayland_surface_activate(xsurface, true);
            if (xsurface->surface) {
                struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(comp->seat);
                if (keyboard) {
                    wlr_seat_keyboard_notify_enter(comp->seat, xsurface->surface,
                                                   keyboard->keycodes, keyboard->num_keycodes,
                                                   &keyboard->modifiers);
                }
            }
        }
#endif
    }
}

void handle_touch_down(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, touch_down);
    struct wlr_touch_down_event* event = data;

    double lx, ly;
    touch_coords_to_layout(comp, event->x, event->y, &lx, &ly);

    /* Click-to-focus */
    touch_focus_at(comp, lx, ly);

    /* Find surface under touch point */
    double sx, sy;
    struct wlr_scene_node* node = wlr_scene_node_at(&comp->scene->tree.node, lx, ly, &sx, &sy);
    if (node && node->type == WLR_SCENE_NODE_BUFFER) {
        struct wlr_scene_buffer* buffer = wlr_scene_buffer_from_node(node);
        struct wlr_scene_surface* scene_surface = wlr_scene_surface_try_from_buffer(buffer);
        if (scene_surface) {
            wlr_seat_touch_notify_down(comp->seat, scene_surface->surface, event->time_msec,
                                       event->touch_id, sx, sy);
            return;
        }
    }
}

void handle_touch_up(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, touch_up);
    struct wlr_touch_up_event* event = data;
    wlr_seat_touch_notify_up(comp->seat, event->time_msec, event->touch_id);
}

void handle_touch_motion(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, touch_motion);
    struct wlr_touch_motion_event* event = data;

    double lx, ly;
    touch_coords_to_layout(comp, event->x, event->y, &lx, &ly);

    double sx, sy;
    struct wlr_scene_node* node = wlr_scene_node_at(&comp->scene->tree.node, lx, ly, &sx, &sy);
    if (node && node->type == WLR_SCENE_NODE_BUFFER) {
        struct wlr_scene_buffer* buffer = wlr_scene_buffer_from_node(node);
        struct wlr_scene_surface* scene_surface = wlr_scene_surface_try_from_buffer(buffer);
        if (scene_surface) {
            wlr_seat_touch_notify_motion(comp->seat, event->time_msec, event->touch_id, sx, sy);
            return;
        }
    }
}

void handle_touch_cancel(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, touch_cancel);
    (void)data;

    /* Cancel all active touch points.  Iterate the touch point list and
     * notify each unique client.  wlr_seat_touch_notify_cancel() takes a
     * seat_client (not a surface) in wlroots 0.19. */
    struct wlr_touch_point* point;
    wl_list_for_each(point, &comp->seat->touch_state.touch_points, link) {
        if (point->client) {
            wlr_seat_touch_notify_cancel(comp->seat, point->client);
        }
    }
}

void handle_touch_frame(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, touch_frame);
    (void)data;
    wlr_seat_touch_notify_frame(comp->seat);
}

static void setup_touch(struct coder_dlp_compositor* comp, struct wlr_input_device* device) {
    struct wlr_touch* touch = wlr_touch_from_input_device(device);
    comp->touch = touch;

    /* Attach touch device to cursor (aggregates all input devices) */
    wlr_cursor_attach_input_device(comp->cursor, device);

    /* Wire listeners through the compositor struct for wl_container_of */
    comp->touch_down.notify = handle_touch_down;
    wl_signal_add(&touch->events.down, &comp->touch_down);

    comp->touch_up.notify = handle_touch_up;
    wl_signal_add(&touch->events.up, &comp->touch_up);

    comp->touch_motion.notify = handle_touch_motion;
    wl_signal_add(&touch->events.motion, &comp->touch_motion);

    comp->touch_cancel.notify = handle_touch_cancel;
    wl_signal_add(&touch->events.cancel, &comp->touch_cancel);

    comp->touch_frame.notify = handle_touch_frame;
    wl_signal_add(&touch->events.frame, &comp->touch_frame);

    wlr_log(WLR_INFO, "touch device configured");
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
        case WLR_INPUT_DEVICE_TOUCH:
            setup_touch(comp, device);
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
    if (device->type == WLR_INPUT_DEVICE_TOUCH) {
        caps |= WL_SEAT_CAPABILITY_TOUCH;
    }
    wlr_seat_set_capabilities(comp->seat, caps);
}
