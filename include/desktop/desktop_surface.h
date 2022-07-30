/* Copyright (c), Charlotte Meyer <dev@buffet.sh>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef KIWMI_DESKTOP_DESKTOP_SURFACE_H
#define KIWMI_DESKTOP_DESKTOP_SURFACE_H

enum kiwmi_desktop_surface_type {
    KIWMI_DESKTOP_SURFACE_VIEW,
    KIWMI_DESKTOP_SURFACE_LAYER,
};

struct kiwmi_desktop_surface {
    // The tree is where the config is supposed to put custom decorations (it
    // also contains the surface_node)
    struct wlr_scene_tree *tree;
    struct wlr_scene_node *surface_node;

    struct wlr_scene_tree *popups_tree;

    enum kiwmi_desktop_surface_type type;
};

#endif /* KIWMI_DESKTOP_DESKTOP_SURFACE_H */
