#include "luak/kiwmi_scene_tree.h"
#include "color.h"
#include "lua.h"
#include "luak/kiwmi_scene_node.h"
#include "luak/luak.h"
#include "text_buffer.h"
#include <lauxlib.h>
#include <stdbool.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

static const char tname[] = "kiwmi_scene_tree";

static int
add_rect(lua_State *L)
{
    struct kiwmi_object *obj =
        *(struct kiwmi_object **)luaL_checkudata(L, 1, tname);
    luaL_checktype(L, 2, LUA_TNUMBER); // width
    luaL_checktype(L, 3, LUA_TNUMBER); // height
    luaL_checktype(L, 4, LUA_TSTRING); // color

    if (!obj->valid) {
        return luaL_error(L, "%s no longer valid", tname);
    }

    struct wlr_scene_tree *tree = obj->object;

    lua_Integer width  = lua_tointeger(L, 2);
    lua_Integer height = lua_tointeger(L, 3);

    float color[4];
    if (!color_parse(lua_tostring(L, 4), color)) {
        return luaL_argerror(L, 4, "not a valid color");
    }

    struct wlr_scene_rect *rect =
        wlr_scene_rect_create(tree, width, height, color);

    lua_pushcfunction(L, luaK_kiwmi_scene_node_new);
    lua_pushlightuserdata(L, obj->lua);
    lua_pushlightuserdata(L, &rect->node);
    if (lua_pcall(L, 2, 1, 0)) {
        wlr_log(WLR_ERROR, "%s", lua_tostring(L, -1));
        return 0;
    }

    return 1;
}

static int
add_text(lua_State *L)
{
    struct kiwmi_object *obj =
        *(struct kiwmi_object **)luaL_checkudata(L, 1, tname);
    luaL_checktype(L, 2, LUA_TSTRING); // text
    luaL_checktype(L, 3, LUA_TSTRING); // color

    if (!obj->valid) {
        return luaL_error(L, "%s no longer valid", tname);
    }

    struct wlr_scene_tree *tree = obj->object;

    float color[4];
    if (!color_parse(lua_tostring(L, 3), color)) {
        return luaL_argerror(L, 3, "not a valid color");
    }

    const char *text = lua_tostring(L, 2);

    struct text_node *text_node = text_node_create(
        tree, obj->lua->server->font_description, text, color, false);

    lua_pushcfunction(L, luaK_kiwmi_scene_node_new);
    lua_pushlightuserdata(L, obj->lua);
    lua_pushlightuserdata(L, text_node->node);
    if (lua_pcall(L, 2, 1, 0)) {
        wlr_log(WLR_ERROR, "%s", lua_tostring(L, -1));
        return 0;
    }

    return 1;
}

static const luaL_Reg methods[] = {
    {"add_rect", add_rect},
    {"add_text", add_text},
    {NULL, NULL},
};

int
luaK_kiwmi_scene_tree_new(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA); // struct kiwmi_lua
    luaL_checktype(L, 2, LUA_TLIGHTUSERDATA); // struct wlr_scene_tree

    struct kiwmi_lua *lua       = lua_touserdata(L, 1);
    struct wlr_scene_tree *tree = lua_touserdata(L, 2);

    struct kiwmi_object *obj =
        luaK_get_kiwmi_object(lua, tree, &tree->node.events.destroy);

    struct kiwmi_object **view_ud = lua_newuserdata(L, sizeof(*view_ud));
    luaL_getmetatable(L, tname);
    lua_setmetatable(L, -2);

    *view_ud = obj;

    return 1;
}

int
luaK_kiwmi_scene_tree_register(lua_State *L)
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
