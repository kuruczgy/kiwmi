/* Copyright (c), Niclas Meyer <niclas@countingsort.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef KIWMI_LUAK_LUAK_H
#define KIWMI_LUAK_LUAK_H

#include <stdbool.h>

#include <lua.h>
#include <wayland-server.h>

#include "server.h"

struct kiwmi_lua {
    lua_State *L;

    // The index of the table in the registry where the objects are stored. The
    // keys are the opaque pointers wrapped as a light userdata, while the
    // values are pointers to kiwmi_object structs, also wrapped as light
    // userdata.
    int objects;

    struct wl_list scheduled_callbacks; // struct kiwmi_lua_callback::link
    struct wl_global *global;

    struct kiwmi_server *server;
};

struct kiwmi_object {
    struct kiwmi_lua *lua;

    void *object;
    size_t refcount;
    bool valid;

    // `luaK_get_kiwmi_object` can optionally attach this to an external signal
    // that signals to destroy this object. The object is not destroyed
    // immediately, but instead `valid` is set to `false`, and the memory is
    // only actually freed when `refcount` reaches zero.
    struct wl_listener destroy;

    struct wl_list callbacks; // struct kiwmi_lua_callback::link

    struct {
        struct wl_signal destroy;
    } events;
};

/**
 * Convert a lua value on the stack to a userdata and check if it has the
 * correct metatable.
 * \param ud The stack index.
 * \param tname The field where the metatable is stored in the registry.
 * \return The memory-block address of the userdata. NULL if the metatable did
 * not match.
*/
void *luaK_toudata(lua_State *L, int ud, const char *tname);

/**
 * Attach it as the `__gc` metamethod to (TODO: what?)
*/
int luaK_kiwmi_object_gc(lua_State *L);

/** Gets the object corresponding to `ptr`, or creates a new one if does not
 * exist yet. */
struct kiwmi_object *luaK_get_kiwmi_object(
    struct kiwmi_lua *lua,
    void *ptr,
    struct wl_signal *destroy);

int luaK_callback_register_dispatch(lua_State *L);

/** Attach this as the `__eq` metamethod to the userdata values. */
int luaK_usertype_ref_equal(lua_State *L);
struct kiwmi_lua *luaK_create(struct kiwmi_server *server);
bool luaK_dofile(struct kiwmi_lua *lua, const char *config_path);
void luaK_destroy(struct kiwmi_lua *lua);

#endif /* KIWMI_LUAK_LUAK_H */
