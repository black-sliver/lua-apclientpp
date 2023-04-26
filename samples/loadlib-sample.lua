-- alternative way to load the library

local dll_ext = package.cpath:match("%p[\\|/]?%p(%a+)")

local AP = package.loadlib("path/to/lua-apclientpp." .. dll_ext, "luaopen_apclientpp")()

for name, _ in pairs(AP) do
    print(name)
end

-- see sample.lua for actual usage
