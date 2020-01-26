/* Copyright (c), Charlotte Meyer <dev@buffet.sh>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "luak/kiwmi_renderer.h"

#include <stdlib.h>
#include <string.h>

#include <lauxlib.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>

#include "desktop/output.h"
#include "luak/luak.h"

struct kiwmi_renderer {
    struct wlr_renderer *wlr_renderer;
    struct kiwmi_output *output;
};

static bool
parse_color(const char *hex, float color[static 4])
{
    if (hex[0] == '#') {
        ++hex;
    }

    int len = strlen(hex);
    if (len != 6 && len != 8) {
        return false;
    }

    uint32_t rgba = (uint32_t)strtoul(hex, NULL, 16);
    if (len == 6) {
        rgba = (rgba << 8) | 0xff;
    }

    for (size_t i = 0; i < 4; ++i) {
        color[3 - i] = (rgba & 0xff) / 255.0;
        rgba >>= 8;
    }

    // premultiply alpha
    color[0] *= color[3];
    color[1] *= color[3];
    color[2] *= color[3];

    return true;
}

static int
l_kiwmi_renderer_draw_rect(lua_State *L)
{
    struct kiwmi_renderer *renderer =
        (struct kiwmi_renderer *)luaL_checkudata(L, 1, "kiwmi_renderer");
    luaL_checktype(L, 2, LUA_TSTRING); // color
    luaL_checktype(L, 3, LUA_TNUMBER); // x
    luaL_checktype(L, 4, LUA_TNUMBER); // y
    luaL_checktype(L, 5, LUA_TNUMBER); // width
    luaL_checktype(L, 6, LUA_TNUMBER); // height

    struct wlr_renderer *wlr_renderer = renderer->wlr_renderer;
    struct kiwmi_output *output       = renderer->output;
    struct wlr_output *wlr_output     = output->wlr_output;

    float color[4];
    if (!parse_color(lua_tostring(L, 2), color)) {
        return luaL_argerror(L, 2, "not a valid color");
    }

    struct wlr_box box = {
        .x      = lua_tonumber(L, 3),
        .y      = lua_tonumber(L, 4),
        .width  = lua_tonumber(L, 5),
        .height = lua_tonumber(L, 6),
    };

    wlr_render_rect(wlr_renderer, &box, color, wlr_output->transform_matrix);

    return 0;
}

static const luaL_Reg kiwmi_renderer_methods[] = {
    {"draw_rect", l_kiwmi_renderer_draw_rect},
    {NULL, NULL},
};

int
luaK_kiwmi_renderer_new(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA); // wlr_renderer
    luaL_checktype(L, 2, LUA_TLIGHTUSERDATA); // wlr_output

    struct wlr_renderer *wlr_renderer = lua_touserdata(L, 1);
    struct kiwmi_output *output       = lua_touserdata(L, 2);

    struct kiwmi_renderer *renderer_ud =
        lua_newuserdata(L, sizeof(*renderer_ud));
    luaL_getmetatable(L, "kiwmi_renderer");
    lua_setmetatable(L, -2);

    renderer_ud->wlr_renderer = wlr_renderer;
    renderer_ud->output       = output;

    return 1;
}

int
luaK_kiwmi_renderer_register(lua_State *L)
{
    luaL_newmetatable(L, "kiwmi_renderer");

    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, kiwmi_renderer_methods, 0);

    lua_pushcfunction(L, luaK_usertype_ref_equal);
    lua_setfield(L, -2, "__eq");

    return 0;
}
