/* Copyright (c), Charlotte Meyer <dev@buffet.sh>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "luak/kiwmi_cursor.h"

#include <linux/input-event-codes.h>

#include <lauxlib.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/util/log.h>

#include "desktop/view.h"
#include "input/cursor.h"
#include "luak/kiwmi_lua_callback.h"
#include "luak/kiwmi_view.h"
#include "luak/luak.h"

static int
l_kiwmi_cursor_pos(lua_State *L)
{
    struct kiwmi_cursor *cursor =
        *(struct kiwmi_cursor **)luaL_checkudata(L, 1, "kiwmi_cursor");

    lua_pushnumber(L, cursor->cursor->x);
    lua_pushnumber(L, cursor->cursor->y);

    return 2;
}

static int
l_kiwmi_cursor_view_at_pos(lua_State *L)
{
    struct kiwmi_cursor *cursor =
        *(struct kiwmi_cursor **)luaL_checkudata(L, 1, "kiwmi_cursor");

    struct kiwmi_server *server = cursor->server;

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
        lua_pushcfunction(L, luaK_kiwmi_view_new);
        lua_pushlightuserdata(L, view);
        if (lua_pcall(L, 1, 1, 0)) {
            wlr_log(WLR_ERROR, "%s", lua_tostring(L, -1));
            return 0;
        }
    } else {
        lua_pushnil(L);
    }

    return 1;
}

static const luaL_Reg kiwmi_cursor_methods[] = {
    {"on", luaK_callback_register_dispatch},
    {"pos", l_kiwmi_cursor_pos},
    {"view_at_pos", l_kiwmi_cursor_view_at_pos},
    {NULL, NULL},
};

static void
kiwmi_cursor_on_button_down_or_up_notify(
    struct wl_listener *listener,
    void *data)
{
    struct kiwmi_lua_callback *lc = wl_container_of(listener, lc, listener);
    struct kiwmi_server *server   = lc->server;
    lua_State *L                  = server->lua->L;
    struct kiwmi_cursor_button_event *event = data;

    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->callback_ref);

    lua_pushinteger(L, event->wlr_event->button - BTN_LEFT + 1);

    if (lua_pcall(L, 1, 1, 0)) {
        wlr_log(WLR_ERROR, "%s", lua_tostring(L, -1));
        lua_pop(L, 1);
        return;
    }

    event->handled |= lua_toboolean(L, -1);
    lua_pop(L, 1);
}

static void
kiwmi_cursor_on_motion_notify(struct wl_listener *listener, void *data)
{
    struct kiwmi_lua_callback *lc = wl_container_of(listener, lc, listener);
    struct kiwmi_server *server   = lc->server;
    lua_State *L                  = server->lua->L;
    struct kiwmi_cursor_motion_event *event = data;

    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->callback_ref);

    lua_newtable(L);

    lua_pushnumber(L, event->oldx);
    lua_setfield(L, -2, "oldx");

    lua_pushnumber(L, event->oldy);
    lua_setfield(L, -2, "oldy");

    lua_pushnumber(L, event->newx);
    lua_setfield(L, -2, "newx");

    lua_pushnumber(L, event->newy);
    lua_setfield(L, -2, "newy");

    if (lua_pcall(L, 1, 0, 0)) {
        wlr_log(WLR_ERROR, "%s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

static int
l_kiwmi_cursor_on_button_down(lua_State *L)
{
    struct kiwmi_cursor *cursor =
        *(struct kiwmi_cursor **)luaL_checkudata(L, 1, "kiwmi_cursor");
    luaL_checktype(L, 2, LUA_TFUNCTION);

    struct kiwmi_server *server = cursor->server;

    lua_pushcfunction(L, luaK_kiwmi_lua_callback_new);
    lua_pushlightuserdata(L, server);
    lua_pushvalue(L, 2);
    lua_pushlightuserdata(L, kiwmi_cursor_on_button_down_or_up_notify);
    lua_pushlightuserdata(L, &cursor->events.button_down);

    if (lua_pcall(L, 4, 1, 0)) {
        wlr_log(WLR_ERROR, "%s", lua_tostring(L, -1));
        return 0;
    }

    return 1;
}

static int
l_kiwmi_cursor_on_button_up(lua_State *L)
{
    struct kiwmi_cursor *cursor =
        *(struct kiwmi_cursor **)luaL_checkudata(L, 1, "kiwmi_cursor");
    luaL_checktype(L, 2, LUA_TFUNCTION);

    struct kiwmi_server *server = cursor->server;

    lua_pushcfunction(L, luaK_kiwmi_lua_callback_new);
    lua_pushlightuserdata(L, server);
    lua_pushvalue(L, 2);
    lua_pushlightuserdata(L, kiwmi_cursor_on_button_down_or_up_notify);
    lua_pushlightuserdata(L, &cursor->events.button_up);

    if (lua_pcall(L, 4, 1, 0)) {
        wlr_log(WLR_ERROR, "%s", lua_tostring(L, -1));
        return 0;
    }

    return 1;
}

static int
l_kiwmi_cursor_on_motion(lua_State *L)
{
    struct kiwmi_cursor *cursor =
        *(struct kiwmi_cursor **)luaL_checkudata(L, 1, "kiwmi_cursor");
    luaL_checktype(L, 2, LUA_TFUNCTION);

    struct kiwmi_server *server = cursor->server;

    lua_pushcfunction(L, luaK_kiwmi_lua_callback_new);
    lua_pushlightuserdata(L, server);
    lua_pushvalue(L, 2);
    lua_pushlightuserdata(L, kiwmi_cursor_on_motion_notify);
    lua_pushlightuserdata(L, &cursor->events.motion);

    if (lua_pcall(L, 4, 1, 0)) {
        wlr_log(WLR_ERROR, "%s", lua_tostring(L, -1));
        return 0;
    }

    return 1;
}

static const luaL_Reg kiwmi_cursor_events[] = {
    {"button_down", l_kiwmi_cursor_on_button_down},
    {"button_up", l_kiwmi_cursor_on_button_up},
    {"motion", l_kiwmi_cursor_on_motion},
    {NULL, NULL},
};

int
luaK_kiwmi_cursor_new(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA); // kiwmi_cursor

    struct kiwmi_cursor *cursor = lua_touserdata(L, 1);

    struct kiwmi_cursor **cursor_ud = lua_newuserdata(L, sizeof(*cursor_ud));
    luaL_getmetatable(L, "kiwmi_cursor");
    lua_setmetatable(L, -2);

    *cursor_ud = cursor;

    return 1;
}

int
luaK_kiwmi_cursor_register(lua_State *L)
{
    luaL_newmetatable(L, "kiwmi_cursor");

    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, kiwmi_cursor_methods, 0);

    luaL_newlib(L, kiwmi_cursor_events);
    lua_setfield(L, -2, "__events");

    lua_pushcfunction(L, luaK_usertype_ref_equal);
    lua_setfield(L, -2, "__eq");

    return 0;
}
