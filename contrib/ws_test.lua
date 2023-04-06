local ws_handler = {}

function printf(...) print(string.format(...)) end

-- kiwmi:to_json({ k1 = "v1", k2 = true, nest1 = { nest2 = "v", nest22 = "v2" } })

local function dump_table(t, ind)
    ind = ind or ""
    for k, v in pairs(t) do
        if type(v) == "table" then
            printf("%s%s:", ind, k)
            dump_table(v, ind .. " ")
        else
            printf("%s%s: %s", ind, k, tostring(v))
        end
    end
end

function ws_handler.connect(client)
    print("connect", client)
    kiwmi:ws_send(client, { msg = "welcome" })
end

function ws_handler.recv(client, msg)
    print("recv", client, msg)
    dump_table(msg)
    kiwmi:ws_send(client, { pong = msg })
    kiwmi:ws_send(client, { "t1", "t2", "t3" })
end

function ws_handler.close(client)
    print("close", client)
    kiwmi:ws_send(client, "bye!")
end

kiwmi:ws_register(ws_handler)
