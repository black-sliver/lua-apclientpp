extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

//#define APCLIENT_DEBUG // to get debug output
#include <apclient.hpp>
#include "luaglue/luamethod.h"


// IMPORTANT: apclientpp can't be used across threads, so capturing L is kind of ok
// FIXME: xmove would be valid outside of an apclient callback, but this breaks the capture and refs
// TODO: show an error when polling on a different thread


#ifdef _WIN32
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT __attribute__ ((visibility ("default")))
#endif


// exported symbols
extern "C" {
DLL_EXPORT int luaopen_apclientpp(lua_State *L);
}


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

static void _to_json(json& j, const APClient::TextNode& node) {
    j = json{
        {"type", node.type},
        {"color", node.color},
        {"text", node.text},
        {"player", node.player},
        {"flags",  node.flags},
    };
}
static void _to_json(json& j, const std::list<APClient::TextNode>& nodes) {
    j = json::array();
    for (const auto& node: nodes) {
        json jnode;
        _to_json(jnode, node);
        j.push_back(jnode);
    }
}

static void _from_json(const json& j, APClient::TextNode& node) {
    node.type = j.value("type", "");
    node.color = j.value("color", "");
    node.text = j.value("text", "");
    node.player = j.value("player", 0);
    node.flags = j.value("flags", 0U);
}


static void _from_json(const json& j, std::list<APClient::TextNode>& nodes) {
    for (const auto& v: j) {
        APClient::TextNode node;
        _from_json(v, node);
        nodes.push_back(node);
    }
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
        assign_set("checked_locations", get_checked_locations());
        assign_set("missing_locations", get_missing_locations());

        if (slot_connected_cb.valid()) {
            lua_rawgeti(_L, LUA_REGISTRYINDEX, slot_connected_cb.ref);
            json_to_lua(_L, slot_data);
            if (lua_pcall(_L, 1, 0, 0)) {
                cb_err("slot_connected");
            }
        }
    }

    void on_location_checked(const std::list<int64_t>& locations)
    {
        // sync location tables
        add_list("checked_locations", locations);
        assign_set("missing_locations", get_missing_locations());

        if (location_checked_cb.valid()) {
            lua_rawgeti(_L, LUA_REGISTRYINDEX, location_checked_cb.ref);
            json j = locations;
            json_to_lua(_L, j);
            if (lua_pcall(_L, 1, 0, 0)) {
                cb_err("location_checked");
            }
        }
    }

    // lua methods

    void set_socket_connected_handler(LuaRef ref)
    {
        unref(socket_connected_cb);
        socket_connected_cb = ref;

        APClient* parent = this;
        parent->set_socket_connected_handler([this]() {
            lua_rawgeti(_L, LUA_REGISTRYINDEX, socket_connected_cb.ref);
            if (lua_pcall(_L, 0, 0, 0)) {
                cb_err("socket_connected");
            }
        });
    }

    void set_socket_error_handler(LuaRef ref)
    {
        unref(socket_error_cb);
        socket_error_cb = ref;

        APClient* parent = this;
        parent->set_socket_error_handler([this](const std::string& msg) {
            lua_rawgeti(_L, LUA_REGISTRYINDEX, socket_error_cb.ref);
            lua_pushstring(_L, msg.c_str());
            if (lua_pcall(_L, 1, 0, 0)) {
                cb_err("socket_error");
            }
        });
    }

    void set_socket_disconnected_handler(LuaRef ref)
    {
        unref(socket_disconnected_cb);
        socket_disconnected_cb = ref;

        APClient* parent = this;
        parent->set_socket_disconnected_handler([this]() {
            lua_rawgeti(_L, LUA_REGISTRYINDEX, socket_disconnected_cb.ref);
            if (lua_pcall(_L, 0, 0, 0)) {
                cb_err("socket_disconnected");
            }
        });
    }

    void set_room_info_handler(LuaRef ref)
    {
        unref(room_info_cb);
        room_info_cb = ref;

        APClient* parent = this;
        parent->set_room_info_handler([this]() {
            lua_rawgeti(_L, LUA_REGISTRYINDEX, room_info_cb.ref);
            if (lua_pcall(_L, 0, 0, 0)) {
                cb_err("room_info");
            }
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
            lua_rawgeti(_L, LUA_REGISTRYINDEX, slot_refused_cb.ref);
            json j = reason;
            json_to_lua(_L, j);
            if (lua_pcall(_L, 1, 0, 0)) {
                cb_err("slot_refused");
            }
        });
    }

    void set_items_received_handler(LuaRef ref)
    {
        unref(items_received_cb);
        items_received_cb = ref;

        APClient* parent = this;
        parent->set_items_received_handler([this](const std::list<NetworkItem>& items) {
            lua_rawgeti(_L, LUA_REGISTRYINDEX, items_received_cb.ref);
            json j = items;
            json_to_lua(_L, j);
            if (lua_pcall(_L, 1, 0, 0)) {
                cb_err("items_received");
            }
        });
    }

    void set_location_info_handler(LuaRef ref)
    {
        unref(location_info_cb);
        location_info_cb = ref;

        APClient* parent = this;
        parent->set_location_info_handler([this](const std::list<NetworkItem>& items) {
            lua_rawgeti(_L, LUA_REGISTRYINDEX, location_info_cb.ref);
            json j = items;
            json_to_lua(_L, j);
            if (lua_pcall(_L, 1, 0, 0)) {
                cb_err("location_info");
            }
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
            lua_rawgeti(_L, LUA_REGISTRYINDEX, data_package_changed_cb.ref);
            json_to_lua(_L, data_package);
            if (lua_pcall(_L, 1, 0, 0)) {
                cb_err("data_package_changed");
            }
        });
    }

    void set_print_handler(LuaRef ref)
    {
        unref(print_cb);
        print_cb = ref;

        APClient* parent = this;
        parent->set_print_handler([this](const std::string& msg) {
            lua_rawgeti(_L, LUA_REGISTRYINDEX, print_cb.ref);
            lua_pushstring(_L, msg.c_str());
            if (lua_pcall(_L, 1, 0, 0)) {
                cb_err("print");
            }
        });
    }

    void set_print_json_handler(LuaRef ref)
    {
        unref(print_json_cb);
        print_json_cb = ref;

        APClient* parent = this;
        parent->set_print_json_handler([this](const PrintJSONArgs& args) {
            lua_rawgeti(_L, LUA_REGISTRYINDEX, print_json_cb.ref);
            json j;
            _to_json(j, args.data);
            json_to_lua(_L, j);
            if (lua_pcall(_L, 1, 0, 0)) {
                cb_err("print_json");
            }
        });
    }

    void set_bounced_handler(LuaRef ref)
    {
        unref(bounced_cb);
        bounced_cb = ref;

        APClient* parent = this;
        parent->set_bounced_handler([this](const json& bounce) {
            lua_rawgeti(_L, LUA_REGISTRYINDEX, bounced_cb.ref);
            json_to_lua(_L, bounce);
            if (lua_pcall(_L, 1, 0, 0)) {
                cb_err("bounced");
            }
        });
    }

    void set_retrieved_handler(LuaRef ref)
    {
        unref(retrieved_cb);
        retrieved_cb = ref;

        APClient* parent = this;
        parent->set_retrieved_handler([this](const std::map<std::string, json>& data) {
            lua_rawgeti(_L, LUA_REGISTRYINDEX, retrieved_cb.ref);
            json j = data;
            json_to_lua(_L, j);
            if (lua_pcall(_L, 1, 0, 0)) {
                cb_err("retrieved");
            }
        });
    }

    void set_set_reply_handler(LuaRef ref)
    {
        unref(set_reply_cb);
        set_reply_cb = ref;

        APClient* parent = this;
        parent->set_set_reply_handler([this](const json& message) {
            lua_rawgeti(_L, LUA_REGISTRYINDEX, set_reply_cb.ref);
            json_to_lua(_L, message);
            if (lua_pcall(_L, 1, 0, 0)) {
                cb_err("set_reply");
            }
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
        } catch (std::exception ex) {
            fprintf(stderr, "Invalid argument for locations\n");
            return false;
        }
        
        APClient* parent = this;
        if (parent->LocationChecks(locations)) {
            // sync location tables
            add_list("checked_locations", locations);
            assign_set("missing_locations", get_missing_locations());
            return true;
        }

        return false;
    }

    bool Get(const json& j)
    {
        std::list<std::string> keys;
        try {
            keys = j.get<std::list<std::string>>();
        } catch (std::exception ex) {
            fprintf(stderr, "Invalid argument for keys\n");
            return false;
        }
        
        APClient* parent = this;
        return parent->Get(keys);
    }

    bool SetNotify(const json& j)
    {
        std::list<std::string> keys;
        try {
            keys = j.get<std::list<std::string>>();
        } catch (std::exception ex) {
            fprintf(stderr, "Invalid argument for keys\n");
            return false;
        }
        
        APClient* parent = this;
        return parent->SetNotify(keys);
    }

    int get_state() const
    {
        const APClient* parent = this;
        return (int)parent->get_state();
    }

    bool poll()
    {
        try {
            APClient* parent = this;
            parent->poll();
            return true;
        } catch (std::exception) {
            return false;
        }
    }

    // lua interface implementation details

    static constexpr char Lua_Name[] = "APClient";

    static LuaAPClient* luaL_checkthis(lua_State *L, int narg)
    {
        return * (LuaAPClient**)luaL_checkudata(L, narg, LuaAPClient::Lua_Name);
    }

    static int __index(lua_State *L)
    {
        LuaAPClient *self = *(LuaAPClient**)lua_touserdata(L, 1);
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
        LuaAPClient *self = *(LuaAPClient**)lua_touserdata(L, 1);
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
    }

    void cb_err(const std::string& name)
    {
        const char* err = luaL_checkstring(_L, -1);
        fprintf(stderr, "Error calling %s_handler:\n%s\n", name.c_str(), err);
        lua_pop(_L, 1); // pop error
    }

    template <class T>
    void assign_set(const char* key, const std::set<T>& set, int table = -1)
    {
        // get table by name
        lua_getfield(_L, table, key);
        // assign values
        size_t n = 0;
        for (const auto& v: set) {
            lua_pushinteger(_L, (lua_Integer)++n);
            Lua(_L).Push(v);
            lua_rawset(_L, -3);
        }
        // delete old values
        size_t len = luaL_len(_L, -1);
        for (size_t i = n; i <= len; i++) {
            lua_pushinteger(_L, (lua_Integer)i);
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
        size_t n = luaL_len(_L, -1);
        for (const auto& v: lst) {
            if (!contains(v)) {
                lua_pushinteger(_L, (lua_Integer)++n);
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
};

#if __cplusplus < 201500L // c++14 needs a proper declaration
decltype(APClient::DEFAULT_URI) constexpr APClient::DEFAULT_URI;
decltype(LuaAPClient::NAME) constexpr LuaAPClient::NAME;
#endif

// C functions - read above
// NOTE: new, call and del will probably always be C or static

static int apclient_new(lua_State *L)
{
    const char* uuid = luaL_checkstring(L, 1);
    const char* game = luaL_checkstring(L, 2);
    const char* host = luaL_checkstring(L, 3);

    LuaAPClient *self = new LuaAPClient(L, uuid, game, host);

    LuaAPClient **p = (LuaAPClient**)lua_newuserdata(L, sizeof(LuaAPClient*));
    *p = self;
    luaL_setmetatable(L, LuaAPClient::Lua_Name);
    lua_newtable(L);
    lua_setfield(L, -2, "checked_locations");
    lua_newtable(L);
    lua_setfield(L, -2, "missing_locations");
    return 1;
}

static int apclient_call(lua_State *L)
{
    lua_remove(L, 1);
    return apclient_new(L);
}

static int apclient_del(lua_State *L)
{
    LuaAPClient *self = *(LuaAPClient**)lua_touserdata(L, 1);
    delete self;
    return 0;
}

static int apclient_render_json(lua_State *L)
{
    LuaAPClient *self = *(LuaAPClient**)lua_touserdata(L, 1);
    std::list<APClient::TextNode> msg;
    try {
        _from_json(lua_to_json(L, 2), msg);
    } catch (std::exception ex) {
        fprintf(stderr, "Invalid argument for msg");
        return 0;
    }
    APClient::RenderFormat fmt = APClient::RenderFormat::TEXT;
    if (lua_gettop(L) >= 3) {
        fmt = (APClient::RenderFormat)luaL_checkinteger(L, 3);
    }

    std::string res = self->render_json(msg, fmt);
    lua_pushstring(L, res.c_str());
    return 1;
}

static int apclient_ConnectSlot(lua_State *L)
{
    LuaAPClient *self = *(LuaAPClient**)lua_touserdata(L, 1);
    const char* slot = luaL_checkstring(L, 2);
    const char* password = luaL_checkstring(L, 3);
    int items_handling = luaL_checkinteger(L, 4);
    std::list<std::string> tags;
    APClient::Version version = {0, 0, 0};

    if (lua_gettop(L) >= 5) {
        try {
            tags = lua_to_json(L, 5).get<std::list<std::string>>();
        } catch (std::exception ex) {
            fprintf(stderr, "Invalid tags argument\n");
            return 0;
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
        } catch (std::exception ex) {
            fprintf(stderr, "Invalid version argument\n");
            return 0;
        }
    }

    bool res;
    if (version.ma > 0 || version.mi > 0 || version.build > 0)
        res = self->ConnectSlot(slot, password, items_handling, tags, version);
    else
        res = self->ConnectSlot(slot, password, items_handling, tags);

    lua_pushboolean(L, res);
    return 1;
}

static int apclient_ConnectUpdate(lua_State *L)
{
    LuaAPClient *self = *(LuaAPClient**)lua_touserdata(L, 1);

    bool res;
    if (lua_isnil(L, 3)) {
        if (lua_isnil(L, 2)) {
            // invalid arguments
            fprintf(stderr, "Either items_handling or tags required\n");
            return 0;
        } else {
            // update items_handling
            int items_handling = luaL_checkinteger(L, 2);
            self->ConnectUpdate(true, items_handling, false, {});
        }
    } else {
        std::list<std::string> tags;
        try {
            tags = lua_to_json(L, 3).get<std::list<std::string>>();
        } catch (std::exception ex) {
            fprintf(stderr, "Invalid tags argument\n");
            return 0;
        }

        if (lua_isnil(L, 2)) {
            // update tags
            self->ConnectUpdate(false, 0, true, tags);
        } else {
            // update both
            int items_handling = luaL_checkinteger(L, 2);
            self->ConnectUpdate(true, items_handling, true, tags);
        }
    }

    lua_pushboolean(L, res);
    return 1;
}

static int apclient_Bounce(lua_State *L)
{
    LuaAPClient *self = *(LuaAPClient**)lua_touserdata(L, 1);
    json data = lua_to_json(L, 2);
    std::list<std::string> games;
    std::list<int> slots;
    std::list<std::string> tags;

    if (lua_gettop(L) >= 3) {
        try {
            games = lua_to_json(L, 3).get<std::list<std::string>>();
        } catch (std::exception) {
            fprintf(stderr, "Invalid games argument\n");
            return 0;
        }
    }

    if (lua_gettop(L) >= 4) {
        try {
            slots = lua_to_json(L, 4).get<std::list<int>>();
        } catch (std::exception) {
            fprintf(stderr, "Invalid slots argument\n");
            return 0;
        }
    }

    if (lua_gettop(L) >= 5) {
        try {
            tags = lua_to_json(L, 5).get<std::list<std::string>>();
        } catch (std::exception) {
            fprintf(stderr, "Invalid tags argument\n");
            return 0;
        }
    }

    bool res = self->Bounce(data, games, slots, tags);
    lua_pushboolean(L, res);
    return 1;
}

static int apclient_LocationScouts(lua_State *L)
{
    LuaAPClient *self = *(LuaAPClient**)lua_touserdata(L, 1);
    std::list<int64_t> locations;
    try {
        locations = lua_to_json(L, 2).get<std::list<int64_t>>();
    } catch (std::exception ex) {
        fprintf(stderr, "Invalid argument for locations\n");
        return 0;
    }

    bool create_as_hints = false;
    if (lua_gettop(L) >= 3) {
        create_as_hints = lua_toboolean(L, 3);
    }
    
    bool res = self->LocationScouts(locations);
    lua_pushboolean(L, res);
    return 1;
}

static int apclient_Set(lua_State *L)
{
    LuaAPClient *self = *(LuaAPClient**)lua_touserdata(L, 1);
    const char* key = luaL_checkstring(L, 2);
    json dflt = lua_to_json(L, 3);
    luaL_checkany(L, 4);
    bool want_reply = lua_toboolean(L, 4);

    std::list<APClient::DataStorageOperation> operations;
    try {
        lua_to_json(L, 5).get_to(operations);
    } catch (std::exception) {
        fprintf(stderr, "Invalid argument for operations\n");
        return 0;
    }

    json extras;
    if (lua_gettop(L) >= 6) {
        extras = lua_to_json(L, 6);
    }

    try {
        bool res = self->Set(key, dflt, want_reply, operations, extras);
        lua_pushboolean(L, res);
        return 1;
    } catch (std::exception ex) {
        fprintf(stderr, "Set failed: %s\n", ex.what());
        return 0;
    }
}

// meta table ("class")

#define SET_CFUNC(name) \
    lua_pushcfunction(L, apclient_ ## name); \
    lua_setfield(L, -2, #name);


#define SET_CLASS_METHOD(CLASS, F, ARGS...) \
    lua_pushcclosure(L, LuaMethod<CLASS, &CLASS::F, ARGS>::Func, 0); \
    lua_setfield(L, -2, #F);

#define SET_METHOD(F, ARGS...) SET_CLASS_METHOD(LuaAPClient, F, ARGS)


static int register_apclient(lua_State *L)
{
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
    SET_METHOD(poll, void);
    SET_METHOD(reset, void);
    SET_METHOD(get_player_alias, int);
    SET_METHOD(get_location_name, int64_t);
    SET_METHOD(get_location_id, const char*);
    SET_METHOD(get_item_name, int64_t);
    SET_METHOD(get_item_id, const char*);
    SET_CFUNC(render_json);
    SET_METHOD(get_state, void);
    SET_METHOD(get_seed, void);
    SET_METHOD(get_slot, void);
    SET_METHOD(get_player_number, void);
    SET_METHOD(get_team_number, void);
    SET_METHOD(get_hint_points, void);
    SET_METHOD(get_hint_cost_points, void);
    SET_METHOD(get_hint_cost_percent, void);
    SET_METHOD(is_data_package_valid, void);
    SET_METHOD(get_server_time, void);

    // handlers
    SET_METHOD(set_socket_connected_handler, LuaRef);
    SET_METHOD(set_socket_error_handler, LuaRef);
    SET_METHOD(set_socket_disconnected_handler, LuaRef);
    SET_METHOD(set_room_info_handler, LuaRef);
    SET_METHOD(set_slot_connected_handler, LuaRef);
    SET_METHOD(set_slot_refused_handler, LuaRef);
    SET_METHOD(set_items_received_handler, LuaRef);
    SET_METHOD(set_location_info_handler, LuaRef);
    SET_METHOD(set_location_checked_handler, LuaRef);
    SET_METHOD(set_data_package_changed_handler, LuaRef);
    SET_METHOD(set_print_handler, LuaRef);
    SET_METHOD(set_print_json_handler, LuaRef);
    SET_METHOD(set_bounced_handler, LuaRef);
    SET_METHOD(set_retrieved_handler, LuaRef);
    SET_METHOD(set_set_reply_handler, LuaRef);

    // commands
    SET_METHOD(Say, const char*);
    SET_CFUNC(ConnectSlot);
    SET_CFUNC(ConnectUpdate);
    SET_METHOD(Sync, void);
    SET_CFUNC(Bounce);
    SET_METHOD(StatusUpdate, int);
    SET_METHOD(LocationChecks, json);
    SET_CFUNC(LocationScouts);
    SET_METHOD(Get, json);
    SET_METHOD(SetNotify, json);
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
    if (res == 1) {
        // return constructor
        return 1;
    } else {
        fprintf(stderr, "register_apclient returned %d (expected 1)\n", res);
        return 0;
    }
}
