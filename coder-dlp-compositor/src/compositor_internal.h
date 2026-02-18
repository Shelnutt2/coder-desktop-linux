#ifndef CODER_DLP_COMPOSITOR_INTERNAL_H
#define CODER_DLP_COMPOSITOR_INTERNAL_H

#include "coder_dlp.h"

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_security_context_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>

struct coder_dlp_toplevel {
    struct wlr_xdg_toplevel* xdg_toplevel;
    struct coder_dlp_compositor* compositor;
    struct wlr_scene_tree* scene_tree;

    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener commit;
    struct wl_listener destroy;
    struct wl_listener request_move;
    struct wl_listener request_resize;

    struct wl_list link; /* coder_dlp_compositor.toplevels */
};

struct coder_dlp_compositor {
    coder_dlp_policy policy;

    /* Surface callback */
    coder_dlp_surface_cb surface_cb;
    void* surface_cb_data;

    /* Log callback */
    coder_dlp_log_cb log_cb;
    void* log_cb_data;

    /* Wayland core */
    struct wl_display* wl_display;
    struct wl_event_loop* wl_event_loop;

    /* wlroots objects */
    struct wlr_backend* backend;
    struct wlr_renderer* renderer;
    struct wlr_allocator* allocator;
    struct wlr_compositor* wlr_compositor;
    struct wlr_subcompositor* subcompositor;
    struct wlr_output_layout* output_layout;
    struct wlr_scene* scene;
    struct wlr_scene_output_layout* scene_layout;
    struct wlr_xdg_shell* xdg_shell;
    struct wlr_data_device_manager* data_device_mgr;

    /* Output */
    struct wlr_output* output;
    struct wl_listener output_frame;
    struct wl_listener output_request_state;
    struct wl_listener output_destroy;

    /* Shell */
    struct wl_listener new_xdg_toplevel;
    struct wl_listener new_xdg_popup;
    struct wl_list toplevels; /* coder_dlp_toplevel.link */

    /* Backend */
    struct wl_listener new_output;

    /* Input */
    struct wl_listener new_input;

    /* Keyboard */
    struct wlr_keyboard* keyboard;
    struct wl_listener keyboard_key;
    struct wl_listener keyboard_modifiers;

    /* Cursor */
    struct wlr_cursor* cursor;
    struct wlr_xcursor_manager* cursor_mgr;
    struct wl_listener cursor_motion;
    struct wl_listener cursor_motion_absolute;
    struct wl_listener cursor_button;
    struct wl_listener cursor_axis;
    struct wl_listener cursor_frame;
    struct wl_listener request_set_cursor;

    /* Seat (keyboard/pointer/clipboard) */
    struct wlr_seat* seat;

    /* Clipboard mediation */
    struct wl_listener request_set_selection;
    struct wl_listener request_set_primary_selection;

    /* Security context */
    struct wlr_security_context_manager_v1* security_context_mgr;
    struct wl_listener security_context_commit;

    /* Socket name for client connections */
    const char* socket;
};

/* Clipboard mediation (clipboard.c) */
void dlp_clipboard_init(struct coder_dlp_compositor* comp);

/* Security context protocol (security_context.c) */
void dlp_security_context_init(struct coder_dlp_compositor* comp);

/* Sandbox launcher helpers (sandbox_launcher.c) — exposed for testing */
char** dlp_build_bwrap_args(const struct coder_dlp_compositor* comp, const char* command,
                            const struct coder_dlp_sandbox_config* sandbox);
void dlp_free_bwrap_args(char** argv);

/* Output event handlers (output.c) */
void compositor_handle_new_output(struct wl_listener* listener, void* data);

/* Input event handlers (input.c) */
void compositor_handle_new_input(struct wl_listener* listener, void* data);

/* Cursor event handlers (input.c) */
void handle_cursor_motion(struct wl_listener* listener, void* data);
void handle_cursor_motion_absolute(struct wl_listener* listener, void* data);
void handle_cursor_button(struct wl_listener* listener, void* data);
void handle_cursor_axis(struct wl_listener* listener, void* data);
void handle_cursor_frame(struct wl_listener* listener, void* data);
void handle_request_set_cursor(struct wl_listener* listener, void* data);

/* Shell event handlers (shell.c) */
void compositor_handle_new_xdg_toplevel(struct wl_listener* listener, void* data);
void compositor_handle_new_xdg_popup(struct wl_listener* listener, void* data);

#endif /* CODER_DLP_COMPOSITOR_INTERNAL_H */
