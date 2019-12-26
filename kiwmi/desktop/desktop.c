/* Copyright (c), Charlotte Meyer <dev@buffet.sh>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "desktop/desktop.h"

#include <stdbool.h>

#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>

#include "desktop/output.h"
#include "desktop/xdg_shell.h"
#include "server.h"

bool
desktop_init(struct kiwmi_desktop *desktop, struct wlr_renderer *renderer)
{
    struct kiwmi_server *server = wl_container_of(desktop, server, desktop);
    desktop->compositor = wlr_compositor_create(server->wl_display, renderer);
    desktop->data_device_manager =
        wlr_data_device_manager_create(server->wl_display);
    desktop->output_layout = wlr_output_layout_create();

    wlr_export_dmabuf_manager_v1_create(server->wl_display);
    wlr_xdg_output_manager_v1_create(
        server->wl_display, desktop->output_layout);

    desktop->xdg_shell = wlr_xdg_shell_create(server->wl_display);
    desktop->xdg_shell_new_surface.notify = xdg_shell_new_surface_notify;
    wl_signal_add(
        &desktop->xdg_shell->events.new_surface,
        &desktop->xdg_shell_new_surface);

    desktop->focused_view = NULL;

    wl_list_init(&desktop->outputs);
    wl_list_init(&desktop->views);

    desktop->new_output.notify = new_output_notify;
    wl_signal_add(&server->backend->events.new_output, &desktop->new_output);

    return true;
}
