#ifndef KIWMI_WEBSOCKET_H
#define KIWMI_WEBSOCKET_H

#include <lua.h>
#include <wayland-server.h>

struct websocket;

struct websocket *
websocket_init(struct lua_State *L, struct wl_event_loop *event_loop);
void websocket_fini(struct websocket *data);
int websocket_send_msg(struct lua_State *L);
void websocket_register_callbacks(
    struct websocket *self,
    int connect_ref,
    int recv_ref,
    int close_ref);

#endif
