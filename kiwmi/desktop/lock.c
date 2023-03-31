// Adapted from sway/lock.c

#include "desktop/lock.h"
#include "desktop/desktop.h"
#include "desktop/output.h"
#include "desktop/stratum.h"
#include "input/seat.h"
#include "server.h"
#include <assert.h>
#include <stdlib.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

struct session_lock_output {
    struct wlr_scene_tree *tree;
    struct wlr_scene_rect *background;
    struct session_lock *lock;

    struct kiwmi_output *output;

    struct wl_list link; // sway_session_lock::outputs

    struct wl_listener destroy;
    struct wl_listener commit;

    struct wlr_session_lock_surface_v1 *surface;

    // invalid if surface is NULL
    struct wl_listener surface_destroy;
    struct wl_listener surface_map;
};

static void
focus_surface(struct session_lock *lock, struct wlr_surface *focused)
{
    lock->focused = focused;
    seat_focus_surface(lock->server->input.seat, focused);
}

static void
refocus_output(struct session_lock_output *output)
{
    // Move the seat focus to another surface if one is available
    if (output->lock->focused == output->surface->surface) {
        struct wlr_surface *next_focus = NULL;

        struct session_lock_output *candidate;
        wl_list_for_each (candidate, &output->lock->outputs, link) {
            if (candidate == output || !candidate->surface) {
                continue;
            }

            if (candidate->surface->mapped) {
                next_focus = candidate->surface->surface;
                break;
            }
        }

        focus_surface(output->lock, next_focus);
    }
}

static void
handle_surface_map(struct wl_listener *listener, void *UNUSED(data))
{
    struct session_lock_output *surf =
        wl_container_of(listener, surf, surface_map);
    if (surf->lock->focused == NULL) {
        focus_surface(surf->lock, surf->surface->surface);
    }
}

static void
handle_surface_destroy(struct wl_listener *listener, void *UNUSED(data))
{
    struct session_lock_output *output =
        wl_container_of(listener, output, surface_destroy);
    refocus_output(output);

    assert(output->surface);
    output->surface = NULL;
    wl_list_remove(&output->surface_destroy.link);
    wl_list_remove(&output->surface_map.link);
}

static void
lock_output_reconfigure(struct session_lock_output *output)
{
    int width  = output->output->wlr_output->width;
    int height = output->output->wlr_output->height;

    wlr_scene_rect_set_size(output->background, width, height);

    if (output->surface) {
        wlr_session_lock_surface_v1_configure(output->surface, width, height);
    }
}

static void
handle_new_surface(struct wl_listener *listener, void *data)
{
    struct session_lock *lock = wl_container_of(listener, lock, new_surface);
    struct wlr_session_lock_surface_v1 *lock_surface = data;
    struct kiwmi_output *output = lock_surface->output->data;

    wlr_log(WLR_DEBUG, "new lock layer surface");

    struct session_lock_output *current_lock_output, *lock_output = NULL;
    wl_list_for_each (current_lock_output, &lock->outputs, link) {
        if (current_lock_output->output == output) {
            lock_output = current_lock_output;
            break;
        }
    }
    assert(lock_output);
    assert(!lock_output->surface);

    lock_output->surface = lock_surface;

    wlr_scene_subsurface_tree_create(lock_output->tree, lock_surface->surface);

    lock_output->surface_destroy.notify = handle_surface_destroy;
    wl_signal_add(&lock_surface->events.destroy, &lock_output->surface_destroy);
    lock_output->surface_map.notify = handle_surface_map;
    wl_signal_add(&lock_surface->events.map, &lock_output->surface_map);

    lock_output_reconfigure(lock_output);
}

static void
session_lock_output_destroy(struct session_lock_output *output)
{
    if (output->surface) {
        refocus_output(output);
        wl_list_remove(&output->surface_destroy.link);
        wl_list_remove(&output->surface_map.link);
    }

    wl_list_remove(&output->commit.link);
    wl_list_remove(&output->destroy.link);
    wl_list_remove(&output->link);

    free(output);
}

static void
lock_node_handle_destroy(struct wl_listener *listener, void *UNUSED(data))
{
    struct session_lock_output *output =
        wl_container_of(listener, output, destroy);
    session_lock_output_destroy(output);
}

static void
lock_output_handle_commit(struct wl_listener *listener, void *data)
{
    struct wlr_output_event_commit *event = data;
    struct session_lock_output *output =
        wl_container_of(listener, output, commit);
    if (event->committed
        & (WLR_OUTPUT_STATE_MODE | WLR_OUTPUT_STATE_SCALE
           | WLR_OUTPUT_STATE_TRANSFORM)) {
        lock_output_reconfigure(output);
    }
}

static struct session_lock_output *
session_lock_output_create(
    struct session_lock *lock,
    struct kiwmi_output *output)
{
    struct session_lock_output *lock_output =
        calloc(1, sizeof(struct session_lock_output));
    if (!lock_output) {
        wlr_log(WLR_ERROR, "failed to allocate a session lock output");
        return NULL;
    }

    // TODO: Probably should add a separate stratum for the lock surfaces.
    struct wlr_scene_tree *tree =
        wlr_scene_tree_create(output->strata[KIWMI_STRATUM_LS_OVERLAY]);
    if (!tree) {
        wlr_log(
            WLR_ERROR, "failed to allocate a session lock output scene tree");
        free(lock_output);
        return NULL;
    }

    float *color = (float[4]){0.f, 0.f, 0.f, 1.f};
    if (lock->abandoned) {
        color[0] = 1.f;
    }

    struct wlr_scene_rect *background =
        wlr_scene_rect_create(tree, 0, 0, color);
    if (!background) {
        wlr_log(
            WLR_ERROR,
            "failed to allocate a session lock output scene background");
        wlr_scene_node_destroy(&tree->node);
        free(lock_output);
        return NULL;
    }

    lock_output->output     = output;
    lock_output->tree       = tree;
    lock_output->background = background;
    lock_output->lock       = lock;

    lock_output->destroy.notify = lock_node_handle_destroy;
    wl_signal_add(&tree->node.events.destroy, &lock_output->destroy);

    lock_output->commit.notify = lock_output_handle_commit;
    wl_signal_add(&output->wlr_output->events.commit, &lock_output->commit);

    lock_output_reconfigure(lock_output);

    wl_list_insert(&lock->outputs, &lock_output->link);

    return lock_output;
}

static void
session_lock_destroy(struct session_lock *lock)
{
    struct session_lock_output *lock_output, *tmp_lock_output;
    wl_list_for_each_safe (lock_output, tmp_lock_output, &lock->outputs, link) {
        // destroying the node will also destroy the whole lock output
        wlr_scene_node_destroy(&lock_output->tree->node);
    }

    if (lock->server->session_lock.lock == lock) {
        lock->server->session_lock.lock = NULL;
    }

    if (!lock->abandoned) {
        wl_list_remove(&lock->destroy.link);
        wl_list_remove(&lock->unlock.link);
        wl_list_remove(&lock->new_surface.link);
    }

    free(lock);
}

static void
handle_unlock(struct wl_listener *listener, void *UNUSED(data))
{
    struct session_lock *lock = wl_container_of(listener, lock, unlock);
    wlr_log(WLR_DEBUG, "session unlocked");

    struct kiwmi_seat *seat = lock->server->input.seat;
    seat_set_exclusive_client(seat, NULL);

    session_lock_destroy(lock);
}

static void
handle_abandon(struct wl_listener *listener, void *UNUSED(data))
{
    struct session_lock *lock = wl_container_of(listener, lock, destroy);
    wlr_log(WLR_INFO, "session lock abandoned");

    // TODO: this would just unlock the session, abort instead for now
    exit(1);
    seat_set_exclusive_client(lock->server->input.seat, NULL);

    struct session_lock_output *lock_output;
    wl_list_for_each (lock_output, &lock->outputs, link) {
        wlr_scene_rect_set_color(
            lock_output->background, (float[4]){1.f, 0.f, 0.f, 1.f});
    }

    lock->abandoned = true;
    wl_list_remove(&lock->destroy.link);
    wl_list_remove(&lock->unlock.link);
    wl_list_remove(&lock->new_surface.link);
}

static void
handle_session_lock(struct wl_listener *listener, void *data)
{
    struct wlr_session_lock_v1 *lock = data;
    struct wl_client *client         = wl_resource_get_client(lock->resource);
    struct kiwmi_server *server =
        wl_container_of(listener, server, session_lock.new_lock);

    if (server->session_lock.lock) {
        if (server->session_lock.lock->abandoned) {
            wlr_log(WLR_INFO, "Replacing abandoned lock");
            session_lock_destroy(server->session_lock.lock);
        } else {
            wlr_log(WLR_ERROR, "Cannot lock an already locked session");
            wlr_session_lock_v1_destroy(lock);
            return;
        }
    }

    struct session_lock *session_lock = calloc(1, sizeof(struct session_lock));
    if (!session_lock) {
        wlr_log(WLR_ERROR, "failed to allocate a session lock object");
        wlr_session_lock_v1_destroy(lock);
        return;
    }

    session_lock->server = server;

    wl_list_init(&session_lock->outputs);

    wlr_log(WLR_DEBUG, "session locked");

    seat_set_exclusive_client(server->input.seat, client);

    struct kiwmi_output *output;
    wl_list_for_each (output, &server->desktop.outputs, link) {
        session_lock_add_output(session_lock, output);
    }

    session_lock->new_surface.notify = handle_new_surface;
    wl_signal_add(&lock->events.new_surface, &session_lock->new_surface);
    session_lock->unlock.notify = handle_unlock;
    wl_signal_add(&lock->events.unlock, &session_lock->unlock);
    session_lock->destroy.notify = handle_abandon;
    wl_signal_add(&lock->events.destroy, &session_lock->destroy);

    wlr_session_lock_v1_send_locked(lock);
    server->session_lock.lock = session_lock;
}

static void
handle_session_lock_destroy(struct wl_listener *listener, void *UNUSED(data))
{
    struct kiwmi_server *server =
        wl_container_of(listener, server, session_lock.manager_destroy);

    // if the server shuts down while a lock is active, destroy the lock
    if (server->session_lock.lock) {
        session_lock_destroy(server->session_lock.lock);
    }

    wl_list_remove(&server->session_lock.new_lock.link);
    wl_list_remove(&server->session_lock.manager_destroy.link);

    server->session_lock.manager = NULL;
}

void
session_lock_add_output(struct session_lock *lock, struct kiwmi_output *output)
{
    struct session_lock_output *lock_output =
        session_lock_output_create(lock, output);

    // if we run out of memory while trying to lock the screen, the best we
    // can do is kill the sway process. Security conscious users will have
    // the sway session fall back to a login shell.
    if (!lock_output) {
        wlr_log(WLR_ERROR, "aborting: failed to allocate a lock output");
        abort();
    }
}

bool
session_lock_has_surface(struct session_lock *lock, struct wlr_surface *surface)
{
    struct session_lock_output *lock_output;
    wl_list_for_each (lock_output, &lock->outputs, link) {
        if (lock_output->surface && lock_output->surface->surface == surface) {
            return true;
        }
    }

    return false;
}

void
session_lock_init(struct kiwmi_server *server)
{
    server->session_lock.manager =
        wlr_session_lock_manager_v1_create(server->wl_display);

    server->session_lock.new_lock.notify        = handle_session_lock;
    server->session_lock.manager_destroy.notify = handle_session_lock_destroy;
    wl_signal_add(
        &server->session_lock.manager->events.new_lock,
        &server->session_lock.new_lock);
    wl_signal_add(
        &server->session_lock.manager->events.destroy,
        &server->session_lock.manager_destroy);
}
