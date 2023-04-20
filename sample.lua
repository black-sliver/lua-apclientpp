local AP = require "lua-apclientpp"

-- global to this mod
local game_name = "Secret of Evermore"
local items_handling = 7  -- full remote
local message_format = AP.RenderFormat.TEXT
local ap = nil


-- TODO: user input
local host = "localhost"
local slot = "Player1"
local password = ""


function connect(server, slot, password)
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
        ap:ConnectSlot(slot, password, items_handling, {"Lua-APClientPP"}, {0, 3, 9})
    end

    function on_slot_connected(slot_data)
        print("Slot connected")
        print(slot_data)
        print("missing locations: " .. table.concat(ap.missing_locations, ", "))
        print("checked locations: " .. table.concat(ap.checked_locations, ", "))
        ap:Say("Hello World!")
        ap:Bounce({name="test"}, {game_name})
        ap:Get({"counter"})
        ap:Set("counter", 0, true, {{"add", 1}}, {blerf="blubb"})
        ap:ConnectUpdate(nil, {"Lua-APClientPP", "DeathLink"})
        ap:LocationChecks({64000, 64001, 64002})
    end


    function on_slot_refused(reasons)
        print("Slot refused: " .. table.concat(reasons, ", "))
    end

    function on_items_received(items)
        print("Items received:")
        for _, item in ipairs(items) do
            print(item.item)
        end
    end

    function on_location_info(items)
        print("Locations scouted:")
        for _, item in ipairs(items) do
            print(item.item)
        end
    end

    function on_location_checked(locations)
        print("Locations checked:" .. table.concat(locations, ", "))
        print("Checked locations: " .. table.concat(ap.checked_locations, ", "))
    end

    function on_data_package_changed(data_package)
        print("Data package changed:")
        print(data_package)
    end

    function on_print(msg)
        print(msg)
    end

    function on_print_json(msg)
        print(ap:render_json(msg, message_format))
    end

    function on_bounced(bounce)
        print("Bounced:")
        print(bounce)
    end

    function on_retrieved(map)
        print("Retrieved:")
        for key, value in pairs(map) do
            print("  " .. key .. ": " .. tostring(value))
        end
    end

    function on_set_reply(message)
        print("Set Reply:")
        for key, value in pairs(message) do
            print("  " .. key .. ": " .. tostring(value))
        end
    end


    local uuid = ""
    ap = AP(uuid, game_name, server);

    ap:set_socket_connected_handler(on_socket_connected)
    ap:set_socket_error_handler(on_socket_error)
    ap:set_socket_disconnected_handler(on_socket_disconnected)
    ap:set_room_info_handler(on_room_info)
    ap:set_slot_connected_handler(on_slot_connected)
    ap:set_slot_refused_handler(on_slot_refused)
    ap:set_items_received_handler(on_items_received)
    ap:set_location_info_handler(on_location_info)
    ap:set_location_checked_handler(on_location_checked)
    ap:set_data_package_changed_handler(on_data_package_changed)
    ap:set_print_handler(on_print)
    ap:set_print_json_handler(on_print_json)
    ap:set_bounced_handler(on_bounced)
    ap:set_retrieved_handler(on_retrieved)
    ap:set_set_reply_handler(on_set_reply)
end


connect(host, slot, password)

print("Will run for 10 seconds ...")
local t0 = os.clock()
while os.clock() - t0 < 10 do
    ap:poll()  -- call this e.g. once per frame
end
print("shutting down...");
