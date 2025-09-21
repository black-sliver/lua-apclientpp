-- we run it 4 times for all variations of the internal handler:
-- - debug.stacktrace available
-- - debug.stacktrace returns error
-- - debug.stacktrace missing
-- - debug missing

uri = "ws://127.0.0.1:38289" -- nothing should be listening there
AP = require "lua-apclientpp"

function on_socket_connected()
    print("Expected socket error")
    os.exit(1)
end

function on_socket_error(reason)
    error("Test") -- return error ...
end

function bad_traceback()
    error("bad traceback")
end

for i = 1, 4 do
    client = AP("", "", uri)
    client:set_socket_connected_handler(on_socket_connected)
    client:set_socket_error_handler(on_socket_error)

    local t0 = os.clock()
    while os.clock() - t0 < 2 do
        -- ... and check if it's properly forwarded ...
        status, err = pcall(function() client:poll() end)
        if err then
            if string.find(err, "Test") then
                if debug and debug.traceback and i == 1 then
                    -- ... and if debug module is available, it should contain a stack trace
                    if not string.find(err, "stack traceback") then
                        print("Missing stack trace")
                        os.exit(1)
                    end
                    debug = {traceback = bad_traceback} -- next time error calling traceback
                    break
                end
                if debug and debug.traceback then
                    debug = {} -- next time no stacktrace
                    break
                end
                if debug then
                    debug = nil -- next time no debug
                    break
                end
                print("OK")
                os.exit(0)
            else
                print("Wrong error returned")
                os.exit(1)
            end
        end
    end
end

print("Timeout waiting for error")
os.exit(1)
