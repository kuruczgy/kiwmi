
#include "websocket.h"
#include "lua.h"

#include <assert.h>
#include <lauxlib.h>
#include <libwebsockets.h>
#include <lua.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <wayland-util.h>
#include <wlr/util/log.h>

const size_t RX_BUFFER_BYTES = 512;

struct context_user_data {
    lua_State *L;

    int connect_ref, recv_ref, close_ref;

    struct lws_context *context;
};

struct json_parse {
    lua_State *L; // copy of L here as well for convenience
    struct lejp_ctx lejp_ctx;
    bool rejected;
    int stack_ref, key_ref, res_ref;
    int stack_top;
};

struct per_session_storage {
    struct json_parse json_parse;
    struct wl_list send_queue;
};

struct send_msg {
    struct wl_list link; // per_session_storage::send_queue
    unsigned char *buf;  // size len + LWS_PRE
    size_t len;
};

static char *
to_json(lua_State *L, size_t *len)
{
    char *buf = realloc(NULL, 1024);
    size_t i = LWS_PRE, cap = 1024;
#define GROW(space)                                                            \
    if (i + (space) >= cap)                                                    \
        cap = (cap + space) * 1.5, buf = realloc(buf, cap);

    struct {
        size_t len, index;
        bool is_array;
    } stack[128];
    size_t stack_len = 0;

    luaL_checkany(L, 1);

    int bottom = lua_gettop(L) - 1;

    while (true) {
        lua_checkstack(L, 3);

        int type = lua_type(L, -1);

        if (type == LUA_TTABLE) {
            lua_rawgeti(L, -1, 1);
            type = lua_type(L, -1);
            lua_pop(L, 1);
            if (type == LUA_TNIL) {
                stack[stack_len].is_array = false;
                lua_pushnil(L);
            } else {
                stack[stack_len].is_array = true;
                stack[stack_len].index    = 0;
                stack[stack_len].len      = lua_objlen(L, -1);
            }
            ++stack_len;
        } else {
            if (type != LUA_TNUMBER) {
                GROW(1);
                buf[i++] = '"';
            }

            size_t len;
            const char *str = lua_tolstring(L, -1, &len);
            GROW(len);
            memcpy(buf + i, str, len);
            i += len;
            lua_pop(L, 1);

            if (type != LUA_TNUMBER) {
                GROW(1);
                buf[i++] = '"';
            }
        }

        while (true) {
            if (lua_gettop(L) == bottom)
                goto end;

            if (stack[stack_len - 1].is_array) {
                ++stack[stack_len - 1].index;
                if (stack[stack_len - 1].index <= stack[stack_len - 1].len) {
                    if (stack[stack_len - 1].index == 1) {
                        // first item in the array
                        GROW(1);
                        buf[i++] = '[';
                    } else {
                        GROW(1);
                        buf[i++] = ',';
                    }
                    lua_rawgeti(L, -1, stack[stack_len - 1].index);
                    break;
                }
            } else {
                type = lua_type(L, -1);
                if (lua_next(L, -2) != 0) {
                    if (type == LUA_TNIL) {
                        // first pair in the table
                        GROW(1);
                        buf[i++] = '{';
                    } else {
                        GROW(1);
                        buf[i++] = ',';
                    }

                    lua_pushvalue(L, -2);
                    size_t len;
                    const char *str = lua_tolstring(L, -1, &len);
                    GROW(len + 3);
                    buf[i++] = '"';
                    memcpy(buf + i, str, len);
                    i += len;
                    buf[i++] = '"';
                    buf[i++] = ':';
                    lua_pop(L, 1);

                    break;
                }
            }
            lua_pop(L, 1);
            GROW(1);

            buf[i++] = stack[stack_len - 1].is_array ? ']' : '}';
            --stack_len;
        }
    }
end:
    *len = i - LWS_PRE;
    return buf;

#undef GROW
}

void
websocket_register_callbacks(
    struct websocket *self,
    int connect_ref,
    int recv_ref,
    int close_ref)
{
    struct context_user_data *ctx = (struct context_user_data *)self;

    if (ctx->connect_ref != LUA_NOREF)
        luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->connect_ref);
    if (ctx->recv_ref != LUA_NOREF)
        luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->recv_ref);
    if (ctx->close_ref != LUA_NOREF)
        luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->close_ref);

    ctx->connect_ref = connect_ref;
    ctx->recv_ref    = recv_ref;
    ctx->close_ref   = close_ref;
}

int
websocket_send_msg(struct lua_State *L)
{
    luaL_checkudata(L, 1, "kiwmi_server");
    luaL_checktype(L, 2, LUA_TLIGHTUSERDATA);
    luaL_checkany(L, 3);

    struct lws *wsi                 = lua_touserdata(L, 2);
    struct per_session_storage *pss = lws_wsi_user(wsi);

    struct send_msg *send_msg = malloc(sizeof(*send_msg));

    lua_pushvalue(L, 3);
    send_msg->buf = (unsigned char *)to_json(L, &send_msg->len);
    // const char *str = lua_tolstring(L, 3, &send_msg->len);
    // send_msg->buf   = malloc(send_msg->len + LWS_PRE);
    // memcpy(send_msg->buf + LWS_PRE, str, send_msg->len);
    // printf("send_msg: %.*s\n", (int)send_msg->len, send_msg->buf + LWS_PRE);

    wl_list_insert(&pss->send_queue, &send_msg->link);

    lws_callback_on_writable(wsi);

    return 0;
}

// static void
// append_message_fragment(
//     struct per_session_storage *pss,
//     void *in,
//     size_t len,
//     size_t remaining)
// {
//     // Make sure that `len` more fits into `pss->msg_buffer`
//     if (!pss->recv_buffer || pss->recv_len + len > pss->recv_buffer_len) {
//         pss->recv_buffer_len = pss->recv_len + len + remaining;
//         pss->recv_buffer     = realloc(pss->recv_buffer, pss->recv_buffer_len);
//     }

//     // Append to buffer
//     memcpy(pss->recv_buffer + pss->recv_len, in, len);
//     pss->recv_len += len;
// }

static void
done_message(
    struct context_user_data *ctx,
    struct per_session_storage *pss,
    struct lws *wsi)
{
    // printf("done_message %.*s\n", (int)pss->recv_len, pss->recv_buffer);

    if (ctx->recv_ref != LUA_NOREF) {
        lua_checkstack(ctx->L, 3); // TODO: needed?
        lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->recv_ref);
        lua_pushlightuserdata(ctx->L, wsi);

        // lua_pushlstring(ctx->L, pss->recv_buffer, pss->recv_len);
        lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, pss->json_parse.res_ref);

        if (lua_pcall(ctx->L, 2, 0, 0)) {
            wlr_log(WLR_ERROR, "%s", lua_tostring(ctx->L, -1));
            lua_pop(ctx->L, 1);
        }
    }

    // pss->recv_len = 0;
}

signed char
json_cb(struct lejp_ctx *json_ctx, char reason)
{
    struct json_parse *jp = json_ctx->user;

    switch (reason) {
    case LEJPCB_PAIR_NAME:
        lua_checkstack(jp->L, 1);
        lua_pushstring(jp->L, json_ctx->path + json_ctx->st[json_ctx->sp].p);
        jp->key_ref = luaL_ref(jp->L, LUA_REGISTRYINDEX);
        break;
    case LEJPCB_VAL_TRUE:
    case LEJPCB_VAL_FALSE:
    case LEJPCB_VAL_NULL:
    case LEJPCB_VAL_NUM_INT:
    case LEJPCB_VAL_NUM_FLOAT:
    case LEJPCB_VAL_STR_END:
        lua_checkstack(jp->L, 4);

        // get the stack
        lua_rawgeti(jp->L, LUA_REGISTRYINDEX, jp->stack_ref);

        // get the table
        lua_rawgeti(jp->L, -1, jp->stack_top);

        // get the key
        if (jp->key_ref != LUA_NOREF) {
            // object
            lua_rawgeti(jp->L, LUA_REGISTRYINDEX, jp->key_ref);
            luaL_unref(jp->L, LUA_REGISTRYINDEX, jp->key_ref);
            jp->key_ref = LUA_NOREF;
        } else {
            // array
            lua_pushnumber(jp->L, json_ctx->i[json_ctx->ipos - 1] + 1);
        }

        // create the value
        switch (reason) {
        case LEJPCB_VAL_TRUE:
            lua_pushboolean(jp->L, true);
            break;
        case LEJPCB_VAL_FALSE:
            lua_pushboolean(jp->L, false);
            break;
        case LEJPCB_VAL_NULL:
            lua_pushnil(jp->L);
            break;
        case LEJPCB_VAL_NUM_INT:
            lua_pushinteger(jp->L, atoll(json_ctx->buf));
            break;
        case LEJPCB_VAL_NUM_FLOAT:
            lua_pushnumber(jp->L, atof(json_ctx->buf));
            break;
        case LEJPCB_VAL_STR_END:
            lua_pushstring(jp->L, json_ctx->buf);
            break;
        }

        // set the value in the table
        lua_rawset(jp->L, -3);

        lua_pop(jp->L, 2);
        break;
    case LEJPCB_OBJECT_START:
    case LEJPCB_ARRAY_START:
        lua_checkstack(jp->L, 5);

        // get the stack
        lua_rawgeti(jp->L, LUA_REGISTRYINDEX, jp->stack_ref);

        // create the new table
        lua_newtable(jp->L);

        if (jp->stack_top > 0) {
            // get the table
            lua_rawgeti(jp->L, -2, jp->stack_top);

            // get the key
            if (jp->key_ref != LUA_NOREF) {
                // object
                lua_rawgeti(jp->L, LUA_REGISTRYINDEX, jp->key_ref);
                luaL_unref(jp->L, LUA_REGISTRYINDEX, jp->key_ref);
                jp->key_ref = LUA_NOREF;
            } else {
                // array
                lua_pushnumber(jp->L, json_ctx->i[json_ctx->ipos - 1] + 1);
            }

            // duplicate the new table
            lua_pushvalue(jp->L, -3);

            // set the new table in the table
            lua_rawset(jp->L, -3);

            lua_pop(jp->L, 1);
        }

        // push the new table on the stack
        lua_rawseti(jp->L, -2, ++jp->stack_top);

        lua_pop(jp->L, 1);
        break;
    case LEJPCB_OBJECT_END:
    case LEJPCB_ARRAY_END:
        lua_checkstack(jp->L, 2);

        // get the stack
        lua_rawgeti(jp->L, LUA_REGISTRYINDEX, jp->stack_ref);

        if (jp->stack_top == 1) {
            lua_rawgeti(jp->L, -1, jp->stack_top);
            jp->res_ref = luaL_ref(jp->L, LUA_REGISTRYINDEX);
        }

        // pop the table from the stack
        lua_pushnil(jp->L);
        lua_rawseti(jp->L, -2, jp->stack_top--);

        lua_pop(jp->L, 1);
        break;
    }

    return 0;
}

static int
cb_main(
    struct lws *wsi,
    enum lws_callback_reasons reason,
    void *user,
    void *in,
    size_t len)
{
    struct context_user_data *ctx   = lws_context_user(lws_get_context(wsi));
    struct per_session_storage *pss = user;

    switch (reason) {
    case LWS_CALLBACK_ESTABLISHED: {
        // printf("connected, calling %d\n", ctx->connect_ref);

        wl_list_init(&pss->send_queue);

        // set up json_parse
        pss->json_parse.L = ctx->L;
        lejp_construct(
            &pss->json_parse.lejp_ctx, json_cb, &pss->json_parse, NULL, 0);
        lua_checkstack(ctx->L, 1);
        lua_newtable(ctx->L);
        pss->json_parse.stack_ref = luaL_ref(ctx->L, LUA_REGISTRYINDEX);
        pss->json_parse.key_ref   = LUA_NOREF;
        pss->json_parse.res_ref   = LUA_NOREF;
        pss->json_parse.stack_top = 0;

        // pss->recv_buffer     = NULL;
        // pss->recv_buffer_len = 0;
        // pss->recv_len        = 0;

        if (ctx->connect_ref != LUA_NOREF) {
            lua_checkstack(ctx->L, 2);
            lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->connect_ref);
            lua_pushlightuserdata(ctx->L, wsi);
            if (lua_pcall(ctx->L, 1, 0, 0)) {
                wlr_log(WLR_ERROR, "%s", lua_tostring(ctx->L, -1));
                lua_pop(ctx->L, 1);
            }
        }

        break;
    }
    case LWS_CALLBACK_CLOSED: {
        if (ctx->close_ref != LUA_NOREF) {
            lua_checkstack(ctx->L, 2);
            lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->close_ref);
            lua_pushlightuserdata(ctx->L, wsi);
            if (lua_pcall(ctx->L, 1, 0, 0)) {
                wlr_log(WLR_ERROR, "%s", lua_tostring(ctx->L, -1));
                lua_pop(ctx->L, 1);
            }
        }

        // free(pss->recv_buffer);
        break;
    }
    case LWS_CALLBACK_RECEIVE: {
        const size_t remaining = lws_remaining_packet_payload(wsi);
        // printf("fragment: %.*s\n", (int)len, (const char *)in);

        if (!pss->json_parse.rejected) {
            if (lejp_parse(&pss->json_parse.lejp_ctx, in, len) < 0) {
                pss->json_parse.rejected = true;
            }
        }
        // append_message_fragment(pss, in, len, remaining);

        if (remaining == 0 && lws_is_final_fragment(wsi)) {
            if (!pss->json_parse.rejected) {
                done_message(ctx, pss, wsi);
            }

            luaL_unref(ctx->L, LUA_REGISTRYINDEX, pss->json_parse.res_ref);
            pss->json_parse.res_ref = LUA_NOREF;

            luaL_unref(ctx->L, LUA_REGISTRYINDEX, pss->json_parse.key_ref);
            pss->json_parse.key_ref = LUA_NOREF;

            luaL_unref(ctx->L, LUA_REGISTRYINDEX, pss->json_parse.stack_ref);
            pss->json_parse.stack_ref = LUA_NOREF;

            // reinit json_parse
            pss->json_parse.rejected = false;
            lejp_construct(
                &pss->json_parse.lejp_ctx, json_cb, &pss->json_parse, NULL, 0);
            lua_checkstack(ctx->L, 1);
            lua_newtable(ctx->L);
            luaL_unref(ctx->L, LUA_REGISTRYINDEX, pss->json_parse.stack_ref);
            pss->json_parse.stack_ref = luaL_ref(ctx->L, LUA_REGISTRYINDEX);
            pss->json_parse.stack_top = 0;
        }
        break;
    }
    case LWS_CALLBACK_SERVER_WRITEABLE: {
        struct send_msg *i, *tmp;
        wl_list_for_each_safe (i, tmp, &pss->send_queue, link) {
            lws_write(wsi, i->buf + LWS_PRE, i->len, LWS_WRITE_TEXT);
            free(i->buf);
            wl_list_remove(&i->link);
            free(i);
        }
        break;
    }
    default:
        break;
    }

    return 0;
}

int
event_source_cb(int fd, uint32_t mask, void *data)
{
    struct lws_context *context = data;

    struct lws_pollfd p = {
        .fd      = fd,
        .events  = 0,
        .revents = 0,
    };
    if (mask & WL_EVENT_READABLE)
        p.events |= POLLIN;
    if (mask & WL_EVENT_WRITABLE)
        p.events |= POLLOUT;
    p.revents = p.events;
    lws_service_fd(context, &p);

    // printf(
    //     "%s context=%p fd=%d wsi=%p mask=%d, srv_r=%d\n",
    //     __func__,
    //     context,
    //     fd,
    //     wsi,
    //     mask,
    //     srv_r);

    // idk what could go wrong? this is supposed to be at the end of the event loop
    while (lws_service_adjust_timeout(context, 1, 0) == 0) {
        lws_service_tsi(context, -1, 0);
    }

    return 0;
}

struct pt_eventlibs_custom {
    struct wl_event_loop *event_loop;
    struct context_user_data *ctx;
};
struct wsi_eventlibs_custom {
    struct wl_event_source *event_source;
    uint32_t event_mask;
};

// HACK: Should be `wsi->evlib_wsi`, but we don't have access to the headers.
// Offset obtained by looking at the memory in gdb.
// #define wsi_to_priv(wsi) ((void *)wsi + 0x178)

static int
init_pt_custom(struct lws_context *cx, void *loop, int tsi)
{
    struct pt_eventlibs_custom *priv =
        (struct pt_eventlibs_custom *)lws_evlib_tsi_to_evlib_pt(cx, tsi);
    *priv = *(struct pt_eventlibs_custom *)loop;
    // printf("%s priv=%p loop=%p\n", __func__, priv, loop);
    return 0;
}

static int
sock_accept_custom(struct lws *wsi)
{
    struct lws_context *context      = lws_get_context(wsi);
    struct pt_eventlibs_custom *priv = lws_evlib_wsi_to_evlib_pt(wsi);
    struct context_user_data *ctx    = priv->ctx;

    // struct wsi_eventlibs_custom *priv_wsi = wsi_to_priv(wsi);
    // create lua registry entry
    lua_pushlightuserdata(ctx->L, wsi);
    struct wsi_eventlibs_custom *priv_wsi =
        lua_newuserdata(ctx->L, sizeof(struct wsi_eventlibs_custom));
    lua_rawset(ctx->L, LUA_REGISTRYINDEX);

    // memset(priv_wsi, 'A', sizeof(struct wsi_eventlibs_custom) + 128);

    int fd = lws_get_socket_fd(wsi);
    // printf(
    //     "%s context=%p priv=%p fd=%d priv_wsi=%p ctx=%p\n",
    //     __func__,
    //     context,
    //     priv,
    //     fd,
    //     priv_wsi,
    //     ctx);

    priv_wsi->event_mask   = WL_EVENT_READABLE;
    priv_wsi->event_source = wl_event_loop_add_fd(
        priv->event_loop, fd, priv_wsi->event_mask, event_source_cb, context);

    // priv_wsi->event_mask     = POLLIN;
    // struct epoll_event event = {
    //     .data   = {.fd = fd},
    //     .events = priv_wsi->event_mask,
    // };

    // if (epoll_ctl(global_efd, EPOLL_CTL_ADD, fd, &event) == -1)
    //     abort();

    return 0;
}

static void
io_custom(struct lws *wsi, unsigned int flags)
{
    struct pt_eventlibs_custom *priv = lws_evlib_wsi_to_evlib_pt(wsi);
    struct context_user_data *ctx    = priv->ctx;

    // struct wsi_eventlibs_custom *priv_wsi = wsi_to_priv(wsi);
    lua_checkstack(ctx->L, 3);
    lua_pushlightuserdata(ctx->L, wsi);
    lua_rawget(ctx->L, LUA_REGISTRYINDEX);
    struct wsi_eventlibs_custom *priv_wsi = lua_touserdata(ctx->L, -1);
    lua_pop(ctx->L, 1);

    uint32_t event_mask = priv_wsi->event_mask;
    if (flags & LWS_EV_START) {
        if (flags & LWS_EV_WRITE)
            event_mask |= WL_EVENT_WRITABLE;
        if (flags & LWS_EV_READ)
            event_mask |= WL_EVENT_READABLE;
    } else {
        if (flags & LWS_EV_WRITE)
            event_mask &= ~WL_EVENT_WRITABLE;
        if (flags & LWS_EV_READ)
            event_mask &= ~WL_EVENT_READABLE;
    }
    // if (flags & LWS_EV_START) {
    //     if (flags & LWS_EV_WRITE)
    //         event_mask |= POLLOUT;
    //     if (flags & LWS_EV_READ)
    //         event_mask |= POLLIN;
    // } else {
    //     if (flags & LWS_EV_WRITE)
    //         event_mask &= ~POLLOUT;
    //     if (flags & LWS_EV_READ)
    //         event_mask &= ~POLLIN;
    // }

    // printf("%s event_mask=%d\n", __func__, event_mask);

    if (event_mask == priv_wsi->event_mask)
        return;
    priv_wsi->event_mask = event_mask;

    wl_event_source_fd_update(priv_wsi->event_source, priv_wsi->event_mask);
    // int fd                   = lws_get_socket_fd(wsi);
    // struct epoll_event event = {
    //     .data   = {.fd = fd},
    //     .events = priv_wsi->event_mask,
    // };
    // if (epoll_ctl(global_efd, EPOLL_CTL_MOD, fd, &event) == -1) {
    //     // So.. let's just ignore this for now
    // }
}

static int
wsi_logical_close_custom(struct lws *wsi)
{
    struct pt_eventlibs_custom *priv = lws_evlib_wsi_to_evlib_pt(wsi);
    struct context_user_data *ctx    = priv->ctx;

    // struct wsi_eventlibs_custom *priv_wsi = wsi_to_priv(wsi);
    lua_checkstack(ctx->L, 3);
    lua_pushlightuserdata(ctx->L, wsi);
    lua_rawget(ctx->L, LUA_REGISTRYINDEX);
    struct wsi_eventlibs_custom *priv_wsi = lua_touserdata(ctx->L, -1);
    lua_pop(ctx->L, 1);

    // printf("%s\n", __func__);
    wl_event_source_remove(priv_wsi->event_source);

    // int fd = lws_get_socket_fd(wsi);
    // if (epoll_ctl(global_efd, EPOLL_CTL_DEL, fd, NULL) == -1)
    //     abort();

    return 0;
}

static const struct lws_event_loop_ops event_loop_ops_custom = {
    .name                  = "custom",
    .init_pt               = init_pt_custom,
    .init_vhost_listen_wsi = sock_accept_custom,
    .sock_accept           = sock_accept_custom,
    .io                    = io_custom,
    .wsi_logical_close     = wsi_logical_close_custom,
    .evlib_size_pt         = sizeof(struct pt_eventlibs_custom),
    // .evlib_size_wsi        = sizeof(struct wsi_eventlibs_custom),
};

static const lws_plugin_evlib_t evlib_custom = {
    .hdr =
        {
            "custom event loop",
            "lws_evlib_plugin",
            LWS_BUILD_HASH,
            LWS_PLUGIN_API_MAGIC,
        },
    .ops = &event_loop_ops_custom,
};

struct websocket *
websocket_init(struct lua_State *L, struct wl_event_loop *event_loop)
{
    struct context_user_data *ctx = malloc(sizeof(*ctx));

    *ctx = (struct context_user_data){
        .L           = L,
        .connect_ref = LUA_NOREF,
        .recv_ref    = LUA_NOREF,
        .close_ref   = LUA_NOREF,
    };

    struct pt_eventlibs_custom loop_var = {
        .event_loop = event_loop,
        .ctx        = ctx,
    };

    struct lws_protocols protocols[] = {
        {
            .name     = "http",
            .callback = lws_callback_http_dummy,
        },
        {
            .name                  = "main",
            .callback              = cb_main,
            .per_session_data_size = sizeof(struct per_session_storage),
            .rx_buffer_size        = RX_BUFFER_BYTES,
        },
        LWS_PROTOCOL_LIST_TERM,
    };
    struct lws_context_creation_info info = {
        .gid              = -1,
        .uid              = -1,
        .user             = ctx,
        .port             = 8000,
        .protocols        = protocols,
        .event_lib_custom = &evlib_custom,
        .foreign_loops    = (void *[]){&loop_var},
    };

    // global_efd = epoll_create1(0);

    lws_set_log_level(LLL_WARN | LLL_ERR, NULL);
    struct lws_context *context = lws_create_context(&info);

    ctx->context = context;

    return (struct websocket *)ctx;

    // while (true) {
    //     struct epoll_event events[16];
    //     int n = epoll_wait(global_efd, events, 16, -1);
    //     if (n == -1 || n == 0)
    //         abort();
    //     for (int i = 0; i < n; ++i) {
    //         struct lws_pollfd p = {
    //             .fd      = events[i].data.fd,
    //             .events  = events[i].events,
    //             .revents = events[i].events,
    //         };
    //         lws_service_fd(context, &p);

    //         while (lws_service_adjust_timeout(context, 1, 0) == 0) {
    //             lws_service_tsi(context, -1, 0);
    //         }
    //     }
    // }

    // return (struct websocket *)context;
}

void
websocket_fini(struct websocket *self)
{
    struct context_user_data *ctx = (struct context_user_data *)self;
    lws_context_destroy(ctx->context);

    luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->connect_ref);
    luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->recv_ref);
    luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->close_ref);

    free(ctx);
}
