---@meta

---`true` when invoked from kiwmic, `false` otherwise.
FROM_KIWMIC = false

---Represents the compositor. This is the entry point to the API.
---@class kiwmi_server
kiwmi = {}

--- @return kiwmi_output output The active output.
function kiwmi:active_output()
end

---Sets the background color (shown behind all views).
---@param color string The color in the format #rrggbb.
function kiwmi:bg_color(color)
end

---@return kiwmi_cursor cursor The cursor object.
function kiwmi:cursor()
end

---@return kiwmi_view view The currently focused view.
function kiwmi:focused_view()
end

---@return kiwmi_output output Returns the output at a specified position
function kiwmi:output_at(lx, ly)
end

---A new keyboard got attached.
---@param event "keyboard"
---@param callback fun(keyboard: kiwmi_keyboard)
function kiwmi:on(event, callback)
end

---Called when the active output needs to be requested (for example because a layer-shell surface needs to be positioned).
---Callback receives nothing and optionally returns a kiwmi_output.
---
---If this isn't set or returns `nil`, the compositor defaults to the output the focused view is on, and if there is no view, the output the mouse is on.
---
---@param event "request_active_output"
---@param callback fun(): kiwmi_output | nil
function kiwmi:on(event, callback)
end

---A new output got attached.
---@param event "output"
---@param callback fun(output: kiwmi_output)
function kiwmi:on(event, callback)
end

---A new view got created (actually mapped).
---@param event "view"
---@param callback fun(view: kiwmi_view)
function kiwmi:on(event, callback)
end

---Quit kiwmi.
function kiwmi:quit()
end

---Call `callback` after `delay` ms.
---Callback get passed itself, so that it can easily reregister itself.
function kiwmi:schedule(delay, callback)
end

--- Sets verbosity of kiwmi to the level specified with a number (see `kiwmi:verbosity()`).
function kiwmi:set_verbosity(level)
end

--- Spawn a new process.
--- `command` is passed to `/bin/sh`.
function kiwmi:spawn(command)
end

--- Stops an interactive move or resize.
function kiwmi:stop_interactive()
end

--- Unfocus the currently focused view.
function kiwmi:unfocus()
end

--- Returns the numerical verbosity level of kiwmi (value of one of `wlr_log_importance`, silent = 0, error = 1, info = 2, debug = 3).
function kiwmi:verbosity()
end

--- Get the view at a specified position.
function kiwmi:view_at(lx, ly)
end

---@class kiwmi_cursor
local cursor = {}

--- Returns the output at the cursor position or `nil` if there is none.
function cursor:output_at_pos()
end

--- Used to register event listeners.
---
--- ### Events
---
--- #### button_down
---
--- A mouse button got pressed.
--- Callback receives the ID of the button (i.e. LMB is 1, RMB is 2, ...).
---
--- The callback is supposed to return `true` if the event was handled.
--- The compositor will not forward it to the view under the cursor.
---
--- #### button_up
---
--- A mouse button got released.
--- Callback receives the ID of the button (i.e. LMB is 1, RMB is 2, ...).
---
--- The callback is supposed to return `true` if the event was handled.
--- The compositor will not forward it to the view under the cursor.
---
--- #### motion
---
--- The cursor got moved.
--- Callback receives a table containing `oldx`, `oldy`, `newx`, and `newy`.
---
--- #### scroll
---
--- Something was scrolled.
--- The callback receives a table containing `device` with the device name, `vertical` indicating whether it was a vertical or horizontal scroll, and `length` with the length of the vector (negative for left of up scrolls).
---
--- The callback is supposed to return `true` if the event was handled.
--- The compositor will not forward it to the view under the cursor.
function cursor:on(event, callback)
end

--- Get the current position of the cursor.
--- Returns two parameters: `x` and `y`.
function cursor:pos()
end

---@return kiwmi_view view The view at the cursor position, or `nil` if there is none.
function cursor:view_at_pos()
end

---@class kiwmi_keyboard
local keyboard = {}

--- The function takes a table as parameter.
--- The possible table indexes are "rules, model, layout, variant, options".
--- All the table parameters are optional and set to the system default if not set.
--- For the values to set have a look at the xkbcommon library.
--- <https://xkbcommon.org/doc/current/structxkb__rule__names.html>
function keyboard:keymap(keymap)
end

--- Returns a table with the state of all modifiers.
--- These are: `shift`, `caps`, `ctrl`, `alt`, `mod2`, `mod3`, `super`, and `mod5`.
function keyboard:modifiers()
end

--- Used to register event listeners.
--- ### Events
---
--- #### destroy
---
--- The keyboard is getting destroyed.
--- Callback receives the keyboard.
---
--- #### key_down
---
--- A key got pressed.
---
--- Callback receives a table containing the `key`, `keycode`, `raw`, and the `keyboard`.
---
--- This event gets triggered twice, once with mods applied (i.e. `Shift+3` is `#`) and `raw` set to `false`, and then again with no mods applied and `raw` set to `true`.
---
--- The callback is supposed to return `true` if the event was handled.
--- The compositor will not forward it to the focused view in that case.
---
--- #### key_up
---
--- A key got released.
--- Callback receives a table containing the `key`, `keycode`, `raw`, and the `keyboard`.
---
--- This event gets triggered twice, once with mods applied (i.e. `Shift+3` is `#`) and `raw` set to `false`, and then again with no mods applied and `raw` set to `true`.
---
--- The callback is supposed to return `true` if the event was handled.
--- The compositor will not forward it to the focused view in that case.
function keyboard:on(event, callback)
end

---Represents an output (most often a display).
---@class kiwmi_output
local output = {}

--- Tells the compositor to start automatically positioning the output (this is on per default).
function output:auto()
end

--- Moves the output to a specified position.
--- This is referring to the top-left corner.
function output:move(lx, ly)
end

--- The name of the output.
function output:name()
end

--- Used to register event listeners.
---
--- ### Events
---
--- #### destroy
---
--- The output is getting destroyed.
--- Callback receives the output.
---
--- #### resize
---
--- The output is being resized.
--- Callback receives a table containing the `output`, the new `width`, and the new `height`.
---
--- #### usable_area_change
---
--- The usable area of this output has changed, e.g. because the output was resized or the bars around it changed.
--- Callback receives a table containing the `output` and the new `x`, `y`, `width` and `height`.
function output:on(event, callbacks)
end

--- Get the position of the output.
--- Returns two parameters: `x` and `y`.
function output:pos()
end

--- Get the size of the output.
--- Returns two parameters: `width` and `height`.
function output:size()
end

--- Returns a table containing the `x`, `y`, `width` and `height` of the output's usable area, relative to the output's top left corner.
function output:usable_area()
end

---@class kiwmi_view
--- Represents a view (a window in kiwmi terms).
local view = {}

--- Returns the app id of the view.
--- This is comparable to the window class of X windows.
function view:app_id()
end

--- Closes the view.
function view:close()
end

--- Set whether the client is supposed to draw their own client decoration.
function view:csd(client_draws)
end

--- Focuses the view.
function view:focus()
end

--- Returns `true` if the view is hidden, `false` otherwise.
function view:hidden()
end

--- Hides the view.
function view:hide()
end

---@return integer id The ID unique to the view.
function view:id()
end

--- Starts an interactive move.
function view:imove()
end

--- Starts an interactive resize.
--- Takes a table containing the name of the edges, that the resize is happening on.
--- So for example to resize pulling on the bottom right corner you would pass `{"b", "r"}`.
function view:iresize(edges)
end

--- Moves the view to the specified position.
function view:move(lx, ly)
end

--- Used to register event listeners.
---
--- ### Events
---
--- #### destroy
---
--- The view is being destroyed.
--- Callback receives the view.
---
--- #### post_render
---
--- This is a no-op event. Temporarily preserved only to make config migration easier.
---
--- #### pre_render
---
--- This is a no-op event. Temporarily preserved only to make config migration easier.
---
--- #### request_move
---
--- The view wants to start an interactive move.
--- Callback receives the view.
---
--- #### request_resize
---
--- The view wants to start an interactive resize.
--- Callback receives a table containing the `view`, and `edges`, containing the edges.
function view:on(event, callback)
end

--- Returns the process ID of the client associated with the view.
function view:pid()
end

--- Returns the position of the view (top-left corner).
function view:pos()
end

--- Resizes the view.
function view:resize(width, height)
end

--- Unhides the view.
function view:show()
end

--- Returns the size of the view.
---
--- **NOTE**: Used directly after `view:resize()`, this still returns the old size.
function view:size()
end

--- Takes a table containing all edges that are tiled, or a bool to indicate all 4 edges.
function view:tiled(edges)
end

--- Returns the title of the view.
function view:title()
end

---Display a debug text for this view.
---@param text string The text to be displayed.
function view:set_debug_text(text)
end

---@return scene_tree tree
function view:scene_tree()
end

---@class scene_node
local node = {}

function node:destroy()
end

---Only valid if created through tree:add_rect
---@param width integer
---@param height integer
function node:set_size(width, height)
end

---Only valid if created through tree:add_rect
---@param color string
function node:set_color(color)
end

---@class scene_tree
local tree = {}

---@param color string The color in the format #rrggbb.
---@return scene_node node
function tree:add_rect(width, height, color)
end

---@return scene_node node
function tree:add_text(text, color)
end
