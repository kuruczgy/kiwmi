/* Copyright (c), Charlotte Meyer <dev@buffet.sh>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef KIWMI_DESKTOP_DESKTOP_H
#define KIWMI_DESKTOP_DESKTOP_H

#include <wayland-server.h>
#include <wlr/render/wlr_renderer.h>

struct kiwmi_desktop {
    struct wlr_compositor *compositor;
    struct wlr_data_device_manager *data_device_manager;
    struct wlr_output_layout *output_layout;
    struct wl_list outputs; // struct kiwmi_output::link

    struct wl_listener new_output;
};

bool desktop_init(struct kiwmi_desktop *desktop, struct wlr_renderer *renderer);

#endif /* KIWMI_DESKTOP_DESKTOP_H */
