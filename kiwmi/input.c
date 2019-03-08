/* Copyright (c), Charlotte Meyer <dev@buffet.sh>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "kiwmi/input.h"

#include <wayland-server.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/util/log.h>

#include "kiwmi/server.h"

static void
new_pointer(struct kiwmi_server *server, struct wlr_input_device *device)
{
    wlr_cursor_attach_input_device(server->cursor->cursor, device);
}

void
new_input_notify(struct wl_listener *listener, void *data)
{
    struct kiwmi_server *server = wl_container_of(listener, server, new_input);
    struct wlr_input_device *device = data;

    wlr_log(WLR_DEBUG, "Adding input device: '%s'", device->name);

    switch (device->type) {
    case WLR_INPUT_DEVICE_POINTER:
        new_pointer(server, device);
        break;
    default:
        // NOT HANDLED
        break;
    }
}
