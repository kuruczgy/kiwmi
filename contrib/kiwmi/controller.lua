local input_event_codes = {
    KEY_1 = 2,
    KEY_9 = 10
}

local keybinds = {
    Escape = function(c) kiwmi:quit() end,
    Return = function(c) kiwmi:spawn("alacritty") end,
    q = function(c)
        local view = kiwmi:focused_view()
        if view then view:close() end
    end,
    r = function(c)
        c.manager:arrange_views()
    end,
    p = function(c) kiwmi:quit() end,
    x = function(c)
        c.manager:adjust_output_max_views(kiwmi:active_output(), -1)
        c.manager:arrange_views()
    end,
    c = function(c)
        c.manager:adjust_output_max_views(kiwmi:active_output(), 1)
        c.manager:arrange_views()
    end,
    f = function(c)
        c.manager:rotate_workspace(c.manager:workspace_of_view(kiwmi:focused_view():id()), 2)
        c.manager:arrange_views()
    end,
    d = function(c)
        kiwmi:spawn("wofi --show drun")
    end,
    l = function(c)
        kiwmi:spawn("swaylock")
    end,
}

Controller = class()

function Controller:ctor(manager, mod_key)
    self.manager = manager
    self.mod_key = mod_key
end

function Controller:handle_mod_shift_num(num)
    local view = kiwmi:focused_view()
    if view then
        local view_id = view:id()
        self.manager:move_view_to_workspace(view_id, tostring(num))
        self.manager:arrange_views()
        return true
    end
end

function Controller:key_down(ev, mods)
    local code = ev.keycode - 8

    if mods[self.mod_key] then
        if mods.shift then
            if input_event_codes.KEY_1 <= code and code <= input_event_codes.KEY_9 then
                local num = code - input_event_codes.KEY_1 + 1
                return self:handle_mod_shift_num(num)
            end
        end

        local bind = keybinds[ev.key]
        if bind then
            bind(self)
            return true
        end
    end

    return false
end
