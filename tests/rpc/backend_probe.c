/* libuv must declare ssize_t before libwebsockets defines it as a macro on Windows. */
#include <uv.h>
#include <libwebsockets.h>

#include <stdio.h>
#include <string.h>

int main(void) {
    uv_loop_t loop;
    if (uv_loop_init(&loop) != 0) {
        return 1;
    }

    struct lws_context* context = NULL;
    void* loops[] = {&loop};
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.options = LWS_SERVER_OPTION_LIBUV;
    info.foreign_loops = loops;
    info.pcontext = &context;
    info.fd_limit_per_thread = 16;

    context = lws_create_context(&info);
    if (context == NULL) {
        uv_loop_close(&loop);
        return 2;
    }

    lws_cancel_service(context);
    uv_run(&loop, UV_RUN_NOWAIT);
    lws_context_destroy(context);
    for (int poll = 0; context != NULL && poll < 1000; ++poll) {
        uv_run(&loop, UV_RUN_NOWAIT);
        if (context != NULL) {
            lws_context_destroy(context);
        }
    }
    if (context != NULL || uv_loop_close(&loop) != 0) {
        return 3;
    }

    puts("libwebsockets/libuv create, wake, async destroy: passed (no listener)");
    return 0;
}
