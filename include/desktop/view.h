/* Copyright (c), Charlotte Meyer <dev@buffet.sh>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef KIWMI_DESKTOP_VIEW_H
#define KIWMI_DESKTOP_VIEW_H

#include <stdbool.h>
#include <stdlib.h>

#include <wayland-server.h>
#include <wlr/types/wlr_xdg_shell.h>

enum kiwmi_view_type {
    KIWMI_VIEW_XDG_SHELL,
};

struct kiwmi_view {
    struct wl_list link;

    struct kiwmi_desktop *desktop;

    const struct kiwmi_view_impl *impl;

    enum kiwmi_view_type type;
    union {
        struct wlr_xdg_surface *xdg_surface;
    };

    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;

    double x;
    double y;

    bool mapped;
};

struct kiwmi_view_impl {
    void (*for_each_surface)(
        struct kiwmi_view *view,
        wlr_surface_iterator_func_t iterator,
        void *user_data);
};

void kiwmi_view_for_each_surface(
    struct kiwmi_view *view,
    wlr_surface_iterator_func_t iterator,
    void *user_data);

void focus_view(struct kiwmi_view *view, struct wlr_surface *surface);
struct kiwmi_view *view_create(struct kiwmi_desktop *desktop, enum kiwmi_view_type type, const struct kiwmi_view_impl *impl);

#endif /* KIWMI_DESKTOP_VIEW_H */
