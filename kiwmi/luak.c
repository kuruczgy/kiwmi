/* Copyright (c), Charlotte Meyer <dev@buffet.sh>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "luak.h"

#include <stdlib.h>

#include <lauxlib.h>
#include <lualib.h>
#include <wayland-server.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/util/log.h>

#include "desktop/view.h"
#include "input/cursor.h"
#include "input/input.h"

struct lua_callback {
    struct wl_list link;
    struct kiwmi_server *server;
    int callback_ref;
    struct wl_listener listener;
};

static int
l_lua_callback_cancel(lua_State *L)
{
    struct lua_callback *lc =
        *(struct lua_callback **)luaL_checkudata(L, 1, "lua_callback");
    lua_pop(L, 1);

    wl_list_remove(&lc->listener.link);
    wl_list_remove(&lc->link);

    luaL_unref(L, LUA_REGISTRYINDEX, lc->callback_ref);

    free(lc);

    return 0;
}

static const luaL_Reg lua_callback_methods[] = {
    {"cancel", l_lua_callback_cancel},
    {NULL, NULL},
};

static int
lua_callback_create(lua_State *L)
{
    struct lua_callback *lc = lua_touserdata(L, 1);

    struct lua_callback **lc_ud = lua_newuserdata(L, sizeof(*lc_ud));
    luaL_getmetatable(L, "lua_callback");
    lua_setmetatable(L, -2);

    *lc_ud = lc;

    return 1;
}

static int
lua_callback_register(lua_State *L)
{
    luaL_newmetatable(L, "lua_callback");
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);
    luaL_setfuncs(L, lua_callback_methods, 0);

    return 0;
}

static int
l_kiwmi_view_close(lua_State *L)
{
    struct kiwmi_view *view =
        *(struct kiwmi_view **)luaL_checkudata(L, 1, "kiwmi_view");

    view_close(view);

    return 0;
}

static int
l_kiwmi_view_focus(lua_State *L)
{
    struct kiwmi_view *view =
        *(struct kiwmi_view **)luaL_checkudata(L, 1, "kiwmi_view");

    view_focus(view);

    return 0;
}

static int
l_kiwmi_view_hidden(lua_State *L)
{
    struct kiwmi_view *view =
        *(struct kiwmi_view **)luaL_checkudata(L, 1, "kiwmi_view");

    lua_pushboolean(L, view->hidden);

    return 1;
}

static int
l_kiwmi_view_hide(lua_State *L)
{
    struct kiwmi_view *view =
        *(struct kiwmi_view **)luaL_checkudata(L, 1, "kiwmi_view");

    view->hidden = true;

    return 0;
}

static int
l_kiwmi_view_move(lua_State *L)
{
    struct kiwmi_view *view =
        *(struct kiwmi_view **)luaL_checkudata(L, 1, "kiwmi_view");

    luaL_checktype(L, 2, LUA_TNUMBER);
    luaL_checktype(L, 3, LUA_TNUMBER);

    view->x = lua_tonumber(L, 2);
    view->y = lua_tonumber(L, 3);

    return 0;
}

static int
l_kiwmi_view_show(lua_State *L)
{
    struct kiwmi_view *view =
        *(struct kiwmi_view **)luaL_checkudata(L, 1, "kiwmi_view");

    view->hidden = false;

    return 0;
}

static const luaL_Reg kiwmi_view_methods[] = {
    {"close", l_kiwmi_view_close},
    {"focus", l_kiwmi_view_focus},
    {"hidden", l_kiwmi_view_hidden},
    {"hide", l_kiwmi_view_hide},
    {"move", l_kiwmi_view_move},
    {"show", l_kiwmi_view_show},
    {NULL, NULL},
};

static int
kiwmi_view_create(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA); // kiwmi_view

    struct kiwmi_view *view = lua_touserdata(L, 1);

    struct kiwmi_view **view_ud = lua_newuserdata(L, sizeof(*view_ud));
    luaL_getmetatable(L, "kiwmi_view");
    lua_setmetatable(L, -2);

    *view_ud = view;

    return 1;
}

static int
kiwmi_view_register(lua_State *L)
{
    luaL_newmetatable(L, "kiwmi_view");
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);
    luaL_setfuncs(L, kiwmi_view_methods, 0);

    return 0;
}

static int
l_on(lua_State *L)
{
    luaL_checkudata(L, 1, "kiwmi_server"); // server
    luaL_checktype(L, 2, LUA_TSTRING);     // type
    luaL_checktype(L, 3, LUA_TFUNCTION);   // callback

    // event_handler = registry['kiwmi_events'][type]
    lua_pushliteral(L, "kiwmi_events");
    lua_gettable(L, LUA_REGISTRYINDEX);
    lua_pushvalue(L, 2);
    lua_gettable(L, -2);

    luaL_argcheck(L, lua_iscfunction(L, -1), 2, "invalid event");

    // stack: server type callback kiwmi_events register_callback

    lua_remove(L, 4); // remove kiwmi_events
    lua_remove(L, 2); // remove type
    lua_rotate(L, 1, 1);

    // stack: register_callback server callback

    // return register_callback(callback)
    if (lua_pcall(L, 2, 1, 0)) {
        wlr_log(WLR_ERROR, "%s", lua_tostring(L, -1));
        return 0;
    }

    return 1;
}

static void
on_cursor_button_notify(struct wl_listener *listener, void *data)
{
    struct lua_callback *lc     = wl_container_of(listener, lc, listener);
    struct kiwmi_server *server = lc->server;
    struct lua_State *L         = server->lua->L;
    struct wlr_event_pointer_button *event = data;

    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->callback_ref);

    lua_newtable(L);

    lua_pushlightuserdata(L, event->device); // TODO: make un-opaque
    lua_setfield(L, -2, "device");

    lua_pushinteger(L, event->time_msec);
    lua_setfield(L, -2, "time");

    lua_pushinteger(L, event->button);
    lua_setfield(L, -2, "button");

    lua_pushinteger(L, event->state);
    lua_setfield(L, -2, "state");

    if (lua_pcall(L, 1, 0, 0)) {
        wlr_log(WLR_ERROR, "%s", lua_tostring(L, -1));
    }
}

static void
on_view_notify(struct wl_listener *listener, void *data)
{
    struct lua_callback *lc     = wl_container_of(listener, lc, listener);
    struct kiwmi_server *server = lc->server;
    struct lua_State *L         = server->lua->L;
    struct kiwmi_view *view     = data;

    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->callback_ref);

    lua_pushcfunction(L, kiwmi_view_create);
    lua_pushlightuserdata(L, view);
    if (lua_pcall(L, 1, 1, 0)) {
        wlr_log(WLR_ERROR, "%s", lua_tostring(L, -1));
    }

    if (lua_pcall(L, 1, 0, 0)) {
        wlr_log(WLR_ERROR, "%s", lua_tostring(L, -1));
    }
}

static int
on_cursor_button(lua_State *L)
{
    struct kiwmi_server *server =
        *(struct kiwmi_server **)luaL_checkudata(L, 1, "kiwmi_server");
    luaL_checktype(L, 2, LUA_TFUNCTION);   // callback

    struct lua_callback *lc = malloc(sizeof(*lc));
    if (!lc) {
        return luaL_error(L, "failed to allocate lua_callback");
    }

    struct kiwmi_cursor *cursor = server->input.cursor;

    wl_list_insert(&server->lua->callbacks, &lc->link);

    lc->server       = server;
    lc->callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    lc->listener.notify = on_cursor_button_notify;
    wl_signal_add(&cursor->cursor->events.button, &lc->listener);

    lua_pushlightuserdata(L, lc);
    lua_callback_create(L);

    return 1;
}

static int
on_view(lua_State *L)
{
    struct kiwmi_server *server =
        *(struct kiwmi_server **)luaL_checkudata(L, 1, "kiwmi_server");
    luaL_checktype(L, 2, LUA_TFUNCTION);   // callback

    struct lua_callback *lc = malloc(sizeof(*lc));
    if (!lc) {
        return luaL_error(L, "failed to allocate lua_callback");
    }

    wl_list_insert(&server->lua->callbacks, &lc->link);

    lc->server       = server;
    lc->callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    lc->listener.notify = on_view_notify;
    wl_signal_add(&server->desktop.events.view_map, &lc->listener);

    lua_pushlightuserdata(L, lc);
    lua_callback_create(L);

    return 1;
}

static int
l_kiwmi_server_quit(lua_State *L)
{
    struct kiwmi_server *server = *(struct kiwmi_server**)luaL_checkudata(L, 1, "kiwmi_server");

    wl_display_terminate(server->wl_display);

    return 0;
}

static int
l_view_under_cursor(lua_State *L)
{
    struct kiwmi_server *server = *(struct kiwmi_server **)luaL_checkudata(L, 1, "kiwmi_server");

    struct kiwmi_cursor *cursor = server->input.cursor;

    struct wlr_surface *surface;
    double sx;
    double sy;

    struct kiwmi_view *view = view_at(
        &server->desktop,
        cursor->cursor->x,
        cursor->cursor->y,
        &surface,
        &sx,
        &sy);

    if (view) {
        lua_pushcfunction(L, kiwmi_view_create);
        lua_pushlightuserdata(L, view);
        if (lua_pcall(L, 1, 1, 0)) {
            wlr_log(WLR_ERROR, "%s", lua_tostring(L, -1));
        }
    } else {
        lua_pushnil(L);
    }

    return 1;
}

static const luaL_Reg kiwmi_server_methods[] = {
    {"on", l_on},
    {"quit", l_quit},
    {"view_under_cursor", l_view_under_cursor},
    {NULL, NULL},
};

static int
kiwmi_server_create(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA); // kiwmi_server

    struct kiwmi_server *server = lua_touserdata(L, 1);

    struct kiwmi_server **server_ud = lua_newuserdata(L, sizeof(*server_ud));
    luaL_getmetatable(L, "kiwmi_server");
    lua_setmetatable(L, -2);

    *server_ud = server;

    return 1;
}

static int
kiwmi_server_register(lua_State *L)
{
    luaL_newmetatable(L, "kiwmi_server");
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);
    luaL_setfuncs(L, kiwmi_server_methods, 0);

    return 0;
}

static const luaL_Reg kiwmi_events[] = {
    {"cursor_button", on_cursor_button},
    {"view", on_view},
    {NULL, NULL},
};

bool
luaK_init(struct kiwmi_server *server)
{
    struct kiwmi_lua *lua = malloc(sizeof(*lua));
    if (!lua) {
        wlr_log(WLR_ERROR, "Failed to allocate kiwmi_lua");
        return false;
    }

    lua_State *L = luaL_newstate();
    if (!L) {
        return false;
    }

    luaL_openlibs(L);

    // registry['kiwmi_events'] = {'key_down' = l_key_down, ...}
    lua_pushliteral(L, "kiwmi_events");
    luaL_newlib(L, kiwmi_events);
    lua_settable(L, LUA_REGISTRYINDEX);

    // register types
    int error = false;

    lua_pushcfunction(L, lua_callback_register);
    error |= lua_pcall(L, 0, 0, 0);
    lua_pushcfunction(L, kiwmi_view_register);
    error |= lua_pcall(L, 0, 0, 0);
    lua_pushcfunction(L, kiwmi_server_register);
    error |= lua_pcall(L, 0, 0, 0);

    if (error) {
        wlr_log(WLR_ERROR, "%s", lua_tostring(L, -1));
        return false;
    }

    // create kiwmi global
    lua_pushcfunction(L, kiwmi_server_create);
    lua_pushlightuserdata(L, server);
    if (lua_pcall(L, 1, 1, 0)) {
        wlr_log(WLR_ERROR, "%s", lua_tostring(L, -1));
        return false;
    }
    lua_setglobal(L, "kiwmi");

    lua->L      = L;
    server->lua = lua;

    wl_list_init(&lua->callbacks);

    return true;
}

bool
luaK_dofile(struct kiwmi_lua *lua, const char *config_path)
{
    if (luaL_dofile(lua->L, config_path)) {
        wlr_log(
            WLR_ERROR, "Error running config: %s", lua_tostring(lua->L, -1));
        return false;
    }

    return true;
}

void
luaK_fini(struct kiwmi_lua *lua)
{
    struct lua_callback *lc;
    struct lua_callback *tmp;
    wl_list_for_each_safe (lc, tmp, &lua->callbacks, link) {
        wl_list_remove(&lc->listener.link);
        wl_list_remove(&lc->link);

        luaL_unref(lua->L, LUA_REGISTRYINDEX, lc->callback_ref);

        free(lc);
    }

    lua_close(lua->L);
}
