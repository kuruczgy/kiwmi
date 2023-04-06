local input_event_codes = {
    KEY_1 = 2,
    KEY_9 = 10
}

local keybinds = {
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
        c.manager:rotate_workspace(c.manager:workspace_of_view(c.manager.focused_view_id), 2)
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

    self.show_panel = false
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

function Controller:_set_panel(visible)
    local view_id = self.manager:views_with_tag('panel')[1]
    if not view_id then return end

    local view = self.manager.view_by_id[view_id]
    local view_info = self.manager.view_info_by_id[view_id]

    if visible then
        local output = self.manager.output_by_name[self.manager.output_names_ordered[1]]
        local usable_area = output:usable_area()
        local output_pos = Vec(output:pos()) + Vec(usable_area)
        local output_size = Vec(usable_area.width, usable_area.height)

        local factor = Vec(0.8, 0.8)

        local pos = output_pos + output_size * (factor * -1 + 1) * 0.5
        local size = output_size * factor

        view:move(pos.x, pos.y)
        view:resize(size.x, size.y)

        local border = view_info.border
        border.left:set_size(0, 0)
        border.right:set_size(0, 0)
        border.top:set_size(0, 0)
        border.bottom:set_size(0, 0)

        view:show()
        view:focus()
    else
        view:hide()
    end
end

function Controller:key_down(ev, mods)
    local code = ev.keycode - 8

    if ev.key == 'Super_L' or ev.key == 'Alt_R' then
        self.show_panel = true
        self:_set_panel(true)
        return true
    end

    if mods[self.mod_key] or mods.alt then
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

function Controller:key_up(ev, mods)
    if ev.key == 'Super_L' or ev.key == 'Alt_R' then
        self:_set_panel(false)
        self.show_panel = false
        return true
    end
end
