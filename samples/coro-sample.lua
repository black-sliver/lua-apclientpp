-- coroutine example
-- see sample.lua for a more complete example of the API

local AP = require "lua-apclientpp"


-- global to this mod
local game_name = "Secret of Evermore"
local items_handling = 7  -- full remote
local message_format = AP.RenderFormat.TEXT
local ap = nil
local co = nil

-- TODO: user input
local host = "localhost"
local slot = "Player1"
local password = ""


function connect(server, slot, password)
    local running = true  -- set this to false to kill the coroutine

    function on_socket_connected()
        print("Socket connected")
    end

    function on_socket_error(msg)
        print("Socket error: " .. msg)
    end

    function on_socket_disconnected()
        print("Socket disconnected")
    end

    function on_room_info()
        print("Room info")
        -- ...
    end

    -- ...

    local uuid = ""
    ap = AP(uuid, game_name, server);

    ap:set_socket_connected_handler(on_socket_connected)
    ap:set_socket_error_handler(on_socket_error)
    ap:set_socket_disconnected_handler(on_socket_disconnected)
    -- ...

    while running do
        ap:poll()
        coroutine.yield()
    end
end

co = coroutine.create(function () connect(host, slot, password) end)

print("Will run for 10 seconds ...")
local t0 = os.clock()
while os.clock() - t0 < 10 do
    local status = coroutine.resume(co)
end
print("shutting down...");
