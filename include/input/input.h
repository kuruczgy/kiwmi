/* Copyright (c), Charlotte Meyer <dev@buffet.sh>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef KIWMI_INPUT_INPUT_H
#define KIWMI_INPUT_INPUT_H

#include <wayland-server.h>

struct kiwmi_input {
    struct wl_list keyboards; // struct kiwmi_keyboard::link
    struct wl_listener new_input;
    struct kiwmi_cursor *cursor;
};

bool input_init(struct kiwmi_input *input);

#endif /* KIWMI_INPUT_INPUT_H */
