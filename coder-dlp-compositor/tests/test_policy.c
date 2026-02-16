#include "coder_dlp.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Expose internals for testing */
#include "../src/compositor_internal.h"

static void test_set_policy(void) {
    coder_dlp_compositor* comp = coder_dlp_create(NULL);
    assert(comp != NULL);

    coder_dlp_policy policy;
    memset(&policy, 0, sizeof(policy));
    policy.clipboard_block_outgoing = true;
    policy.clipboard_block_incoming = false;
    policy.screenshot_block = true;
    policy.file_sandbox = true;
    policy.network_sandbox = false;

    coder_dlp_set_policy(comp, &policy);

    assert(comp->policy.clipboard_block_outgoing == true);
    assert(comp->policy.clipboard_block_incoming == false);
    assert(comp->policy.screenshot_block == true);
    assert(comp->policy.file_sandbox == true);
    assert(comp->policy.network_sandbox == false);

    coder_dlp_destroy(comp);
    printf("test_set_policy: PASSED\n");
}

static void test_null_safety(void) {
    /* These should not crash */
    coder_dlp_set_policy(NULL, NULL);
    coder_dlp_destroy(NULL);
    coder_dlp_dispatch(NULL);
    assert(coder_dlp_get_fd(NULL) == -1);

    printf("test_null_safety: PASSED\n");
}

int main(void) {
    test_set_policy();
    test_null_safety();
    printf("All tests passed.\n");
    return 0;
}
