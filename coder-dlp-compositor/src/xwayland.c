/* Xwayland surface management.
 *
 * Mirrors the XDG toplevel lifecycle in shell.c but for X11 apps running
 * through Xwayland.  Managed windows (non-override-redirect) are auto-
 * maximised to fill the output; override-redirect windows (menus, tooltips,
 * popups) are positioned at their requested coordinates. */

#include "coder_dlp.h"
#include "compositor_internal.h"

#include <stdlib.h>

#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <wlr/xwayland/xwayland.h>

#if WLR_HAS_X11_BACKEND

/* --- Focus helper -------------------------------------------------------- */

static void focus_xwayland_surface(struct coder_dlp_xwayland_surface* xsurf) {
    struct coder_dlp_compositor* comp = xsurf->compositor;
    struct wlr_xwayland_surface* xsurface = xsurf->xwayland_surface;

    if (!xsurface->surface) {
        return;
    }

    /* Raise in scene graph */
    wlr_scene_node_raise_to_top(&xsurf->scene_tree->node);

    /* Deactivate previous focused surface (Xwayland or XDG) */
    struct wlr_surface* prev = comp->seat->keyboard_state.focused_surface;
    if (prev) {
        struct wlr_xwayland_surface* prev_xsurface =
            wlr_xwayland_surface_try_from_wlr_surface(prev);
        if (prev_xsurface) {
            wlr_xwayland_surface_activate(prev_xsurface, false);
        } else {
            struct wlr_xdg_toplevel* prev_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(prev);
            if (prev_toplevel) {
                wlr_xdg_toplevel_set_activated(prev_toplevel, false);
            }
        }
    }

    /* Activate + focus based on ICCCM input model */
    enum wlr_xwayland_icccm_input_model model = wlr_xwayland_surface_icccm_input_model(xsurface);
    if (model == WLR_ICCCM_INPUT_MODEL_GLOBAL) {
        wlr_xwayland_surface_offer_focus(xsurface);
    } else {
        wlr_xwayland_surface_activate(xsurface, true);
    }

    struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(comp->seat);
    if (keyboard) {
        wlr_seat_keyboard_notify_enter(comp->seat, xsurface->surface, keyboard->keycodes,
                                       keyboard->num_keycodes, &keyboard->modifiers);
    }
}

/* --- Surface event handlers ---------------------------------------------- */

static void handle_xwayland_surface_map(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_xwayland_surface* xsurf = wl_container_of(listener, xsurf, map);
    struct wlr_xwayland_surface* xsurface = xsurf->xwayland_surface;

    wlr_log(WLR_INFO, "xwayland surface mapped: class=%s title=%s override_redirect=%d",
            xsurface->class ? xsurface->class : "(null)",
            xsurface->title ? xsurface->title : "(null)", xsurface->override_redirect);

    wlr_scene_node_set_enabled(&xsurf->scene_tree->node, true);

    if (xsurface->override_redirect) {
        wlr_scene_node_set_position(&xsurf->scene_tree->node, xsurface->x, xsurface->y);
        wlr_scene_node_raise_to_top(&xsurf->scene_tree->node);
        if (wlr_xwayland_surface_override_redirect_wants_focus(xsurface)) {
            focus_xwayland_surface(xsurf);
        }
    } else {
        /* Managed window: maximize to fill the output */
        struct coder_dlp_compositor* comp = xsurf->compositor;
        if (comp->output) {
            wlr_xwayland_surface_configure(xsurface, 0, 0, comp->output->width,
                                           comp->output->height);
            wlr_xwayland_surface_set_maximized(xsurface, true, true);
        }
        focus_xwayland_surface(xsurf);
    }
}

static void handle_xwayland_surface_unmap(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_xwayland_surface* xsurf = wl_container_of(listener, xsurf, unmap);

    wlr_log(WLR_INFO, "xwayland surface unmapped: class=%s",
            xsurf->xwayland_surface->class ? xsurf->xwayland_surface->class : "(null)");

    wlr_scene_node_set_enabled(&xsurf->scene_tree->node, false);
}

static void handle_xwayland_surface_associate(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_xwayland_surface* xsurf = wl_container_of(listener, xsurf, associate);
    struct wlr_xwayland_surface* xsurface = xsurf->xwayland_surface;

    /* The wlr_surface is now valid — create the scene subsurface tree.
     * We deferred this from new_surface because xsurface->surface was NULL. */
    if (xsurf->scene_tree) {
        wlr_scene_node_destroy(&xsurf->scene_tree->node);
    }
    xsurf->scene_tree =
        wlr_scene_subsurface_tree_create(&xsurf->compositor->scene->tree, xsurface->surface);
    if (xsurf->scene_tree) {
        xsurf->scene_tree->node.data = xsurf;
        wlr_scene_node_set_enabled(&xsurf->scene_tree->node, false);
    }

    /* Wire up map/unmap listeners on the wlr_surface */
    xsurf->map.notify = handle_xwayland_surface_map;
    wl_signal_add(&xsurface->surface->events.map, &xsurf->map);

    xsurf->unmap.notify = handle_xwayland_surface_unmap;
    wl_signal_add(&xsurface->surface->events.unmap, &xsurf->unmap);

    /* Notify the surface callback if registered */
    if (xsurf->compositor->surface_cb) {
        xsurf->compositor->surface_cb(xsurf->compositor, xsurface->surface,
                                      xsurf->compositor->surface_cb_data);
    }
}

static void handle_xwayland_surface_dissociate(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_xwayland_surface* xsurf = wl_container_of(listener, xsurf, dissociate);

    wl_list_remove(&xsurf->map.link);
    wl_list_init(&xsurf->map.link);
    wl_list_remove(&xsurf->unmap.link);
    wl_list_init(&xsurf->unmap.link);
}

static void handle_xwayland_surface_destroy(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_xwayland_surface* xsurf = wl_container_of(listener, xsurf, destroy);

    wlr_log(WLR_INFO, "xwayland surface destroyed: class=%s",
            xsurf->xwayland_surface->class ? xsurf->xwayland_surface->class : "(null)");

    wl_list_remove(&xsurf->map.link);
    wl_list_remove(&xsurf->unmap.link);
    wl_list_remove(&xsurf->associate.link);
    wl_list_remove(&xsurf->dissociate.link);
    wl_list_remove(&xsurf->destroy.link);
    wl_list_remove(&xsurf->request_configure.link);
    wl_list_remove(&xsurf->request_move.link);
    wl_list_remove(&xsurf->request_resize.link);
    wl_list_remove(&xsurf->request_maximize.link);
    wl_list_remove(&xsurf->request_fullscreen.link);
    wl_list_remove(&xsurf->request_activate.link);
    wl_list_remove(&xsurf->set_override_redirect.link);
    wl_list_remove(&xsurf->set_title.link);
    wl_list_remove(&xsurf->set_class.link);
    wl_list_remove(&xsurf->link);
    free(xsurf);
}

static void handle_xwayland_request_configure(struct wl_listener* listener, void* data) {
    struct coder_dlp_xwayland_surface* xsurf = wl_container_of(listener, xsurf, request_configure);
    struct wlr_xwayland_surface_configure_event* event = data;

    if (xsurf->xwayland_surface->override_redirect) {
        /* Override-redirect: allow requested position/size */
        wlr_xwayland_surface_configure(xsurf->xwayland_surface, event->x, event->y, event->width,
                                       event->height);
        wlr_scene_node_set_position(&xsurf->scene_tree->node, event->x, event->y);
    } else {
        /* Managed: force maximize */
        struct coder_dlp_compositor* comp = xsurf->compositor;
        if (comp->output) {
            wlr_xwayland_surface_configure(xsurf->xwayland_surface, 0, 0, comp->output->width,
                                           comp->output->height);
        } else {
            wlr_xwayland_surface_configure(xsurf->xwayland_surface, event->x, event->y,
                                           event->width, event->height);
        }
    }
}

static void handle_xwayland_request_move(struct wl_listener* listener, void* data) {
    (void)listener;
    (void)data;
    /* Interactive move not supported — clients are maximised */
}

static void handle_xwayland_request_resize(struct wl_listener* listener, void* data) {
    (void)listener;
    (void)data;
    /* Interactive resize not supported */
}

static void handle_xwayland_request_maximize(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_xwayland_surface* xsurf = wl_container_of(listener, xsurf, request_maximize);
    struct coder_dlp_compositor* comp = xsurf->compositor;

    if (comp->output) {
        wlr_xwayland_surface_configure(xsurf->xwayland_surface, 0, 0, comp->output->width,
                                       comp->output->height);
        wlr_xwayland_surface_set_maximized(xsurf->xwayland_surface, true, true);
    }
}

static void handle_xwayland_request_fullscreen(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_xwayland_surface* xsurf = wl_container_of(listener, xsurf, request_fullscreen);
    struct coder_dlp_compositor* comp = xsurf->compositor;

    if (comp->output) {
        wlr_xwayland_surface_configure(xsurf->xwayland_surface, 0, 0, comp->output->width,
                                       comp->output->height);
        wlr_xwayland_surface_set_fullscreen(xsurf->xwayland_surface, true);
    }
}

static void handle_xwayland_request_activate(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_xwayland_surface* xsurf = wl_container_of(listener, xsurf, request_activate);
    focus_xwayland_surface(xsurf);
}

static void handle_xwayland_set_override_redirect(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_xwayland_surface* xsurf =
        wl_container_of(listener, xsurf, set_override_redirect);

    wlr_log(WLR_DEBUG, "xwayland surface override_redirect changed to %d",
            xsurf->xwayland_surface->override_redirect);
}

static void handle_xwayland_set_title(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_xwayland_surface* xsurf = wl_container_of(listener, xsurf, set_title);

    wlr_log(WLR_DEBUG, "xwayland surface title changed: %s",
            xsurf->xwayland_surface->title ? xsurf->xwayland_surface->title : "(null)");
}

static void handle_xwayland_set_class(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_xwayland_surface* xsurf = wl_container_of(listener, xsurf, set_class);

    wlr_log(WLR_DEBUG, "xwayland surface class changed: %s",
            xsurf->xwayland_surface->class ? xsurf->xwayland_surface->class : "(null)");
}

/* --- Xwayland server events ---------------------------------------------- */

static void handle_xwayland_new_surface(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, xwayland_surface);
    struct wlr_xwayland_surface* xsurface = data;

    wlr_log(WLR_INFO, "new xwayland surface: class=%s title=%s override_redirect=%d",
            xsurface->class ? xsurface->class : "(null)",
            xsurface->title ? xsurface->title : "(null)", xsurface->override_redirect);

    struct coder_dlp_xwayland_surface* xsurf = calloc(1, sizeof(*xsurf));
    if (!xsurf) {
        return;
    }

    xsurf->type = DLP_SURFACE_XWAYLAND;
    xsurf->xwayland_surface = xsurface;
    xsurf->compositor = comp;

    /* Init map/unmap links so they are always safe to wl_list_remove() even
     * if the surface is destroyed before associate fires. */
    wl_list_init(&xsurf->map.link);
    wl_list_init(&xsurf->unmap.link);

    /* Scene tree is created later in the associate handler once the
     * wlr_surface is valid.  Store a placeholder empty tree for now. */
    xsurf->scene_tree = wlr_scene_tree_create(&comp->scene->tree);
    if (xsurf->scene_tree) {
        xsurf->scene_tree->node.data = xsurf;
        wlr_scene_node_set_enabled(&xsurf->scene_tree->node, false);
    }

    /* Lifecycle events */
    xsurf->associate.notify = handle_xwayland_surface_associate;
    wl_signal_add(&xsurface->events.associate, &xsurf->associate);

    xsurf->dissociate.notify = handle_xwayland_surface_dissociate;
    wl_signal_add(&xsurface->events.dissociate, &xsurf->dissociate);

    xsurf->destroy.notify = handle_xwayland_surface_destroy;
    wl_signal_add(&xsurface->events.destroy, &xsurf->destroy);

    /* Request events */
    xsurf->request_configure.notify = handle_xwayland_request_configure;
    wl_signal_add(&xsurface->events.request_configure, &xsurf->request_configure);

    xsurf->request_move.notify = handle_xwayland_request_move;
    wl_signal_add(&xsurface->events.request_move, &xsurf->request_move);

    xsurf->request_resize.notify = handle_xwayland_request_resize;
    wl_signal_add(&xsurface->events.request_resize, &xsurf->request_resize);

    xsurf->request_maximize.notify = handle_xwayland_request_maximize;
    wl_signal_add(&xsurface->events.request_maximize, &xsurf->request_maximize);

    xsurf->request_fullscreen.notify = handle_xwayland_request_fullscreen;
    wl_signal_add(&xsurface->events.request_fullscreen, &xsurf->request_fullscreen);

    xsurf->request_activate.notify = handle_xwayland_request_activate;
    wl_signal_add(&xsurface->events.request_activate, &xsurf->request_activate);

    /* Property change events */
    xsurf->set_override_redirect.notify = handle_xwayland_set_override_redirect;
    wl_signal_add(&xsurface->events.set_override_redirect, &xsurf->set_override_redirect);

    xsurf->set_title.notify = handle_xwayland_set_title;
    wl_signal_add(&xsurface->events.set_title, &xsurf->set_title);

    xsurf->set_class.notify = handle_xwayland_set_class;
    wl_signal_add(&xsurface->events.set_class, &xsurf->set_class);

    wl_list_insert(&comp->xwayland_surfaces, &xsurf->link);
}

static void handle_xwayland_ready(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, xwayland_ready);

    wlr_log(WLR_INFO, "xwayland ready on display %s",
            comp->xwayland->display_name ? comp->xwayland->display_name : "(null)");
}

/* --- Public API ---------------------------------------------------------- */

const char* coder_dlp_get_xwayland_display(const coder_dlp_compositor* comp) {
    if (!comp || !comp->xwayland) {
        return NULL;
    }
    return comp->xwayland->display_name;
}

void dlp_xwayland_init(struct coder_dlp_compositor* comp) {
    wl_list_init(&comp->xwayland_surfaces);

    /* Disable glamor/DRI3 in Xwayland.  Without this, Xwayland's glamor
     * allocates DMA-BUF buffers with GPU-specific tiled/compressed modifiers
     * (e.g. Intel CCS) that the nested compositor's renderer cannot import,
     * causing garbled or black rendering.  With glamor disabled, buffer
     * transport falls back to wl_shm (software pixmap conversion) while X11
     * clients still use GPU for rasterization — only the compositor handoff
     * changes.  Xwayland inherits our environment when wlroots fork()s it. */
    setenv("XWAYLAND_NO_GLAMOR", "1", 1);

    comp->xwayland = wlr_xwayland_create(comp->wl_display, comp->wlr_compositor, true);
    if (!comp->xwayland) {
        wlr_log(WLR_ERROR, "failed to create xwayland");
        return;
    }

    wlr_xwayland_set_seat(comp->xwayland, comp->seat);

    comp->xwayland_surface.notify = handle_xwayland_new_surface;
    wl_signal_add(&comp->xwayland->events.new_surface, &comp->xwayland_surface);

    comp->xwayland_ready.notify = handle_xwayland_ready;
    wl_signal_add(&comp->xwayland->events.ready, &comp->xwayland_ready);

    wlr_log(WLR_INFO, "xwayland initialized (lazy mode)");
}

void dlp_xwayland_destroy(struct coder_dlp_compositor* comp) {
    /* Clean up the environment variable set in dlp_xwayland_init(). */
    unsetenv("XWAYLAND_NO_GLAMOR");

    if (!comp->xwayland) {
        return;
    }

    wl_list_remove(&comp->xwayland_surface.link);
    wl_list_remove(&comp->xwayland_ready.link);

    /* Clean up remaining tracked surfaces */
    struct coder_dlp_xwayland_surface* xsurf;
    struct coder_dlp_xwayland_surface* tmp;
    wl_list_for_each_safe(xsurf, tmp, &comp->xwayland_surfaces, link) {
        wl_list_remove(&xsurf->map.link);
        wl_list_remove(&xsurf->unmap.link);
        wl_list_remove(&xsurf->associate.link);
        wl_list_remove(&xsurf->dissociate.link);
        wl_list_remove(&xsurf->destroy.link);
        wl_list_remove(&xsurf->request_configure.link);
        wl_list_remove(&xsurf->request_move.link);
        wl_list_remove(&xsurf->request_resize.link);
        wl_list_remove(&xsurf->request_maximize.link);
        wl_list_remove(&xsurf->request_fullscreen.link);
        wl_list_remove(&xsurf->request_activate.link);
        wl_list_remove(&xsurf->set_override_redirect.link);
        wl_list_remove(&xsurf->set_title.link);
        wl_list_remove(&xsurf->set_class.link);
        wl_list_remove(&xsurf->link);
        free(xsurf);
    }

    wlr_xwayland_destroy(comp->xwayland);
    comp->xwayland = NULL;
}

#endif /* WLR_HAS_X11_BACKEND */
