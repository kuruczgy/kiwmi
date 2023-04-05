WebsocketServer = class()

function WebsocketServer:ctor(manager)
    self.clients = {}
    self.manager = manager

    manager.state_changed:subscribe(function()
        local msg = {
            kind = "layout_state",
            data = self.manager:export_layout_state()
        }

        for client, _ in pairs(self.clients) do
            kiwmi:ws_send(client, msg)
        end
    end)

    local ws_handler = {}
    function ws_handler.connect(client)
        self.clients[client] = true
        print("connect", client)

        kiwmi:ws_send(client, {
            kind = "layout_state",
            data = self.manager:export_layout_state()
        })
    end

    function ws_handler.recv(client, msg)
        print("recv", client, msg)

        if msg.kind == "set_layout_state" then
            self.manager:import_layout_state(msg.state)
        end

        if msg == "rotate" then
            self.manager:rotate_workspace(self.manager:workspace_of_view(kiwmi:focused_view():id()), 2)
            self.manager:arrange_views()
        end
    end

    function ws_handler.close(client)
        self.clients[client] = nil
        print("close", client)
    end

    kiwmi:ws_register(ws_handler)
end
