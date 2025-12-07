#ifdef _WIN32
// we currently don't do project-wide defines for msvc, so define the relevant ones here
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif
#endif


//#define APCLIENT_DEBUG // to get debug output


#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas" // gcc doesn't know -Wunknown-warning-option
#pragma GCC diagnostic ignored "-Wunknown-warning-option" // clang doesn't know -Wtemplate-id-cdtor
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wtemplate-id-cdtor"
#pragma GCC diagnostic ignored "-Wnull-pointer-subtraction"
#pragma GCC diagnostic ignored "-Wdeprecated-literal-operator"
#ifdef _WIN32
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
#endif

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <apclient.hpp>
#include <luaglue/luacompat.h>
#include <luaglue/luapp.h>
#include <luaglue/luaref.h>
#include <luaglue/lua_json.h>

#ifndef _MSC_VER
#pragma GCC diagnostic pop
#pragma GCC diagnostic error "-Wconversion"
#endif


// IMPORTANT: apclientpp can't be used across threads, so capturing L is kind of ok
// FIXME: xmove would be valid outside of an apclient callback, but this breaks the capture and refs
// TODO: show an error when polling on a different thread


#ifndef DLL_EXPORT
#ifdef _WIN32
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT __attribute__ ((visibility ("default")))
#endif
#endif


// exported symbols
extern "C" {
DLL_EXPORT int luaopen_apclientpp(lua_State *L);
}

// print functions
//#define LUA_APCLIENTPP_USE_PRINTF // to use stdout/stderr directly instead

#ifdef LUA_APCLIENTPP_USE_PRINTF
static void print(lua_State *, const std::string& line)
{
    puts(line.c_str());
}
#define errorf(L, S, ...) fprintf(stderr, S "\n", ##__VA_ARGS__)
#else
static void print(lua_State *L, const std::string& line)
{
    lua_getglobal(L, "print");
    lua_pushstring(L, line.c_str());
    if (lua_pcall(L, 1, 0, 0) != 0) {
        fprintf(stderr, "Could not call Lua print!\n");
    }
}

static void errorf(lua_State *L, const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start (args, fmt);
    auto res = vsnprintf(buf, sizeof(buf), fmt, args);
    if (res < 0)
        strcpy(buf, "Error formatting error");
    else if ((size_t)res >= sizeof(buf))
        memcpy(buf + sizeof(buf) - 4, "...", 4);
    print(L, buf);
    va_end (args);
}
#endif

#ifdef NDEBUG
#define debug(...)
#else
#define debug print
#endif


// json conversion functions - NetworkItem and DataStorageOperation can be public, TextNode can not (_ prefix)
static void to_json(json& j, const APClient::NetworkItem& item) {
    j = json{
        {"item", item.item},
        {"location", item.location},
        {"player", item.player},
        {"flags", item.flags},
        {"index", item.index},
    };
}

static void from_json(const json& j, APClient::DataStorageOperation& op) {
    if (j.is_array() && j.size() == 2) {
        j[0].get_to(op.operation);
        j[1].get_to(op.value);
    } else {
        j.at("operation").get_to(op.operation);
        j.at("value").get_to(op.value);
    }
}

static void from_json(const json& j, std::list<APClient::TextNode>& nodes) {
    for (const auto& v: j) {
        nodes.push_back(APClient::TextNode::from_json(v));
    }
}

class BadArgumentException : public std::exception
{
public:
    BadArgumentException(int arg, const std::string& expected, const std::string& func="?")
    {
        message = "bad argument #" + std::to_string(arg) + " to '" + func + "' (" + expected + " expected)";
    }

    const char* what() const noexcept override
    {
        return message.c_str();
    }

private:
    std::string message;
};

/// Helper function to get integer from Lua stack and check bounds to fit c int
static int checkcint(lua_State *L, int arg)
{
    lua_Integer val = luaL_checkinteger(L, arg);
    if (sizeof(lua_Integer) != sizeof(int)) {
        constexpr int lower = std::numeric_limits<int>::min();
        constexpr int upper = std::numeric_limits<int>::max();
        if (val < lower || val > upper) {
            {
                BadArgumentException ex(arg, std::to_string(lower) + " <= integer <= " + std::to_string(upper));
                lua_pushstring(L, ex.what());
            }
            lua_error(L);
        }
    }
    return static_cast<int>(val);
}

// subclass for extra fields
// NOTE: we still need some C functions for variable arguments
// TODO: make lua glue support this use-case better
class LuaAPClient : public APClient
{
public:
    LuaAPClient(lua_State *L, const std::string& uuid, const std::string& game, const std::string& uri = DEFAULT_URI)
        : APClient(uuid, game, uri), _L(L)
    {
        // TODO: cert Store

        // connect internal handlers
        APClient* parent = this;
        parent->set_slot_connected_handler([this](const json& slot_data) {
            on_slot_connected(slot_data);
        });
        parent->set_location_checked_handler([this](const std::list<int64_t>& locations) {
            on_location_checked(locations);
        });
    }

    virtual ~LuaAPClient()
    {
        unref(socket_connected_cb);
        unref(socket_error_cb);
        unref(socket_disconnected_cb);
        unref(room_info_cb);
        unref(slot_connected_cb);
        unref(slot_refused_cb);
        unref(items_received_cb);
        unref(location_info_cb);
        unref(location_checked_cb);
        unref(data_package_changed_cb);
        unref(print_cb);
        unref(print_json_cb);
        unref(bounced_cb);
        unref(retrieved_cb);
        unref(set_reply_cb);
        unref(checked_locations);
        unref(missing_locations);
    }

    // internal handlers

    void on_slot_connected(const json& slot_data)
    {
        // sync location tables
        assign_set("checked_locations", get_checked_locations(), 1);
        assign_set("missing_locations", get_missing_locations(), 1);

        if (slot_connected_cb.valid()) {
            lua_pushcfunction(_L, error_handler);
            lua_rawgeti(_L, LUA_REGISTRYINDEX, slot_connected_cb.ref);
            json_to_lua(_L, slot_data);
            if (lua_pcall(_L, 1, 0, -3)) {
                cb_error("slot_connected");
            }
            lua_pop(_L, 1);
        }
    }

    void on_location_checked(const std::list<int64_t>& locations)
    {
        // sync location tables
        add_list("checked_locations", locations, 1);
        assign_set("missing_locations", get_missing_locations(), 1);

        if (location_checked_cb.valid()) {
            lua_pushcfunction(_L, error_handler);
            lua_rawgeti(_L, LUA_REGISTRYINDEX, location_checked_cb.ref);
            {
                json j = locations;
                json_to_lua(_L, j);
            }
            if (lua_pcall(_L, 1, 0, -3)) {
                cb_error("location_checked");
            }
            lua_pop(_L, 1);
        }
    }

    // lua methods

    void set_socket_connected_handler(LuaRef ref)
    {
        unref(socket_connected_cb);
        socket_connected_cb = ref;

        APClient* parent = this;
        parent->set_socket_connected_handler([this]() {
            lua_pushcfunction(_L, error_handler);
            lua_rawgeti(_L, LUA_REGISTRYINDEX, socket_connected_cb.ref);
            if (lua_pcall(_L, 0, 0, -2)) {
                cb_error("socket_connected");
            }
            lua_pop(_L, 1);
        });
    }

    void set_socket_error_handler(LuaRef ref)
    {
        unref(socket_error_cb);
        socket_error_cb = ref;

        APClient* parent = this;
        parent->set_socket_error_handler([this](const std::string& msg) {
            lua_pushcfunction(_L, error_handler);
            lua_rawgeti(_L, LUA_REGISTRYINDEX, socket_error_cb.ref);
            lua_pushstring(_L, msg.c_str());
            if (lua_pcall(_L, 1, 0, -3)) {
                cb_error("socket_error");
            }
            lua_pop(_L, 1);
        });
    }

    void set_socket_disconnected_handler(LuaRef ref)
    {
        unref(socket_disconnected_cb);
        socket_disconnected_cb = ref;

        APClient* parent = this;
        parent->set_socket_disconnected_handler([this]() {
            lua_pushcfunction(_L, error_handler);
            lua_rawgeti(_L, LUA_REGISTRYINDEX, socket_disconnected_cb.ref);
            if (lua_pcall(_L, 0, 0, -2)) {
                cb_error("socket_disconnected");
            }
            lua_pop(_L, 1);
        });
    }

    void set_room_info_handler(LuaRef ref)
    {
        unref(room_info_cb);
        room_info_cb = ref;

        APClient* parent = this;
        parent->set_room_info_handler([this]() {
            lua_pushcfunction(_L, error_handler);
            lua_rawgeti(_L, LUA_REGISTRYINDEX, room_info_cb.ref);
            if (lua_pcall(_L, 0, 0, -2)) {
                cb_error("room_info");
            }
            lua_pop(_L, 1);
        });
    }

    void set_slot_connected_handler(LuaRef ref)
    {
        unref(slot_connected_cb);
        slot_connected_cb = ref;
    }

    void set_slot_refused_handler(LuaRef ref)
    {
        unref(slot_refused_cb);
        slot_refused_cb = ref;

        APClient* parent = this;
        parent->set_slot_refused_handler([this](const std::list<std::string>& reason) {
            lua_pushcfunction(_L, error_handler);
            lua_rawgeti(_L, LUA_REGISTRYINDEX, slot_refused_cb.ref);
            {
                json j = reason;
                json_to_lua(_L, j);
            }
            if (lua_pcall(_L, 1, 0, -3)) {
                cb_error("slot_refused");
            }
            lua_pop(_L, 1);
        });
    }

    void set_items_received_handler(LuaRef ref)
    {
        static_assert(sizeof(lua_Number) >= 8, "Can't represent required ID range in lua_Number");

        unref(items_received_cb);
        items_received_cb = ref;

        APClient* parent = this;
        parent->set_items_received_handler([this](const std::list<NetworkItem>& items) {
            lua_pushcfunction(_L, error_handler);
            lua_rawgeti(_L, LUA_REGISTRYINDEX, items_received_cb.ref);
            {
                json j = items;
                json_to_lua(_L, j);
            }
            if (lua_pcall(_L, 1, 0, -3)) {
                cb_error("items_received");
            }
            lua_pop(_L, 1);
        });
    }

    void set_location_info_handler(LuaRef ref)
    {
        unref(location_info_cb);
        location_info_cb = ref;

        APClient* parent = this;
        parent->set_location_info_handler([this](const std::list<NetworkItem>& items) {
            lua_pushcfunction(_L, error_handler);
            lua_rawgeti(_L, LUA_REGISTRYINDEX, location_info_cb.ref);
            {
                json j = items;
                json_to_lua(_L, j);
            }
            if (lua_pcall(_L, 1, 0, -3)) {
                cb_error("location_info");
            }
            lua_pop(_L, 1);
        });
    }

    void set_location_checked_handler(LuaRef ref)
    {
        unref(location_checked_cb);
        location_checked_cb = ref;
    }

    void set_data_package_changed_handler(LuaRef ref)
    {
        unref(data_package_changed_cb);
        data_package_changed_cb = ref;

        APClient* parent = this;
        parent->set_data_package_changed_handler([this](const json& data_package) {
            lua_pushcfunction(_L, error_handler);
            lua_rawgeti(_L, LUA_REGISTRYINDEX, data_package_changed_cb.ref);
            json_to_lua(_L, data_package);
            if (lua_pcall(_L, 1, 0, -3)) {
                cb_error("data_package_changed");
            }
            lua_pop(_L, 1);
        });
    }

    void set_print_handler(LuaRef ref)
    {
        unref(print_cb);
        print_cb = ref;

        APClient* parent = this;
        parent->set_print_handler([this](const std::string& msg) {
            lua_pushcfunction(_L, error_handler);
            lua_rawgeti(_L, LUA_REGISTRYINDEX, print_cb.ref);
            lua_pushstring(_L, msg.c_str());
            if (lua_pcall(_L, 1, 0, -3)) {
                cb_error("print");
            }
            lua_pop(_L, 1);
        });
    }

    void set_print_json_handler(LuaRef ref)
    {
        unref(print_json_cb);
        print_json_cb = ref;

        APClient* parent = this;
        parent->set_print_json_handler([this](const json& command) {
            lua_pushcfunction(_L, error_handler);
            lua_rawgeti(_L, LUA_REGISTRYINDEX, print_json_cb.ref);
            json_to_lua(_L, command);
            lua_getfield(_L, -1, "data");
            lua_insert(_L, -2); // first arg is data, second is full command
            if (lua_pcall(_L, 2, 0, -4)) {
                cb_error("print_json");
            }
            lua_pop(_L, 1);
        });
    }

    void set_bounced_handler(LuaRef ref)
    {
        unref(bounced_cb);
        bounced_cb = ref;

        APClient* parent = this;
        parent->set_bounced_handler([this](const json& bounce) {
            lua_pushcfunction(_L, error_handler);
            lua_rawgeti(_L, LUA_REGISTRYINDEX, bounced_cb.ref);
            json_to_lua(_L, bounce);
            if (lua_pcall(_L, 1, 0, -3)) {
                cb_error("bounced");
            }
            lua_pop(_L, 1);
        });
    }

    void set_retrieved_handler(LuaRef ref)
    {
        unref(retrieved_cb);
        retrieved_cb = ref;

        APClient* parent = this;
        parent->set_retrieved_handler([this](const std::map<std::string, json>& data, const json& message) {
            lua_pushcfunction(_L, error_handler);
            lua_rawgeti(_L, LUA_REGISTRYINDEX, retrieved_cb.ref);
            {
                json j = data;
                std::list<std::string> keys;
                for (const auto& item: j.items())
                    keys.push_back(item.key());
                json_to_lua(_L, j);
                json_to_lua(_L, keys);
                json_to_lua(_L, message);
            }
            if (lua_pcall(_L, 3, 0, -5)) {
                cb_error("retrieved");
            }
            lua_pop(_L, 1);
        });
    }

    void set_set_reply_handler(LuaRef ref)
    {
        unref(set_reply_cb);
        set_reply_cb = ref;

        APClient* parent = this;
        parent->set_set_reply_handler([this](const json& message) {
            lua_pushcfunction(_L, error_handler);
            lua_rawgeti(_L, LUA_REGISTRYINDEX, set_reply_cb.ref);
            json_to_lua(_L, message);
            if (lua_pcall(_L, 1, 0, -3)) {
                cb_error("set_reply");
            }
            lua_pop(_L, 1);
        });
    }

    bool StatusUpdate(int status)
    {
        APClient* parent = this;
        return parent->StatusUpdate((ClientStatus)status);
    }

    bool LocationChecks(const json& j)
    {
        std::list<int64_t> locations;
        try {
            locations = j.get<std::list<int64_t>>();
        } catch (const std::exception&) {
            if (!j.is_object() || !j.empty()) {
                throw BadArgumentException(2, "array of integer", "LocationChecks");
            }
        }

        APClient* parent = this;
        if (parent->LocationChecks(locations)) {
            // sync location tables
            add_list("checked_locations", locations, 1);
            assign_set("missing_locations", get_missing_locations(), 1);
            return true;
        }

        return false;
    }

    bool Get(const json& j, const json& extra = json::value_t::null)
    {
        APClient* parent = this;
        if (!extra.is_null() && !extra.is_object()) {
            throw BadArgumentException(3, "table or nil", "Get");
        }
        std::list<std::string> keys;
        try {
            keys = j.get<std::list<std::string>>();
        } catch (const std::exception&) {
            throw BadArgumentException(2, "array of string", "Get");
        }
        return parent->Get(keys, extra);
    }

    bool SetNotify(const json& j)
    {
        std::list<std::string> keys;
        try {
            keys = j.get<std::list<std::string>>();
        } catch (const std::exception&) {
            throw BadArgumentException(2, "array of string", "SetNotify");
        }

        APClient* parent = this;
        return parent->SetNotify(keys);
    }

    int get_state() const
    {
        const APClient* parent = this;
        return (int)parent->get_state();
    }

    json get_players() const
    {
        const APClient* parent = this;
        return parent->get_players();
    }

    static int poll(lua_State *L)
    {
        LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
        try {
            if (self->_L != L) {
                const char* msg = "Lua state changed. Multi-threading not supported!";
                errorf(L, "%s", msg);
                luaL_error(L, "%s", msg);
                return 0;
            }
            APClient* parent = self;
            parent->poll();
        } catch (const std::exception& ex) {
            self->push_error(ex.what());
        }

        if (!self->errors.empty()) {
            lua_pushstring(L, self->errors.c_str());
            self->errors.clear();
            lua_error(L);
            return 0; // LCOV_EXCL_LINE // unreachable
        }

        lua_pushboolean(L, true);
        return 1;
    }

    // lua interface implementation details

#if !defined _MSC_VER || _MSC_VER >= 1911
    static constexpr char Lua_Name[] = "APClient";
#else
    static char Lua_Name[]; // = "APClient"; // assign this in implementation
#endif

    static LuaAPClient* luaL_checkthis(lua_State *L, int narg)
    {
        return * (LuaAPClient**)luaL_checkudata(L, narg, LuaAPClient::Lua_Name);
    }

    static int __index(lua_State *L)
    {
        LuaAPClient *self = luaL_checkthis(L, 1);
        const char* key = luaL_checkstring(L, 2);
        if (strcmp(key, "checked_locations") == 0) {
            if (self->checked_locations.valid()) {
                lua_rawgeti(self->_L, LUA_REGISTRYINDEX, self->checked_locations.ref);
                return 1;
            }
        }
        else if (strcmp(key, "missing_locations") == 0) {
            if (self->missing_locations.valid()) {
                lua_rawgeti(self->_L, LUA_REGISTRYINDEX, self->missing_locations.ref);
                return 1;
            }
        }
        else if (lua_getmetatable(L, 1)) {
            lua_insert(L, -2);
            lua_rawget(L, -2);
            return 1;
        }
        return 0;
    }

    static int __newindex(lua_State *L)
    {
        LuaAPClient *self = luaL_checkthis(L, 1);
        const char* key = luaL_checkstring(L, 2);
        if (strcmp(key, "checked_locations") == 0) {
            self->unref(self->checked_locations);
            self->checked_locations.ref = luaL_ref(self->_L, LUA_REGISTRYINDEX);
        }
        else if (strcmp(key, "missing_locations") == 0) {
            self->unref(self->missing_locations);
            self->missing_locations.ref = luaL_ref(self->_L, LUA_REGISTRYINDEX);
        }
        return 0;
    }

private:
    void unref(LuaRef& ref)
    {
        if (ref.valid())
            luaL_unref(_L, LUA_REGISTRYINDEX, ref.ref); 
        ref = {};
    }

    void cb_error(const std::string& name)
    {
        const char* err = lua_tostring(_L, -1);
        std::string error_message = "Error calling " + name + "_handler:\n" + (err ? err : "<null>");
        push_error(error_message);
        lua_pop(_L, 1); // pop error
    }

    void print_error(const std::string& message)
    {
        errorf(_L, "%s", message.c_str());
    }

    void push_error(const std::string& message)
    {
        print_error(message.c_str());
        if (!errors.empty())
            errors += "\n---\n";
        errors += message;
    }

    static int error_handler(lua_State *L)
    {
        lua_getglobal(L, "debug");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "traceback");
            if (lua_isfunction(L, -1)) {
                lua_pushvalue(L, -3); // original message
                lua_pushinteger(L, 2); // don't show error_handler
                if (lua_pcall(L, 2, 1, 0) == LUA_OK) {
                    // stack: original error, debug, message
                    lua_replace(L, -3);
                } else {
                    lua_pop(L, 1); // pop error
                }
            } else {
                lua_pop(L, 1); // pop traceback
            }
        }
        lua_pop(L, 1); // pop debug
        return 1; // original error or traceback
    }

    template <class T>
    void assign_set(const char* key, const std::set<T>& set, int table = -1)
    {
        // get table by name
        lua_getfield(_L, table, key);
        // assign values
        lua_Integer n = 0;
        for (const auto& v: set) {
            lua_pushinteger(_L, ++n);
            Lua(_L).Push(v);
            lua_rawset(_L, -3);
        }
        // delete old values
        lua_Integer len = luaL_len(_L, -1);
        for (lua_Integer i = n + 1; i <= len; i++) {
            lua_pushinteger(_L, i);
            lua_pushnil(_L);
            lua_rawset(_L, -3);
        }
        // pop table
        lua_pop(_L, 1);
    }

    template <class T>
    bool contains(const T& v, int table = -1)
    {
        if (table < 0) table -= 2;
        Lua(_L).Push(v); // push v
        lua_pushnil(_L); // push nil for first key
        while (lua_next(_L, table) != 0) {
            if (lua_rawequal(_L, -1, -3)) {
                // pop value, key and v
                lua_pop(_L, 3);
                return true;
            }
            lua_pop(_L, 1); // pop value, keep key
        }
        // lua_next will pop the last key once done
        lua_pop(_L, 1); // pop v
        return false;
    }

    template <class T>
    void add_list(const char* key, const std::list<T>& lst, int table = -1)
    {
        // get table by name
        lua_getfield(_L, table, key);
        // append items if they don't exist already
        lua_Integer n = luaL_len(_L, -1);
        for (const auto& v: lst) {
            if (!contains(v)) {
                lua_pushinteger(_L, ++n);
                Lua(_L).Push(v);
                lua_rawset(_L, -3);
            }
        }
        // pop table
        lua_pop(_L, 1);
    }

    lua_State *_L;
    LuaRef socket_connected_cb;
    LuaRef socket_error_cb;
    LuaRef socket_disconnected_cb;
    LuaRef room_info_cb;
    LuaRef slot_connected_cb;
    LuaRef slot_refused_cb;
    LuaRef items_received_cb;
    LuaRef location_info_cb;
    LuaRef location_checked_cb;
    LuaRef data_package_changed_cb;
    LuaRef print_cb;
    LuaRef print_json_cb;
    LuaRef bounced_cb;
    LuaRef retrieved_cb;
    LuaRef set_reply_cb;
    LuaRef checked_locations;
    LuaRef missing_locations;
    std::string errors;
};

#if defined _MSC_VER && _MSC_VER < 1911
decltype(APClient::DEFAULT_URI) APClient::DEFAULT_URI = "localhost:38281";
decltype(LuaAPClient::Lua_Name) LuaAPClient::Lua_Name = "APClient";
decltype(LuaJson_EmptyArray::Lua_Name) LuaJson_EmptyArray::Lua_Name = "LuaJson_EmptyArray";
#elif __cplusplus < 201500L // c++14 needs a proper declaration
decltype(APClient::DEFAULT_URI) constexpr APClient::DEFAULT_URI;
decltype(LuaAPClient::Lua_Name) constexpr LuaAPClient::Lua_Name;
decltype(LuaJson_EmptyArray::Lua_Name) constexpr LuaJson_EmptyArray::Lua_Name;
#endif

// C functions - read above
// NOTE: new, call and del will probably always be C or static

static int apclient_new(lua_State *L)
{
    debug(L, "APClient.new");
    const char* uuid = luaL_checkstring(L, 1);
    const char* game = luaL_checkstring(L, 2);
    const char* host = luaL_checkstring(L, 3);

    auto p = static_cast<LuaAPClient**>(lua_newuserdata(L, sizeof(LuaAPClient*)));

    try {
        LuaAPClient *self = new LuaAPClient(L, uuid, game, host);
        *p = self;
        luaL_getmetatable(L, LuaAPClient::Lua_Name);
        lua_setmetatable(L, -2);
        lua_newtable(L);
        lua_setfield(L, -2, "checked_locations");
        lua_newtable(L);
        lua_setfield(L, -2, "missing_locations");
        debug(L, "APClient instance created");
        return 1;
    } catch (const std::exception& ex) {
        // NOTE: userdata will be freed by the GC since we don't return it
        lua_pushstring(L, ex.what());
    }
	lua_error(L);
	return 0; // LCOV_EXCL_LINE // unreachable
}

static int apclient_call(lua_State *L)
{
    lua_remove(L, 1);
    return apclient_new(L);
}

static int apclient_del(lua_State *L)
{
    debug(L, "APClient.__gc");
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    delete self;
    debug(L, "APClient instance deleted");
    return 0;
}

static int apclient_reset(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    self->reset();
    return 0;
}

static int apclient_get_player_alias(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    lua_Integer slot = luaL_checkinteger(L, 2);
    if (sizeof(lua_Integer) > sizeof(int)) {
        if (slot < std::numeric_limits<int>::min() || slot > std::numeric_limits<int>::max()) {
            slot = std::numeric_limits<int>::min(); // non-existent
        }
    }
    lua_pushstring(L, self->get_player_alias(static_cast<int>(slot)).c_str());
    return 1;
}

static int apclient_get_player_game(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    lua_Integer slot = luaL_checkinteger(L, 2);
    if (sizeof(lua_Integer) > sizeof(int)) {
        if (slot < std::numeric_limits<int>::min() || slot > std::numeric_limits<int>::max()) {
            slot = std::numeric_limits<int>::min(); // non-existent
        }
    }
    lua_pushstring(L, self->get_player_game(static_cast<int>(slot)).c_str());
    return 1;
}

static int apclient_get_game(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    lua_pushstring(L, self->get_game().c_str());
    return 1;
}

static int apclient_get_location_name(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    int64_t code;
    if (lua_isinteger(L, 2))
        code = (int64_t)lua_tointeger(L, 2);
    else
        code = (int64_t)luaL_checknumber(L, 2);

    if (lua_gettop(L) >= 3) {
        if (lua_isnil(L, 3)) {
            lua_pushstring(L, self->get_location_name(code, self->get_game()).c_str());
        } else {
            const char* game = luaL_checkstring(L, 3);
            lua_pushstring(L, self->get_location_name(code, game).c_str());
        }
        return 1;
    } else {
        luaL_error(L, "missing argument #2 to 'get_location_name' (string or nil expected)");
        return 0; // LCOV_EXCL_LINE // unreachable
    }
}

static int apclient_get_location_id(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    int64_t id = self->get_location_id(luaL_checkstring(L, 2));
    static_assert(sizeof(lua_Integer) >= 7 || sizeof(lua_Number) >= 8, "Can't fit valid ID range in Lua number");
    if (id >= std::numeric_limits<lua_Integer>::min() && id <= std::numeric_limits<lua_Integer>::max())
        lua_pushinteger(L, static_cast<lua_Integer>(id));
    else
        lua_pushnumber(L, static_cast<lua_Number>(id)); // ignores loss of precision if server gives invalid ID
    return 1;
}

static int apclient_get_item_name(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    int64_t code;
    if (lua_isinteger(L, 2))
        code = (int64_t)lua_tointeger(L, 2);
    else
        code = (int64_t)luaL_checknumber(L, 2);

    if (lua_gettop(L) >= 3) {
        if (lua_isnil(L, 3)) {
            lua_pushstring(L, self->get_item_name(code, self->get_game()).c_str());
        } else {
            const char* game = luaL_checkstring(L, 3);
            lua_pushstring(L, self->get_item_name(code, game).c_str());
        }
        return 1;
    } else {
        luaL_error(L, "missing argument #2 to 'get_item_name' (string or nil expected)");
        return 0; // LCOV_EXCL_LINE // unreachable
    }
}

static int apclient_get_item_id(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    int64_t id = self->get_item_id(luaL_checkstring(L, 2));
    static_assert(sizeof(lua_Integer) >= 7 || sizeof(lua_Number) >= 8, "Can't fit valid ID range in Lua number");
    if (id >= std::numeric_limits<lua_Integer>::min() && id <= std::numeric_limits<lua_Integer>::max())
        lua_pushinteger(L, static_cast<lua_Integer>(id));
    else
        lua_pushnumber(L, static_cast<lua_Number>(id)); // ignores loss of precision if server gives invalid ID
    return 1;
}

static int apclient_ConnectSlot(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    const char* slot = luaL_checkstring(L, 2);
    const char* password = luaL_checkstring(L, 3);
    int items_handling = checkcint(L, 4);

    try {
        std::list<std::string> tags;
        APClient::Version version = {0, 0, 0};

        if (lua_gettop(L) >= 5) {
            try {
                auto j = lua_to_json(L, 5);
                if (j.size() > 0)
                    tags = j.get<std::list<std::string>>();
            } catch (const std::exception&) {
                throw BadArgumentException(5, "optional array of string", "ConnectSlot");
            }
        }
        if (lua_gettop(L) >= 6) {
            try {
                json jversion = lua_to_json(L, 6);
                if (jversion.is_object()) {
                    version = APClient::Version::from_json(jversion);
                } else if (jversion.is_array()) {
                    if (jversion.size() > 0)
                        version.ma = jversion[0].get<int>();
                    if (jversion.size() > 1)
                        version.mi = jversion[1].get<int>();
                    if (jversion.size() > 2)
                        version.build = jversion[2].get<int>();
                }
            } catch (const std::exception&) {
                throw BadArgumentException(6, "optional version table or array of integer", "ConnectSlot");
            }
        }

        bool res;
        if (version.ma > 0 || version.mi > 0 || version.build > 0)
            res = self->ConnectSlot(slot, password, items_handling, tags, version);
        else
            res = self->ConnectSlot(slot, password, items_handling, tags);

        lua_pushboolean(L, res);
        return 1;
    } catch (const std::exception& ex) {
        lua_pushstring(L, ex.what());
    }
    lua_error(L);
    return 0; // LCOV_EXCL_LINE // unreachable
}

static int apclient_render_json(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    APClient::RenderFormat fmt = APClient::RenderFormat::TEXT;
    if (lua_gettop(L) >= 3) {
        fmt = (APClient::RenderFormat)luaL_checkinteger(L, 3);
    }

    try {
        std::list<APClient::TextNode> msg;
        try {
            from_json(lua_to_json(L, 2), msg);
        } catch (const std::exception&) {
            throw BadArgumentException(2, "array of TextNode", "render_json");
        }

        try {
            std::string res = self->render_json(msg, fmt);
            lua_pushstring(L, res.c_str());
            return 1;
        } catch (const std::invalid_argument&) {
            throw; // rethrow
        } catch (...) { // LCOV_EXCL_START
            // this is probably unreachable; if the content of a text node was somehow invalid, still return string
            lua_pushstring(L, "(Invalid chat message)");
            return 1;
        }  // LCOV_EXCL_STOP
    } catch (const std::exception& ex) {
        lua_pushstring(L, ex.what());
    }
    lua_error(L);
    return 0; // LCOV_EXCL_LINE // unreachable
}

static int apclient_get_state(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    lua_pushinteger(L, self->get_state());
    return 1;
}

static int apclient_get_seed(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    lua_pushstring(L, self->get_seed().c_str());
    return 1;
}

static int apclient_get_slot(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    lua_pushstring(L, self->get_slot().c_str());
    return 1;
}

static int apclient_get_player_number(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    lua_pushinteger(L, self->get_player_number());
    return 1;
}

static int apclient_get_team_number(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    lua_pushinteger(L, self->get_team_number());
    return 1;
}

static int apclient_get_hint_points(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    lua_pushinteger(L, self->get_hint_points());
    return 1;
}

static int apclient_get_hint_cost_points(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    lua_pushinteger(L, self->get_hint_cost_points());
    return 1;
}

static int apclient_get_hint_cost_percent(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    lua_pushinteger(L, self->get_hint_cost_percent());
    return 1;
}

static int apclient_is_data_package_valid(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    lua_pushboolean(L, self->is_data_package_valid());
    return 1;
}

static int apclient_get_server_time(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    lua_pushnumber(L, self->get_server_time());
    return 1;
}

static int apclient_get_players(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    json_to_lua(L, self->get_players());
    return 1;
}

static int apclient_set_socket_connected_handler(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    LuaRef ref;
    lua_pushvalue(L, 2); // make copy on top of stack
    ref.ref = luaL_ref(L, LUA_REGISTRYINDEX); // pop copy and store
    self->set_socket_connected_handler(ref);
    return 0;
}

static int apclient_set_socket_error_handler(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    LuaRef ref;
    lua_pushvalue(L, 2); // make copy on top of stack
    ref.ref = luaL_ref(L, LUA_REGISTRYINDEX); // pop copy and store
    self->set_socket_error_handler(ref);
    return 0;
}

static int apclient_set_socket_disconnected_handler(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    LuaRef ref;
    lua_pushvalue(L, 2); // make copy on top of stack
    ref.ref = luaL_ref(L, LUA_REGISTRYINDEX); // pop copy and store
    self->set_socket_disconnected_handler(ref);
    return 0;
}

static int apclient_set_room_info_handler(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    LuaRef ref;
    lua_pushvalue(L, 2); // make copy on top of stack
    ref.ref = luaL_ref(L, LUA_REGISTRYINDEX); // pop copy and store
    self->set_room_info_handler(ref);
    return 0;
}

static int apclient_set_slot_connected_handler(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    LuaRef ref;
    lua_pushvalue(L, 2); // make copy on top of stack
    ref.ref = luaL_ref(L, LUA_REGISTRYINDEX); // pop copy and store
    self->set_slot_connected_handler(ref);
    return 0;
}

static int apclient_set_slot_refused_handler(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    LuaRef ref;
    lua_pushvalue(L, 2); // make copy on top of stack
    ref.ref = luaL_ref(L, LUA_REGISTRYINDEX); // pop copy and store
    self->set_slot_refused_handler(ref);
    return 0;
}

static int apclient_set_items_received_handler(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    LuaRef ref;
    lua_pushvalue(L, 2); // make copy on top of stack
    ref.ref = luaL_ref(L, LUA_REGISTRYINDEX); // pop copy and store
    self->set_items_received_handler(ref);
    return 0;
}

static int apclient_set_location_info_handler(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    LuaRef ref;
    lua_pushvalue(L, 2); // make copy on top of stack
    ref.ref = luaL_ref(L, LUA_REGISTRYINDEX); // pop copy and store
    self->set_location_info_handler(ref);
    return 0;
}

static int apclient_set_location_checked_handler(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    LuaRef ref;
    lua_pushvalue(L, 2); // make copy on top of stack
    ref.ref = luaL_ref(L, LUA_REGISTRYINDEX); // pop copy and store
    self->set_location_checked_handler(ref);
    return 0;
}

static int apclient_set_data_package_changed_handler(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    LuaRef ref;
    lua_pushvalue(L, 2); // make copy on top of stack
    ref.ref = luaL_ref(L, LUA_REGISTRYINDEX); // pop copy and store
    self->set_data_package_changed_handler(ref);
    return 0;
}

static int apclient_set_print_handler(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    LuaRef ref;
    lua_pushvalue(L, 2); // make copy on top of stack
    ref.ref = luaL_ref(L, LUA_REGISTRYINDEX); // pop copy and store
    self->set_print_handler(ref);
    return 0;
}

static int apclient_set_print_json_handler(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    LuaRef ref;
    lua_pushvalue(L, 2); // make copy on top of stack
    ref.ref = luaL_ref(L, LUA_REGISTRYINDEX); // pop copy and store
    self->set_print_json_handler(ref);
    return 0;
}

static int apclient_set_bounced_handler(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    LuaRef ref;
    lua_pushvalue(L, 2); // make copy on top of stack
    ref.ref = luaL_ref(L, LUA_REGISTRYINDEX); // pop copy and store
    self->set_bounced_handler(ref);
    return 0;
}

static int apclient_set_retrieved_handler(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    LuaRef ref;
    lua_pushvalue(L, 2); // make copy on top of stack
    ref.ref = luaL_ref(L, LUA_REGISTRYINDEX); // pop copy and store
    self->set_retrieved_handler(ref);
    return 0;
}

static int apclient_set_set_reply_handler(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    LuaRef ref;
    lua_pushvalue(L, 2); // make copy on top of stack
    ref.ref = luaL_ref(L, LUA_REGISTRYINDEX); // pop copy and store
    self->set_set_reply_handler(ref);
    return 0;
}

static int apclient_ConnectUpdate(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);

    bool has_items_handling = !lua_isnil(L, 2);
    bool has_tags = !lua_isnil(L, 3);
    int items_handling = has_items_handling ? checkcint(L, 2) : 0;

    try {
        if (!has_tags) {
            if (!has_items_handling) {
                // invalid arguments
                throw BadArgumentException(3, "items_handling or tags not nil", "ConnectUpdate");
            } else {
                // update items_handling
                lua_pushboolean(L, self->ConnectUpdate(true, items_handling, false, {}));
            }
        } else {
            std::list<std::string> tags;
            try {
                auto j = lua_to_json(L, 3);
                if (j.size() > 0)
                    tags = j.get<std::list<std::string>>();
            } catch (const std::exception&) {
                throw BadArgumentException(3, "array of string", "ConnectUpdate");
            }

            if (has_items_handling) {
                // update both
                lua_pushboolean(L, self->ConnectUpdate(true, items_handling, true, tags));
            } else {
                // update tags
                lua_pushboolean(L, self->ConnectUpdate(false, 0, true, tags));
            }
        }

        return 1;
    } catch (const websocketpp::exception&) {
        // network error -> false; TODO: this should be handled in apclientpp with NO_EXCEPTION
        lua_pushboolean(L, false);
        return 1;
    } catch (const std::exception& ex) {
        lua_pushstring(L, ex.what());
    }
    lua_error(L);
    return 0; // LCOV_EXCL_LINE // unreachable
}

static int apclient_Bounce(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    try {
        json data = lua_to_json(L, 2);
        std::list<std::string> games;
        std::list<int> slots;
        std::list<std::string> tags;

        if (lua_gettop(L) >= 3) {
            try {
                auto j = lua_to_json(L, 3);
                if (j.size() > 0)
                    games = j.get<std::list<std::string>>();
            } catch (const std::exception&) {
                throw BadArgumentException(3, "optional array of string", "Bounce");
            }
        }

        if (lua_gettop(L) >= 4) {
            try {
                auto j = lua_to_json(L, 4);
                if (j.size() > 0)
                    slots = j.get<std::list<int>>();
            } catch (const std::exception&) {
                throw BadArgumentException(4, "optional array of integer", "Bounce");
            }
        }

        if (lua_gettop(L) >= 5) {
            try {
                auto j = lua_to_json(L, 5);
                if (j.size() > 0)
                    tags = j.get<std::list<std::string>>();
            } catch (const std::exception&) {
                throw BadArgumentException(5, "optional array of string", "Bounce");
            }
        }

        bool res = self->Bounce(data, games, slots, tags);
        lua_pushboolean(L, res);
        return 1;
    } catch (const std::exception& ex) {
        lua_pushstring(L, ex.what());
    }
    lua_error(L);
    return 0; // LCOV_EXCL_LINE // unreachable
}

static int apclient_Say(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    const char* text = luaL_checkstring(L, 2);
    lua_pushboolean(L, self->Say(text));
    return 1;
}

static int apclient_Sync(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    lua_pushboolean(L, self->Sync());
    return 1;
}

static int apclient_StatusUpdate(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    lua_pushboolean(L, self->StatusUpdate(checkcint(L, 2)));
    return 1;
}

static int apclient_LocationChecks(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    try {
        lua_pushboolean(L, self->LocationChecks(lua_to_json(L, 2)));
        return 1;
    } catch (const std::exception& ex) {
        lua_pushstring(L, ex.what());
    }
    lua_error(L);
    return 0; // LCOV_EXCL_LINE // unreachable
}

static int apclient_LocationScouts(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);

    int create_as_hints = 0;
    if (lua_gettop(L) >= 3) {
        if (lua_isboolean(L, 3))
            create_as_hints = lua_toboolean(L, 3) ? 1 : 0;
        else
            create_as_hints = (int)luaL_checkinteger(L, 3);
    }

    try {
        std::list<int64_t> locations;
        {
            json j = lua_to_json(L, 2);
            try {
                locations = j.get<std::list<int64_t>>();
            } catch (const std::exception&) {
                if (!j.is_object() || !j.empty()) {
                    throw BadArgumentException(2, "array of integer", "LocationScouts");
                }
            }
        }
        lua_pushboolean(L, self->LocationScouts(locations, create_as_hints));
        return 1;
    } catch (const std::exception& ex) {
        lua_pushstring(L, ex.what());
    }
    lua_error(L);
    return 0; // LCOV_EXCL_LINE // unreachable
}

static int apclient_Get(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    try {
        json keys = lua_to_json(L, 2);
        json extra;
        if (lua_gettop(L) >= 3)
            extra = lua_to_json(L, 3);

        bool res = self->Get(keys, extra);
        lua_pushboolean(L, res);
        return 1;
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
    }
    lua_error(L);
    return 0; // LCOV_EXCL_LINE // unreachable
}

static int apclient_SetNotify(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    try {
        lua_pushboolean(L, self->SetNotify(lua_to_json(L, 2)));
        return 1;
    } catch (const std::exception& ex) {
        lua_pushstring(L, ex.what());
    }
    lua_error(L);
    return 0; // LCOV_EXCL_LINE // unreachable
}

static int apclient_Set(lua_State *L)
{
    LuaAPClient *self = LuaAPClient::luaL_checkthis(L, 1);
    const char* key = luaL_checkstring(L, 2);
    luaL_checkany(L, 4);
    try {
        json dflt = lua_to_json(L, 3);
        bool want_reply = lua_toboolean(L, 4);

        std::list<APClient::DataStorageOperation> operations;
        try {
            lua_to_json(L, 5).get_to(operations);
        } catch (const std::exception&) {
            throw BadArgumentException(5, "array of operations", "Set");
        }

        json extras;
        if (lua_gettop(L) >= 6) {
            extras = lua_to_json(L, 6);
        }

        bool res = self->Set(key, dflt, want_reply, operations, extras);
        lua_pushboolean(L, res);
        return 1;
    } catch (const std::exception& ex) {
        lua_pushstring(L, ex.what());
    }
    lua_error(L);
    return 0; // LCOV_EXCL_LINE // unreachable
}

static int apclient_poll(lua_State *L)
{
    // run actual poll via pcall to avoid trashing any state
    lua_pushcfunction(L, LuaAPClient::poll);
    lua_pushvalue(L, -2);
    if (lua_pcall(L, 1, 1, 0)) {
        lua_error(L);
    }
    return 1;
}

// meta table ("class")

#define SET_CFUNC(name) \
    lua_pushcfunction(L, apclient_ ## name); \
    lua_setfield(L, -2, #name);


static int register_apclient(lua_State *L)
{
    // register dependency types
    LuaJson_EmptyArray::Lua_Register(L);

    // register type/metatable for ctor and dtor
    luaL_newmetatable(L, LuaAPClient::Lua_Name);

    // special functions
    lua_pushcfunction(L, apclient_del);
    lua_setfield(L, -2, "__gc");

    lua_pushcfunction(L, LuaAPClient::__index);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, LuaAPClient::__newindex);
    lua_setfield(L, -2, "__newindex");

    // functions
    SET_CFUNC(new);
    SET_CFUNC(poll);
    SET_CFUNC(reset);
    SET_CFUNC(get_player_alias);
    SET_CFUNC(get_player_game);
    SET_CFUNC(get_game);
    SET_CFUNC(get_location_name);
    SET_CFUNC(get_location_id);
    SET_CFUNC(get_item_name);
    SET_CFUNC(get_item_id);
    SET_CFUNC(render_json);
    SET_CFUNC(get_state);
    SET_CFUNC(get_seed);
    SET_CFUNC(get_slot);
    SET_CFUNC(get_player_number);
    SET_CFUNC(get_team_number);
    SET_CFUNC(get_hint_points);
    SET_CFUNC(get_hint_cost_points);
    SET_CFUNC(get_hint_cost_percent);
    SET_CFUNC(is_data_package_valid);
    SET_CFUNC(get_server_time);
    SET_CFUNC(get_players);

    // handlers
    SET_CFUNC(set_socket_connected_handler);
    SET_CFUNC(set_socket_error_handler);
    SET_CFUNC(set_socket_disconnected_handler);
    SET_CFUNC(set_room_info_handler);
    SET_CFUNC(set_slot_connected_handler);
    SET_CFUNC(set_slot_refused_handler);
    SET_CFUNC(set_items_received_handler);
    SET_CFUNC(set_location_info_handler);
    SET_CFUNC(set_location_checked_handler);
    SET_CFUNC(set_data_package_changed_handler);
    SET_CFUNC(set_print_handler);
    SET_CFUNC(set_print_json_handler);
    SET_CFUNC(set_bounced_handler);
    SET_CFUNC(set_retrieved_handler);
    SET_CFUNC(set_set_reply_handler);

    // commands
    SET_CFUNC(Say);
    SET_CFUNC(ConnectSlot);
    SET_CFUNC(ConnectUpdate);
    SET_CFUNC(Sync);
    SET_CFUNC(Bounce);
    SET_CFUNC(StatusUpdate);
    SET_CFUNC(LocationChecks);
    SET_CFUNC(LocationScouts);
    SET_CFUNC(Get);
    SET_CFUNC(SetNotify);
    SET_CFUNC(Set);

    // enums
    json_to_lua(L, {
        {"UNKNOWN", LuaAPClient::ClientStatus::UNKNOWN},
        {"READY", LuaAPClient::ClientStatus::READY},
        {"PLAYING", LuaAPClient::ClientStatus::PLAYING},
        {"GOAL", LuaAPClient::ClientStatus::GOAL},
    });
    lua_setfield(L, -2, "ClientStatus");

    json_to_lua(L, {
        {"TEXT", LuaAPClient::RenderFormat::TEXT},
        {"HTML", LuaAPClient::RenderFormat::HTML},
        {"ANSI", LuaAPClient::RenderFormat::ANSI},
    });
    lua_setfield(L, -2, "RenderFormat");

    json_to_lua(L, {
        {"FLAG_NONE", LuaAPClient::ItemFlags::FLAG_NONE},
        {"FLAG_ADVANCEMENT", LuaAPClient::ItemFlags::FLAG_ADVANCEMENT},
        {"FLAG_NEVER_EXCLUDE", LuaAPClient::ItemFlags::FLAG_NEVER_EXCLUDE},
        {"FLAG_TRAP", LuaAPClient::ItemFlags::FLAG_TRAP},
    });
    lua_setfield(L, -2, "ItemFlags");

    json_to_lua(L, {
        {"DISCONNECTED", LuaAPClient::State::DISCONNECTED},
        {"SOCKET_CONNECTING", LuaAPClient::State::SOCKET_CONNECTING},
        {"SOCKET_CONNECTED", LuaAPClient::State::SOCKET_CONNECTED},
        {"ROOM_INFO", LuaAPClient::State::ROOM_INFO},
        {"SLOT_CONNECTED", LuaAPClient::State::SLOT_CONNECTED},
    });
    lua_setfield(L, -2, "State");

    // pseudo constant to emit empty json array
    LuaJson_EmptyArray().Lua_Push(L);
    lua_setfield(L, -2, "EMPTY_ARRAY");

    // calling the metatable should be the same as new for easy use,
    // so set a metatable for the metatable
    lua_newtable(L);
    lua_pushcfunction(L, apclient_call);
    lua_setfield(L, -2, "__call");
    lua_setmetatable(L, -2);

    // and return it
    return 1;
}

// init

DLL_EXPORT int luaopen_apclientpp(lua_State *L)
{
    // register type
    int res = register_apclient(L);
    if (res != 1) { // LCOV_EXCL_START
        errorf(L, "register_apclient returned %d (expected 1)", res);
        return 0;
    } // LCOV_EXCL_STOP

    // return constructor
    return 1;
}
