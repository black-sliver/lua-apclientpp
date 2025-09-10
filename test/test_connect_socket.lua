-- try to connect and disconnect (via GC) socket

AP = require "lua-apclientpp"
client = AP("", "", "ws://localhost:38281")  -- expect socket connected handler ...
done = false

function on_socket_connected()
    done = true
end

client:set_socket_connected_handler(on_socket_connected)

local t0 = os.clock()
while os.clock() - t0 < 2 do
    client:poll()
    if done then
        break
    end
end

client = nil
collectgarbage("collect")

if done then
    print("OK")
    os.exit(0)
else
    print("Timeout waiting for connect - is AP running on 38281?")
    os.exit(1)
end
