local embed = os.getenv("KIWMI_EMBED")
local mod_key = embed and "alt" or "super"

local manager
local controller
local websocket_server

-- idempotent parts go in here
local function reload()
    print("reload")
    local function require(modname)
        local path = package.searchpath(modname, package.path)
        dofile(path)
    end
    local function assign(target, source, blacklist)
        for k, v in pairs(source) do
            if not blacklist[k] then
                target[k] = v
            end
        end
    end

    require("util")
    require("config")
    require("manager")
    require("controller")
    require("websocket_server")

    local new = {}
    new.manager = Manager()
    new.controller = Controller(new.manager, mod_key)
    new.websocket_server = WebsocketServer(new.manager)

    -- try to preserve the state (this can obviously be buggy, but some special
    -- cases can be added here)
    assign(new.manager, manager or {}, { state_changed = true })
    assign(new.controller, controller or {}, { manager = true })
    assign(new.websocket_server, websocket_server or {}, { manager = true })

    manager = new.manager
    controller = new.controller
    websocket_server = new.websocket_server

    manager.state_changed:subscribe(function()
        manager:arrange_views()
    end)

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
    local tags = {}

    -- TODO: some better abstraction for auto-tagging
    if view:title() == 'Kiwmi Panel' then tags.panel = 1 end

    manager:add_view(view, tags)

    view:csd(false)
    view:tiled(true)
    if tags.panel then
        view:hide()
    end

    view:on("destroy", function(view)
        manager:remove_view(view)
    end)

    view:on("request_move", function()
        view:imove()
    end)

    view:on("request_resize", function(ev)
        view:iresize(ev.edges)
    end)
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
    keyboard:on("key_up", function(ev)
        if not ev.raw then return end

        local mods = ev.keyboard:modifiers()
        return controller:key_up(ev, mods)
    end)
end)

cursor:on("button_down", function(id)
    if global_keyboard and global_keyboard:modifiers()[mod_key] then
        local view = cursor:view_at_pos()
        local view_info = manager.view_info_by_id[view:id()]
        if view_info.tags.panel then
            return
        end

        if view then
            if id == 1 then view:imove() end
            if id == 2 then
                view:iresize({ "b", "r" })
            end
        end
    end
end)

reload()

kiwmi:spawn("gtk-launch kiwmi-panel")
if not embed then
    -- kiwmi:spawn("swaybg -c '#333333'")
    kiwmi:spawn("waybar")
    -- kiwmi:spawn("code --ozone-platform-hint=auto")
    -- kiwmi:spawn("alacritty -e /bin/sh -c 'tail -f /tmp/log.txt'")
else
    -- for i = 1, 3 do
    --     kiwmi:spawn(string.format("alacritty -T '%d' -e /bin/sh -c 'echo number: %d; read'", i, i))
    -- end
    -- kiwmi:spawn("chromium --ozone-platform-hint=auto --user-data-dir='/tmp/kiwmi_kiosk' 'http://localhost:3000'")
end
