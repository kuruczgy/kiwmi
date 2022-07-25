/* Copyright (c), Charlotte Meyer <dev@buffet.sh>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef KIWMI_INPUT_POINTER_H
#define KIWMI_INPUT_POINTER_H

#include <wayland-server.h>
#include <wlr/types/wlr_input_device.h>

#include "server.h"

struct kiwmi_pointer {
    struct wlr_input_device *device;
    struct wl_list link;

    struct wl_listener device_destroy;
};

struct kiwmi_pointer *
pointer_create(struct kiwmi_server *server, struct wlr_input_device *device);
void pointer_destroy(struct kiwmi_pointer *pointer);

#endif /* KIWMI_INPUT_POINTER_H */
