/* Copyright (c), Niclas Meyer <niclas@countingsort.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef KIWMI_SERVER_H
#define KIWMI_SERVER_H

#include <stdbool.h>

#include "desktop/desktop.h"
#include "input/input.h"
#include "websocket.h"
#include <pango/pango-font.h>

struct kiwmi_server {
    struct wl_display *wl_display;
    struct wl_event_loop *wl_event_loop;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;

    const char *socket;
    char *config_path;
    struct kiwmi_lua *lua;
    struct kiwmi_desktop desktop;
    struct kiwmi_input input;

    PangoFontDescription *font_description;
    struct websocket *websocket;

    struct {
        struct wl_signal destroy;
    } events;

    struct {
        struct session_lock *lock;
        struct wlr_session_lock_manager_v1 *manager;

        struct wl_listener new_lock;
        struct wl_listener manager_destroy;
    } session_lock;
};

bool server_init(struct kiwmi_server *server, char *config_path);
bool server_run(struct kiwmi_server *server);
void server_fini(struct kiwmi_server *server);

#endif /* KIWMI_SERVER_H */
