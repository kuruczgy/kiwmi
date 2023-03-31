-- compat
if unpack then
    table.unpack = unpack
    unpack = nil
end

-- stdlib additions

---Find a value in an array
---@param t table The array.
---@param value any The value to search for. (Compared with the equality operator.)
---@return integer? i The index of the first occurrence of value, or nil if it was not found.
function table.find(t, value)
    for i, v in ipairs(t) do
        if value == v then
            return i
        end
    end
end

---Remove the element at `from` and insert it at index `to`.
---@param t table The array.
---@param from integer The index to move from.
---@param to integer The index to move to.
function table.mov(t, from, to)
    table.insert(t, to, table.remove(t, from))
end

function printf(...)
    print(string.format(...))
end

function class(objmt)
    local class = {}

    objmt = objmt or {}
    objmt.__index = class

    local classmt = {}

    function classmt.__call(class, ...)
        local obj = {}
        setmetatable(obj, objmt)
        obj:ctor(...)
        return obj
    end

    setmetatable(class, classmt)
    return class
end

Vec = class({
    __add = function(a, b)
        if type(b) == 'table' then
            return Vec(a.x + b.x, a.y + b.y)
        end
        return Vec(a.x + b, a.y + b)
    end,
    __sub = function(a, b)
        if type(b) == 'table' then
            return Vec(a.x - b.x, a.y - b.y)
        end
        return Vec(a.x - b, a.y - b)
    end,
    __mul = function(a, b)
        if type(b) == 'table' then
            return Vec(a.x * b.x, a.y * b.y)
        end
        return Vec(a.x * b, a.y * b)
    end,
    __div = function(a, b)
        if type(b) == 'table' then
            return Vec(a.x / b.x, a.y / b.y)
        end
        return Vec(a.x / b, a.y / b)
    end,
    __tostring = function(v)
        return string.format("Vec(%f, %f)", v.x, v.y)
    end,
})

function Vec:ctor(x, y)
    if type(x) == 'table' then
        x, y = x.x, x.y
    end

    self.x = x
    self.y = y
end
