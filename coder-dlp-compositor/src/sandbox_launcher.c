#include "coder_dlp.h"

#include <stdio.h>

int coder_dlp_launch_app(coder_dlp_compositor* comp, const char* command,
                          const coder_dlp_sandbox_config* sandbox) {
    (void)comp;
    (void)command;
    (void)sandbox;

    /* TODO (Phase 4): fork/exec the command inside a bwrap sandbox
     * with WAYLAND_DISPLAY pointing at our nested compositor socket,
     * applying sandbox_config constraints. */
    fprintf(stderr, "coder_dlp_launch_app: not yet implemented\n");
    return -1;
}
