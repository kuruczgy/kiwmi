WebsocketServer = class()

function WebsocketServer:ctor(manager)
    self.clients = {}
    self.manager = manager

    local ws_handler = {}
    function ws_handler.connect(client)
        self.clients[client] = true
        print("connect", client)
    end

    function ws_handler.recv(client, msg)
        print("recv", client, msg)

        if msg.kind == "get_layout_state" then
            kiwmi:ws_send(client, { state = self.manager:export_layout_state() })
        end

        if msg.kind == "set_layout_state" then
            self.manager:import_layout_state(msg.state)
            kiwmi:ws_send(client, { ack = msg.state })
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
