#include "luak/kiwmi_scene_node.h"
#include "color.h"
#include "lua.h"
#include "luak/luak.h"
#include "text_buffer.h"
#include <lauxlib.h>
#include <luajit-2.1/lua.h>
#include <stdbool.h>
#include <wlr/types/wlr_scene.h>

static const char tname[] = "kiwmi_scene_node";

static int
set_position(lua_State *L)
{
    struct kiwmi_object *obj =
        *(struct kiwmi_object **)luaL_checkudata(L, 1, tname);
    luaL_checktype(L, 2, LUA_TNUMBER); // x
    luaL_checktype(L, 3, LUA_TNUMBER); // y

    if (!obj->valid) {
        return luaL_error(L, "%s no longer valid", tname);
    }

    struct wlr_scene_node *node = obj->object;

    // TODO: some explicit rounding instead of just truncation?
    lua_Number x = lua_tonumber(L, 2);
    lua_Number y = lua_tonumber(L, 3);

    wlr_scene_node_set_position(node, x, y);

    return 0;
}

static int
set_size(lua_State *L)
{
    struct kiwmi_object *obj =
        *(struct kiwmi_object **)luaL_checkudata(L, 1, tname);
    luaL_checktype(L, 2, LUA_TNUMBER); // width
    luaL_checktype(L, 3, LUA_TNUMBER); // height

    if (!obj->valid) {
        return luaL_error(L, "%s no longer valid", tname);
    }

    struct wlr_scene_node *node = obj->object;

    if (node->type != WLR_SCENE_NODE_RECT) {
        return luaL_error(L, "node must be a rect");
    }

    struct wlr_scene_rect *rect = wl_container_of(node, rect, node);

    lua_Number width  = lua_tonumber(L, 2);
    lua_Number height = lua_tonumber(L, 3);

    wlr_scene_rect_set_size(rect, width, height);

    return 0;
}

static int
set_color(lua_State *L)
{
    struct kiwmi_object *obj =
        *(struct kiwmi_object **)luaL_checkudata(L, 1, tname);
    luaL_checktype(L, 2, LUA_TSTRING); // color

    if (!obj->valid) {
        return luaL_error(L, "%s no longer valid", tname);
    }

    struct wlr_scene_node *node = obj->object;

    if (node->type != WLR_SCENE_NODE_RECT) {
        return luaL_error(L, "node must be a rect");
    }

    struct wlr_scene_rect *rect = wl_container_of(node, rect, node);

    float color[4];
    if (!color_parse(lua_tostring(L, 2), color)) {
        return luaL_argerror(L, 2, "not a valid color");
    }

    wlr_scene_rect_set_color(rect, color);

    return 0;
}

static const luaL_Reg methods[] = {
    {"set_position", set_position},
    {"set_size", set_size},
    {"set_color", set_color},
    {NULL, NULL},
};

int
luaK_kiwmi_scene_node_new(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA); // struct kiwmi_lua
    luaL_checktype(L, 2, LUA_TLIGHTUSERDATA); // struct wlr_scene_node

    struct kiwmi_lua *lua       = lua_touserdata(L, 1);
    struct wlr_scene_node *node = lua_touserdata(L, 2);

    struct kiwmi_object *obj =
        luaK_get_kiwmi_object(lua, node, &node->events.destroy);

    struct kiwmi_object **view_ud = lua_newuserdata(L, sizeof(*view_ud));
    luaL_getmetatable(L, tname);
    lua_setmetatable(L, -2);

    *view_ud = obj;

    return 1;
}

int
luaK_kiwmi_scene_node_register(lua_State *L)
{
    luaL_newmetatable(L, tname);

    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, methods, 0);

    lua_pushcfunction(L, luaK_usertype_ref_equal);
    lua_setfield(L, -2, "__eq");

    lua_pushcfunction(L, luaK_kiwmi_object_gc);
    lua_setfield(L, -2, "__gc");

    return 0;
}
