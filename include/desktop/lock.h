#ifndef KIWMI_DESKTOP_LOCK_H
#define KIWMI_DESKTOP_LOCK_H

#include "server.h"
#include <wlr/types/wlr_session_lock_v1.h>

struct session_lock {
    struct kiwmi_server *server;

    struct wlr_session_lock_v1 *lock;
    struct wlr_surface *focused;
    bool abandoned;

    struct wl_list outputs; // struct session_lock_output

    // invalid if the session is abandoned
    struct wl_listener new_surface;
    struct wl_listener unlock;
    struct wl_listener destroy;
};

void session_lock_init(struct kiwmi_server *server);
void
session_lock_add_output(struct session_lock *lock, struct kiwmi_output *output);
bool session_lock_has_surface(
    struct session_lock *lock,
    struct wlr_surface *surface);

#endif
