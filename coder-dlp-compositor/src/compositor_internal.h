#ifndef CODER_DLP_COMPOSITOR_INTERNAL_H
#define CODER_DLP_COMPOSITOR_INTERNAL_H

#include "coder_dlp.h"
#include "watermark.h"

#include <sys/types.h> /* pid_t */

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/config.h>
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
#if WLR_HAS_XWAYLAND
#include <wlr/xwayland/xwayland.h>
#endif

enum dlp_surface_type {
    DLP_SURFACE_XDG,
    DLP_SURFACE_XWAYLAND,
};

struct coder_dlp_toplevel {
    enum dlp_surface_type type;
    struct wlr_xdg_toplevel* xdg_toplevel;
    struct coder_dlp_compositor* compositor;
    struct wlr_scene_tree* scene_tree;

    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener commit;
    struct wl_listener destroy;
    struct wl_listener request_move;
    struct wl_listener request_resize;
    struct wl_listener request_maximize;
    struct wl_listener request_fullscreen;

    struct wl_list link; /* coder_dlp_compositor.toplevels */
};

#if WLR_HAS_XWAYLAND
struct coder_dlp_xwayland_surface {
    enum dlp_surface_type type;
    struct wlr_xwayland_surface* xwayland_surface;
    struct coder_dlp_compositor* compositor;
    struct wlr_scene_tree* scene_tree;

    struct wl_listener associate;
    struct wl_listener dissociate;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;
    struct wl_listener request_configure;
    struct wl_listener request_move;
    struct wl_listener request_resize;
    struct wl_listener request_maximize;
    struct wl_listener request_fullscreen;
    struct wl_listener request_activate;
    struct wl_listener set_override_redirect;
    struct wl_listener set_title;
    struct wl_listener set_class;

    struct wl_list link; /* coder_dlp_compositor.xwayland_surfaces */
};
#endif

struct dlp_dbus_proxy {
    pid_t pid;
    int pipe_write_fd;     /* close this to signal proxy to exit */
    char socket_path[256]; /* proxy socket path */
    char dir_path[256];    /* directory containing socket, for cleanup */
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

#if WLR_HAS_XWAYLAND
    /* Xwayland */
    struct wlr_xwayland* xwayland;
    struct wl_listener xwayland_surface;
    struct wl_listener xwayland_ready;
    struct wl_list xwayland_surfaces; /* coder_dlp_xwayland_surface.link */
#endif

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

    /* Touch */
    struct wlr_touch* touch;
    struct wl_listener touch_down;
    struct wl_listener touch_up;
    struct wl_listener touch_motion;
    struct wl_listener touch_cancel;
    struct wl_listener touch_frame;

    /* Seat (keyboard/pointer/clipboard) */
    struct wlr_seat* seat;

    /* Clipboard mediation */
    struct wl_listener request_set_selection;
    struct wl_listener request_set_primary_selection;

    /* Security context */
    struct wlr_security_context_manager_v1* security_context_mgr;
    struct wl_listener security_context_commit;

    /* Wayland client tracking */
    int client_count;
    struct wl_listener client_created;

    /* Socket name for client connections */
    const char* socket;

    /* Window title for the compositor output */
    char* output_title;

    /* Watermark */
    struct dlp_watermark_state watermark;

#if WLR_HAS_X11_BACKEND
    bool is_x11_backend; /* true when running on X11 host (wlr X11 backend) */
#endif

    /* Zombie reaper for D-Bus proxy child processes */
    struct wl_event_source* reap_timer;

    /* D-Bus proxy tracking */
    struct dlp_dbus_proxy* dbus_proxies;
    int dbus_proxy_count;
    int dbus_proxy_capacity;
};

/* Clipboard mediation (clipboard.c) */
void dlp_clipboard_init(struct coder_dlp_compositor* comp);

/* Security context protocol (security_context.c) */
void dlp_security_context_init(struct coder_dlp_compositor* comp);

/* Sandbox launcher helpers (sandbox_launcher.c) — exposed for testing */
char** dlp_build_bwrap_args(const struct coder_dlp_compositor* comp, const char* command,
                            const struct coder_dlp_sandbox_config* sandbox,
                            const char* dbus_proxy_socket);
void dlp_free_bwrap_args(char** argv);

/* D-Bus proxy lifecycle (sandbox_launcher.c) */
void dlp_cleanup_dbus_proxies(struct coder_dlp_compositor* comp);
void dlp_reap_dbus_proxies(struct coder_dlp_compositor* comp);

/* Output event handlers (output.c) */
void compositor_handle_new_output(struct wl_listener* listener, void* data);

/* Input event handlers (input.c) */
void compositor_handle_new_input(struct wl_listener* listener, void* data);

/* Touch event handlers (input.c) */
void handle_touch_down(struct wl_listener* listener, void* data);
void handle_touch_up(struct wl_listener* listener, void* data);
void handle_touch_motion(struct wl_listener* listener, void* data);
void handle_touch_cancel(struct wl_listener* listener, void* data);
void handle_touch_frame(struct wl_listener* listener, void* data);

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

#if WLR_HAS_XWAYLAND
/* Xwayland support (xwayland.c) */
void dlp_xwayland_init(struct coder_dlp_compositor* comp);
void dlp_xwayland_destroy(struct coder_dlp_compositor* comp);
#endif

#endif /* CODER_DLP_COMPOSITOR_INTERNAL_H */
