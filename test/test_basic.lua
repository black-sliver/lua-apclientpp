-- simply test loading the module and getting a field

AP = require "lua-apclientpp"
assert(AP)

---@type (string | nil)
local version = AP._VERSION
print("lua-apclientpp version: " .. version)
assert(type(version) == "string")

-- cleanup

AP = nil
version = nil
collectgarbage()
