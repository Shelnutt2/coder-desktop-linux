#ifndef CODER_DLP_COMPOSITOR_INTERNAL_H
#define CODER_DLP_COMPOSITOR_INTERNAL_H

#include "coder_dlp.h"

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xdg_shell.h>

struct coder_dlp_toplevel {
    struct wlr_xdg_toplevel *xdg_toplevel;
    struct coder_dlp_compositor *compositor;
    struct wlr_scene_tree *scene_tree;

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
    void *surface_cb_data;

    /* Wayland core */
    struct wl_display *wl_display;
    struct wl_event_loop *wl_event_loop;

    /* wlroots objects */
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    struct wlr_compositor *wlr_compositor;
    struct wlr_subcompositor *subcompositor;
    struct wlr_output_layout *output_layout;
    struct wlr_scene *scene;
    struct wlr_scene_output_layout *scene_layout;
    struct wlr_xdg_shell *xdg_shell;
    struct wlr_data_device_manager *data_device_mgr;

    /* Output */
    struct wlr_output *output;
    struct wl_listener output_frame;
    struct wl_listener output_request_state;
    struct wl_listener output_destroy;

    /* Shell */
    struct wl_listener new_xdg_toplevel;
    struct wl_listener new_xdg_popup;
    struct wl_list toplevels; /* coder_dlp_toplevel.link */

    /* Backend */
    struct wl_listener new_output;
    struct wl_listener backend_destroy;

    /* Socket name for client connections */
    const char *socket;
};

/* Output event handlers (output.c) */
void compositor_handle_new_output(struct wl_listener *listener, void *data);

/* Shell event handlers (shell.c) */
void compositor_handle_new_xdg_toplevel(struct wl_listener *listener, void *data);
void compositor_handle_new_xdg_popup(struct wl_listener *listener, void *data);

#endif /* CODER_DLP_COMPOSITOR_INTERNAL_H */
