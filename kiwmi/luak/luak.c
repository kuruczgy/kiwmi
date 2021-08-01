/* Copyright (c), Charlotte Meyer <dev@buffet.sh>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "luak/luak.h"

#include <stdlib.h>

#include <lauxlib.h>
#include <lualib.h>
#include <wlr/util/log.h>

#include "luak/ipc.h"
#include "luak/kiwmi_cursor.h"
#include "luak/kiwmi_keyboard.h"
#include "luak/kiwmi_lua_callback.h"
#include "luak/kiwmi_output.h"
#include "luak/kiwmi_renderer.h"
#include "luak/kiwmi_server.h"
#include "luak/kiwmi_view.h"

void *
luaK_toudata(lua_State *L, int ud, const char *tname)
{
    void *p = lua_touserdata(L, ud);
    if (p != NULL) {  /* value is a userdata? */
        if (lua_getmetatable(L, ud)) {  /* does it have a metatable? */
            lua_getfield(L, LUA_REGISTRYINDEX, tname);  /* get correct metatable */
            if (lua_rawequal(L, -1, -2)) {  /* does it have the correct mt? */
                lua_pop(L, 2);  /* remove both metatables */
                return p;
            }
        }
    }
    return NULL;
}

int
luaK_kiwmi_object_gc(lua_State *L)
{
    struct kiwmi_object *obj = *(struct kiwmi_object **)lua_touserdata(L, 1);

    --obj->refcount;

    if (obj->refcount == 0 && wl_list_empty(&obj->callbacks)) {
        free(obj);
    }

    return 0;
}

static void
kiwmi_object_destroy_notify(struct wl_listener *listener, void *data)
{
    struct kiwmi_object *obj = wl_container_of(listener, obj, destroy);

    wl_signal_emit(&obj->events.destroy, data);

    struct kiwmi_lua_callback *lc;
    struct kiwmi_lua_callback *tmp;
    wl_list_for_each_safe (lc, tmp, &obj->callbacks, link) {
        wl_list_remove(&lc->listener.link);
        wl_list_remove(&lc->link);

        luaL_unref(lc->server->lua->L, LUA_REGISTRYINDEX, lc->callback_ref);

        free(lc);
    }

    wl_list_remove(&obj->destroy.link);

    lua_State *L = obj->lua->L;

    lua_rawgeti(L, LUA_REGISTRYINDEX, obj->lua->objects);
    lua_pushlightuserdata(L, obj->object);
    lua_pushnil(L);
    lua_settable(L, -3);
    lua_pop(L, 1);

    obj->valid = false;

    if (obj->refcount == 0) {
        free(obj);
    }
}

struct kiwmi_object *
luaK_get_kiwmi_object(
    struct kiwmi_lua *lua,
    void *ptr,
    struct wl_signal *destroy)
{
    lua_State *L = lua->L;

    lua_rawgeti(L, LUA_REGISTRYINDEX, lua->objects);
    lua_pushlightuserdata(L, ptr);
    lua_gettable(L, -2);

    struct kiwmi_object *obj = lua_touserdata(L, -1);

    lua_pop(L, 2);

    if (obj) {
        ++obj->refcount;
        return obj;
    }

    obj = malloc(sizeof(*obj));
    if (!obj) {
        wlr_log(WLR_ERROR, "Failed to allocate kiwmi_object");
        return NULL;
    }

    obj->lua      = lua;
    obj->object   = ptr;
    obj->refcount = 1;
    obj->valid    = true;

    if (destroy) {
        obj->destroy.notify = kiwmi_object_destroy_notify;
        wl_signal_add(destroy, &obj->destroy);

        wl_signal_init(&obj->events.destroy);
    }

    wl_list_init(&obj->callbacks);

    lua_rawgeti(L, LUA_REGISTRYINDEX, lua->objects);
    lua_pushlightuserdata(L, ptr);
    lua_pushlightuserdata(L, obj);
    lua_settable(L, -3);

    return obj;
}

int
luaK_callback_register_dispatch(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TUSERDATA); // object
    luaL_checktype(L, 2, LUA_TSTRING);   // type
    luaL_checktype(L, 3, LUA_TFUNCTION); // callback

    int has_mt = lua_getmetatable(L, 1);
    luaL_argcheck(L, has_mt, 1, "no metatable");
    lua_getfield(L, -1, "__events");
    luaL_argcheck(L, lua_istable(L, -1), 1, "no __events");
    lua_pushvalue(L, 2);
    lua_gettable(L, -2);

    luaL_argcheck(L, lua_iscfunction(L, -1), 2, "invalid event");
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 3);

    if (lua_pcall(L, 2, 1, 0)) {
        wlr_log(WLR_ERROR, "%s", lua_tostring(L, -1));
        return 0;
    }

    return 1;
}

int
luaK_usertype_ref_equal(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TUSERDATA);
    luaL_checktype(L, 2, LUA_TUSERDATA);

    void *a = *(void **)lua_touserdata(L, 1);
    void *b = *(void **)lua_touserdata(L, 2);

    lua_pushboolean(L, a == b);

    return 1;
}

struct kiwmi_lua *
luaK_create(struct kiwmi_server *server)
{
    struct kiwmi_lua *lua = malloc(sizeof(*lua));
    if (!lua) {
        wlr_log(WLR_ERROR, "Failed to allocate kiwmi_lua");
        return NULL;
    }

    lua_State *L = luaL_newstate();
    if (!L) {
        free(lua);
        return NULL;
    }

    lua->L = L;

    luaL_openlibs(L);

    wl_list_init(&lua->scheduled_callbacks);

    // init object registry
    lua_newtable(L);
    lua->objects = luaL_ref(L, LUA_REGISTRYINDEX);

    // register types
    int error = 0;

    lua_pushcfunction(L, luaK_kiwmi_cursor_register);
    error |= lua_pcall(L, 0, 0, 0);
    lua_pushcfunction(L, luaK_kiwmi_keyboard_register);
    error |= lua_pcall(L, 0, 0, 0);
    lua_pushcfunction(L, luaK_kiwmi_output_register);
    error |= lua_pcall(L, 0, 0, 0);
    lua_pushcfunction(L, luaK_kiwmi_renderer_register);
    error |= lua_pcall(L, 0, 0, 0);
    lua_pushcfunction(L, luaK_kiwmi_server_register);
    error |= lua_pcall(L, 0, 0, 0);
    lua_pushcfunction(L, luaK_kiwmi_view_register);
    error |= lua_pcall(L, 0, 0, 0);

    if (error) {
        wlr_log(WLR_ERROR, "%s", lua_tostring(L, -1));
        lua_close(L);
        return NULL;
    }

    // create kiwmi global
    lua_pushcfunction(L, luaK_kiwmi_server_new);
    lua_pushlightuserdata(L, lua);
    lua_pushlightuserdata(L, server);
    if (lua_pcall(L, 2, 1, 0)) {
        wlr_log(WLR_ERROR, "%s", lua_tostring(L, -1));
        lua_close(L);
        free(lua);
        return NULL;
    }
    lua_setglobal(L, "kiwmi");

    if (!luaK_ipc_init(server, lua)) {
        wlr_log(WLR_ERROR, "Failed to initialize IPC");
        lua_close(L);
        free(lua);
        return NULL;
    }

    return lua;
}

bool
luaK_dofile(struct kiwmi_lua *lua, const char *config_path)
{
    int top = lua_gettop(lua->L);

    if (luaL_dofile(lua->L, config_path)) {
        wlr_log(
            WLR_ERROR, "Error running config: %s", lua_tostring(lua->L, -1));
        return false;
    }

    lua_pop(lua->L, top - lua_gettop(lua->L));

    return true;
}

void
luaK_destroy(struct kiwmi_lua *lua)
{
    lua_close(lua->L);

    struct kiwmi_lua_callback *lc;
    struct kiwmi_lua_callback *tmp;
    wl_list_for_each_safe (lc, tmp, &lua->scheduled_callbacks, link) {
        wl_event_source_remove(lc->event_source);
        wl_list_remove(&lc->link);
        free(lc);
    }

    free(lua);
}
