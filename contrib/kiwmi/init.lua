local watch = os.getenv("KIWMI_EMBED")
local mod_key = watch and "alt" or "super"

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


local special_view = nil
local function set_special_view_visible(visible)
    if not special_view then return end
    if visible then
        local output = manager.output_by_name[manager.output_names_ordered[1]]
        local usable_area = output:usable_area()
        local output_pos = Vec(output:pos()) + Vec(usable_area)
        local output_size = Vec(usable_area.width, usable_area.height)

        local factor = Vec(0.8, 0.8)

        local pos = output_pos + output_size * (factor * -1 + 1) * 0.5
        local size = output_size * factor

        special_view:move(pos.x, pos.y)
        special_view:resize(size.x, size.y)

        special_view:show()
        special_view:focus()
    else
        special_view:hide()
    end
end

kiwmi:on("view", function(view)
    local special = view:title() == 'Kiwmi Panel'

    if not special then
        manager:add_view(view)
        view:csd(false)
        view:tiled(true)
        view:on("destroy", function(view)
            manager:remove_view(view)
        end)
    else
        special_view = view
        special_view:hide()
    end

    view:on("request_move", function()
        view:imove()
    end)

    view:on("request_resize", function(ev)
        view:iresize(ev.edges)
    end)
end)

local global_keyboard = nil

local alt_tab_down = false

kiwmi:on("keyboard", function(keyboard)
    global_keyboard = keyboard
    keyboard:on("key_down", function(ev)
        if not ev.raw then return end
        local mods = ev.keyboard:modifiers()

        if mods[mod_key] and ev.key == 'o' then
            reload()
            return true
        end

        if mods['alt'] and ev.key == 'Tab' then
            alt_tab_down = true
            set_special_view_visible(true)
            return true
        end

        return controller:key_down(ev, mods)
    end)
    keyboard:on("key_up", function(ev)
        if not ev.raw then return end

        -- (ev.key == 'Alt_L' or ev.key == 'Alt_R')
        if alt_tab_down and ev.key == 'Tab' then
            set_special_view_visible(false)
            alt_tab_down = false
            return true
        end
    end)
end)

cursor:on("button_down", function(id)
    if alt_tab_down then return end
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
    -- kiwmi:spawn("code --ozone-platform-hint=auto")
    -- kiwmi:spawn("alacritty -e /bin/sh -c 'tail -f /tmp/log.txt'")
else
    -- for i = 1, 3 do
    --     kiwmi:spawn(string.format("alacritty -T '%d' -e /bin/sh -c 'echo number: %d; read'", i, i))
    -- end
    -- kiwmi:spawn("chromium --ozone-platform-hint=auto --user-data-dir='/tmp/kiwmi_kiosk' 'http://localhost:3000'")
    kiwmi:spawn("gtk-launch kiwmi-panel")
end
