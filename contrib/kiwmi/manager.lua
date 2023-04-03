local layouts = {}

function layouts.tall(n, pos, size)
    if n == 0 then return end
    coroutine.yield(
        pos + size * Vec(0, 0),
        size * Vec(0.5, 1)
    )
    layouts.stacked(n - 1, pos + size * Vec(0.5, 0), size * Vec(0.5, 1))
end

function layouts.stacked(n, pos, size)
    for i = 1, n do
        coroutine.yield(
            pos + size * Vec(0, (i - 1) / n),
            size * Vec(1, 1 / n)
        )
    end
end

function layouts.spiral(n, pos, size, ratio, i)
    ratio = ratio or 0.618
    i = i or 0

    if n == 0 then return end
    if n == 1 then
        coroutine.yield(pos, size)
        return
    end

    local pos_main = i % 2 == 0 and Vec(ratio, 0) or Vec(0, ratio)
    local pos_rest = i % 2 == 0 and Vec(1 - ratio, 0) or Vec(0, 1 - ratio)
    local size_main = i % 2 == 0 and Vec(ratio, 1) or Vec(1, ratio)
    local size_rest = i % 2 == 0 and Vec(1 - ratio, 1) or Vec(1, 1 - ratio)

    local pos1, pos2
    if i % 4 < 2 then
        pos1, pos2 = pos, pos + size * pos_main
    else
        pos1, pos2 = pos + size * pos_rest, pos
    end

    coroutine.yield(pos1, size * size_main)
    layouts.spiral(n - 1, pos2, size * size_rest, ratio, i + 1)
end

local function inset(iter, amount)
    for pos, size in iter do
        coroutine.yield(pos + amount, size - 2 * amount)
    end
end

local layout = function(...)
    local args = { ... }
    inset(coroutine.wrap(function() layouts.spiral(table.unpack(args)) end), 5)
end

local function iterc(f, ...)
    local args = { ... }
    return coroutine.wrap(function() f(table.unpack(args)) end)
end

local config_output_priority = {}
for i, cfg in ipairs(config.outputs) do
    config_output_priority[cfg.name] = i
end

Manager = class()

function Manager:ctor()
    self.view_by_id = {}
    self.view_info_by_id = {}
    self.output_by_name = {}
    self.output_info_by_name = {}
    self.output_names_ordered = {}

    -- any string is a valid workspace
    self.ws_by_id = {}

    self.focused_view_id = nil
end

function Manager:add_view(view)
    local view_id = view:id()
    self.view_by_id[view_id] = view

    local view_info = {
        -- nil means not displayed
        ws_id = nil
    }
    self.view_info_by_id[view_id] = view_info

    -- place into a workspace by default
    local ws_id = self:get_populated_workspace_ids()[0] or "1"
    self:move_view_to_workspace(view_id, ws_id)

    -- set up scene nodes
    local tree = view:scene_tree()
    local color = config.theme.border_color
    view_info.border = {
        left = tree:add_rect(0, 0, color),
        right = tree:add_rect(0, 0, color),
        top = tree:add_rect(0, 0, color),
        bottom = tree:add_rect(0, 0, color),
    }
end

function Manager:focus(view_id)
    local view_info = self.view_info_by_id[view_id]

    if self.focused_view_id then
        local border = self.view_info_by_id[self.focused_view_id].border
        local color = config.theme.border_color
        border.left:set_color(color)
        border.right:set_color(color)
        border.top:set_color(color)
        border.bottom:set_color(color)
    end

    local border = view_info.border
    local color = config.theme.focused_color
    border.left:set_color(color)
    border.right:set_color(color)
    border.top:set_color(color)
    border.bottom:set_color(color)

    self.focused_view_id = view_id

    self.view_by_id[view_id]:focus()
end

function Manager:remove_view(view)
    local view_id = view:id()
    self:remove_view_from_workspace(view_id)
    self.view_by_id[view_id] = nil
    self.view_info_by_id[view_id] = nil

    if self.focused_view_id == view_id then
        self.focused_view_id = nil
    end
end

function Manager:remove_view_from_workspace(view_id)
    local view_info = self.view_info_by_id[view_id]
    assert(view_info.ws_id)

    local ws = self.ws_by_id[view_info.ws_id]
    local idx = table.find(ws.view_ids, view_id)
    if idx then table.remove(ws.view_ids, idx) end
end

function Manager:move_view_to_workspace(view_id, ws_id)
    local view_info = self.view_info_by_id[view_id]
    if view_info.ws_id then self:remove_view_from_workspace(view_id) end
    view_info.ws_id = ws_id

    local ws = self.ws_by_id[ws_id]
    if not ws then
        ws = {
            -- order matters
            view_ids = {}
        }
        self.ws_by_id[ws_id] = ws
    end

    ws.view_ids[#ws.view_ids + 1] = view_id
end

function Manager:export_layout_state()
    local ls = {
        workspaces = {}
    }
    for ws_id, ws in pairs(self.ws_by_id) do
        local workspace = { views = {} }
        for i, view_id in ipairs(ws.view_ids) do
            workspace.views[i] = view_id
        end
        ls.workspaces[ws_id] = workspace
    end
    return ls
end

function Manager:import_layout_state(ls)
    for _, view_info in pairs(self.view_info_by_id) do
        view_info.ws_id = nil
    end

    self.ws_by_id = {}
    for ws_id, ws in pairs(ls.workspaces) do
        local view_ids = {}
        for _, view_id in ipairs(ws.views) do
            if self.view_info_by_id[view_id] then
                self.view_info_by_id[view_id].ws_id = ws_id
                view_ids[#view_ids + 1] = view_id
            end
        end
        self.ws_by_id[ws_id] = { view_ids = view_ids }
    end

    self:arrange_views()
end

function Manager:workspace_of_view(view_id)
    return self.view_info_by_id[view_id].ws_id
end

function Manager:add_output(output)
    local name = output:name()
    self.output_by_name[name] = output
    self.output_info_by_name[name] = {
        max_views = 3,
    }

    -- output position config
    output:auto()
    for _, cfg in ipairs(config.outputs) do
        if cfg.name == name then
            if cfg.position then
                output:move(cfg.position.x or 0, cfg.position.y or 0)
            end
            if cfg.transform then
                output:set_transform(cfg.transform.rotation or 0, cfg.transform.flipped)
            end
            break
        end
    end

    self.output_names_ordered[#self.output_names_ordered + 1] = name
    table.sort(self.output_names_ordered, function(a, b)
        local ia, ib = config_output_priority[a], config_output_priority[b]
        if not ia then return false end
        if not ib then return true end
        return ia < ib
    end)
end

function Manager:remove_output(output)
    local name = output:name()
    self.output_by_name[name] = nil
    self.output_info_by_name[name] = nil

    local idx = table.find(self.output_names_ordered, name)
    table.remove(self.output_names_ordered, idx)
end

function Manager:get_populated_workspace_ids()
    local keys = {}
    for k, _ in pairs(self.ws_by_id) do
        keys[#keys + 1] = k
    end
    table.sort(keys)
    return keys
end

function Manager:rotate_workspace(ws_id, top_k)
    local view_ids = self.ws_by_id[ws_id].view_ids
    table.insert(view_ids, 1, table.remove(view_ids, top_k))
end

function Manager:adjust_output_max_views(output, n)
    local output_info = self.output_info_by_name[output:name()]
    output_info.max_views = output_info.max_views + n
    if output_info.max_views < 1 then output_info.max_views = 1 end
    if output_info.max_views > 8 then output_info.max_views = 8 end
end

function Manager:arrange_views()
    local output_view_ids = {}
    for i, name in ipairs(self.output_names_ordered) do
        local output_info = self.output_info_by_name[name]
        output_view_ids[i] = { max = output_info.max_views }
    end
    local output_i = 1
    local function insert_view(view_id)
        local l = output_view_ids[output_i]
        while #l >= l.max and output_i < #self.output_names_ordered do
            output_i = output_i + 1
            l = output_view_ids[output_i]
        end
        l[#l + 1] = view_id

        local view_info = self.view_info_by_id[view_id]
        local view = self.view_by_id[view_id]
        view:set_debug_text(string.format("workspace %s\noutput %d (max: %d)", view_info.ws_id or "(none)", output_i,
            l.max))
    end

    for _, ws_id in ipairs(self:get_populated_workspace_ids()) do
        local ws = self.ws_by_id[ws_id]
        for _, view_id in ipairs(ws.view_ids) do
            insert_view(view_id)
        end

        -- Skip to next output if only one slot remains
        -- local l = output_view_ids[output_i]
        -- if l.max - #l <= 1 and output_i < #manager.output_names_ordered then
        --     output_i = output_i + 1
        -- end
    end

    for i, name in ipairs(self.output_names_ordered) do
        local output = self.output_by_name[name]
        local view_ids = output_view_ids[i]

        local usable_area = output:usable_area()
        local output_pos = Vec(output:pos()) + Vec(usable_area)
        local output_size = Vec(usable_area.width, usable_area.height)

        local view_i = 1
        for pos, size in iterc(layout, #view_ids, output_pos, output_size) do
            local view_id = view_ids[view_i]
            view_i = view_i + 1

            local view_info = self.view_info_by_id[view_id]
            local view = self.view_by_id[view_id]
            view:move(pos.x, pos.y)
            view:resize(size.x, size.y)

            -- adjust border
            local border = view_info.border
            local th = 2

            border.left:set_size(th, size.y)
            border.left:set_position(-th, 0)

            border.right:set_size(th, size.y)
            border.right:set_position(size.x, 0)

            border.top:set_size(size.x + 2 * th, th)
            border.top:set_position(-th, -th)

            border.bottom:set_size(size.x + 2 * th, th)
            border.bottom:set_position(-th, size.y)
        end
    end
end
