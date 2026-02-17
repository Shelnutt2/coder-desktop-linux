#include "coder_dlp.h"
#include "compositor_internal.h"

#include <string.h>

void coder_dlp_set_policy(coder_dlp_compositor* comp, const coder_dlp_policy* policy) {
    if (!comp || !policy) {
        return;
    }
    memcpy(&comp->policy, policy, sizeof(coder_dlp_policy));
}
