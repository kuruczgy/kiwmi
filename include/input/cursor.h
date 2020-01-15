/* Copyright (c), Charlotte Meyer <dev@buffet.sh>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef KIWMI_INPUT_CURSOR_H
#define KIWMI_INPUT_CURSOR_H

#include <wayland-server.h>
#include <wlr/types/wlr_output_layout.h>

struct kiwmi_cursor {
    struct kiwmi_server *server;
    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *xcursor_manager;

    struct wl_listener cursor_motion;
    struct wl_listener cursor_motion_absolute;
    struct wl_listener cursor_button;
    struct wl_listener cursor_axis;
    struct wl_listener cursor_frame;
    struct wl_listener seat_request_set_cursor;

    struct {
        struct wl_signal button_down;
        struct wl_signal button_up;
        struct wl_signal motion;
    } events;
};

struct kiwmi_cursor_button_event {
    struct wlr_event_pointer_button *wlr_event;
    bool handled;
};

struct kiwmi_cursor_motion_event {
    double oldx;
    double oldy;
    double newx;
    double newy;
};

struct kiwmi_cursor *cursor_create(
    struct kiwmi_server *server,
    struct wlr_output_layout *output_layout);
void cursor_destroy(struct kiwmi_cursor *cursor);

#endif /* KIWMI_INPUT_CURSOR_H */
