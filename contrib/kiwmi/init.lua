require("util")
require("config")

local watch = os.getenv("KIWMI_EMBED")
local mod_key = watch and "alt" or "super"

local manager
local controller
local websocket_server

local function reload()
    print("reload")
    local function require(modname)
        local path = package.searchpath(modname, package.path)
        dofile(path)
    end
    local function assign(target, source)
        for k, v in pairs(source) do
            target[k] = v
        end
    end

    require("manager")
    require("controller")
    require("websocket_server")

    local new = {}
    new.manager = Manager()
    new.controller = Controller(new.manager, mod_key)
    new.websocket_server = WebsocketServer(new.manager)

    -- try to preserve the state (this can obviously be buggy, but some special cases can be added here)
    assign(new.manager, manager or {})
    assign(new.controller, controller or {})
    assign(new.websocket_server, websocket_server or {})
    new.controller.manager = new.manager
    new.websocket_server.manager = new.manager

    manager = new.manager
    controller = new.controller
    websocket_server = new.websocket_server

    manager:arrange_views()
end

local cursor = kiwmi:cursor()
local function focus_at_cursor()
    local view = cursor:view_at_pos()

    if view then
        manager:focus(view:id())
    end
end

cursor:on("motion", function()
    focus_at_cursor()
end)

cursor:on("button_up", function()
    kiwmi:stop_interactive()
end)

kiwmi:on("output", function(output)
    manager:add_output(output)

    output:on("destroy", function(output)
        manager:remove_output(output)
        manager:arrange_views()
    end)

    output:on("usable_area_change", function()
        manager:arrange_views()
    end)
end)

kiwmi:on("view", function(view)
    manager:add_view(view)

    view:csd(false)
    view:tiled(true)

    view:on("destroy", function(view)
        manager:remove_view(view)
        manager:arrange_views()
    end)

    view:on("request_move", function()
        view:imove()
    end)

    view:on("request_resize", function(ev)
        view:iresize(ev.edges)
    end)

    manager:arrange_views()
end)

local global_keyboard = nil

kiwmi:on("keyboard", function(keyboard)
    global_keyboard = keyboard
    keyboard:on("key_down", function(ev)
        if not ev.raw then return end
        local mods = ev.keyboard:modifiers()

        if mods[mod_key] and ev.key == 'o' then
            reload()
            return true
        end

        return controller:key_down(ev, mods)
    end)
end)

cursor:on("button_down", function(id)
    if global_keyboard and global_keyboard:modifiers()[mod_key] then
        local view = cursor:view_at_pos()

        if view then
            if id == 1 then view:imove() end
            if id == 2 then
                view:iresize({ "b", "r" })
            end
        end
    end
end)

reload()

if not watch then
    -- kiwmi:spawn("swaybg -c '#333333'")
    kiwmi:spawn("waybar")
    kiwmi:spawn("code --ozone-platform-hint=auto")
    kiwmi:spawn("alacritty -e /bin/sh -c 'tail -f /tmp/log.txt'")
else
    for i = 1, 3 do
        kiwmi:spawn(string.format("alacritty -T '%d' -e /bin/sh -c 'echo number: %d; read'", i, i))
    end
end
