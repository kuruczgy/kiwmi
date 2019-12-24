/* Copyright (c), Charlotte Meyer <dev@buffet.sh>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "desktop/view.h"
#include "server.h"

void
kiwmi_view_for_each_surface(
    struct kiwmi_view *view,
    wlr_surface_iterator_func_t iterator,
    void *user_data)
{
    if (view->impl->for_each_surface) {
        view->impl->for_each_surface(view, iterator, user_data);
    }
}

void
focus_view(struct kiwmi_view *view, struct wlr_surface *surface)
{
    if (!view) {
        return;
    }

    struct kiwmi_desktop *desktop = view->desktop;
    struct kiwmi_server *server   = wl_container_of(desktop, server, desktop);
    struct wlr_seat *seat         = server->input.seat;
    struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;

    if (prev_surface == surface) {
        return;
    }

    if (prev_surface) {
        struct wlr_xdg_surface *previous = wlr_xdg_surface_from_wlr_surface(
            seat->keyboard_state.focused_surface);
        wlr_xdg_toplevel_set_activated(previous, false);
    }

    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);

    // move view to front
    wl_list_remove(&view->link);
    wl_list_insert(&desktop->views, &view->link);

    wlr_xdg_toplevel_set_activated(view->xdg_surface, true);
    wlr_seat_keyboard_notify_enter(
        seat,
        view->xdg_surface->surface,
        keyboard->keycodes,
        keyboard->num_keycodes,
        &keyboard->modifiers);
}
