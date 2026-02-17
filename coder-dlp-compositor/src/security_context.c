/* wp-security-context-v1 support.
 *
 * The security context protocol allows a privileged client (the sandbox
 * launcher) to create a restricted Wayland socket.  Clients connecting
 * via that socket are tagged with an application ID / sandbox engine so
 * the compositor can make per-client policy decisions (e.g. deny
 * screencopy for untrusted apps). */

#include "coder_dlp.h"
#include "compositor_internal.h"

#include <stdio.h>

#include <wlr/types/wlr_security_context_v1.h>

static void handle_security_context_commit(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, security_context_commit);
    const struct wlr_security_context_v1_commit_event* event = data;

    /* Log for debugging — the security context is automatically applied
     * by wlroots to clients connecting via the sandboxed socket. */
    fprintf(stderr, "coder-dlp: security context commit: app_id=%s engine=%s\n",
            event->state->app_id ? event->state->app_id : "(none)",
            event->state->sandbox_engine ? event->state->sandbox_engine : "(none)");
}

void dlp_security_context_init(struct coder_dlp_compositor* comp) {
    if (!comp) {
        return;
    }

    comp->security_context_mgr = wlr_security_context_manager_v1_create(comp->wl_display);
    if (!comp->security_context_mgr) {
        fprintf(stderr, "coder-dlp: failed to create security context manager\n");
        return;
    }

    comp->security_context_commit.notify = handle_security_context_commit;
    wl_signal_add(&comp->security_context_mgr->events.commit, &comp->security_context_commit);
}
