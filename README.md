# Lua-APClientPP

This is a native ("C") Lua wrapper for the C++ Archipelago Client Lib
[apclientpp](https://github.com/black-sliver/apclientpp),
allowing to connect to an [Archipelago](https://archipelago.gg/) server with native performance and SSL support.


## Which Version to Pick

* The architecture has to match (32bit or 64bit).
* The Lua version should match (5.1, 5.2, 5.3 or 5.4).
* In almost all circumstances you should pick a dynamic build, not a static build.
* The libc/toolchain has to match more or less.
  * On Windows: you can check with Dependency Walker or `python -m mingw_ldd path/to/exe --dll-lookup-dirs .`.
    * If you see `libgcc_*.dll` it's a mingw (`msvcrXX.dll`) or ucrt build (`msvcrt.dll`).
    * If you see `msvcrXX.dll` but no libgcc, you can try msvc (vs20xx) builds if available, clang otherwise.
    * If you see `msvcrt.dll` or `api-ms-*.dll`, the clang build should work.
  * On Linux and Mac: you can check with ldd - likely to just work as long as the build machine/target is old enough
* If the DLL of the host application is not named lua5x.dll / liblua.so.5.x / liblua.5.x.dylib:
  * On Windows: the lua-apclient.dll will have to be modified to link to the correct name,
    see [here](#changing-target-dll-name).
  * On Linux: the dynamic builds should not be tied to a specific .so, but resolve symbols from the application,
    so no futher action should be required.
  * On Mac: either is possible. Need to investigate when we get there.
* If there is no DLL at all:
  * On Windows: you can try to add a Lua DLL or use a static build, but this is likely to crash.
  * On Linux: the dynamic build should resolve the symbols from the application if libc matches
  * On Mac: see above


## Changing Target DLL name

The lua.dll used by the EXE might have a different filename than what lua-apclientpp was linked to.
If this is the case and lua-apclientpp actually references the filename (see below), you will have to fix the reference.
This is easy to automate, so some fixed up versions might exist in the automated builds.

### Windows

Dynamic Windows builds always link to a specific DLL by filename.
Changing it may require stripping the DLL, which is done for the automated builds already.

```bash
# strip lua-apclientpp.dll  # strip the build (i686-w64-mingw32-strip or whatever)
pip install machomachomangler mingw_ldd
mv lua-apclientpp.dll _lua-apclientpp.dll # rename the original
# replace lua53.dll and lua53.3r.dll with the original and correct names below
python -m machomachomangler.cmd.redll _lua-apclientpp.dll lua-apclientpp.dll lua53.dll Lua5.3.3r.dll
python -m mingw_ldd lua-apclientpp.dll --dll-lookup-dirs .  # check the result, or use Dependency Walker
```

### Linux

Build without linking to a specific lib - the dynamic linker will then resolve symbols from the executable.

### Mac

Either build without linking to a specific lib and use `-Wl,-undefined,dynamic_lookup`
or use `install_name_tool` to change linkage after the build.


## API

See [sample.lua](https://github.com/black-sliver/lua-apclientpp/blob/main/samples/sample.lua)
and [other samples](https://github.com/black-sliver/lua-apclientpp/tree/main/samples).
Follows the API of [apclientpp](https://github.com/black-sliver/apclientpp).

### Lua-specific API

Due to limitations in Lua, some calls or callbacks may be different. Read below and check the samples.

* `set_retrieved_handler -> on_retrieved(map, keys, extra)`
  * map: repsonse dict key -> value; nil values will be missing from pairs(map)
  * keys: array of all keys including nil values
  * extra: extra data sent during Get

* `AP.EMPTY_ARRAY` use this to send an empty array in json since `{}` will be an empty json object

### Handling Connection Failures

Same as with apclientpp, it will try to reconnect and in case of automatic protocol detection (SSL or plain), a socket
error might be generated even though the connection might succeed on the second attempt.

If a game needs to be connected at all times, not receiving SlotConnected within e.g. 10 seconds would be the indicator
of a failed connect / connect timeout. Receiving a disconnect or error after being connected would be a lost connection.

## To-Do

* Full build matrix
  * Linux - Ubuntu 20.04 might be fine and has static libssl? Otherwise alma container.
  * Mac - using brew? Brew's libs target somewhat recent macos.
  * MSVC builds - currently there is only a 32bit lua5.1 build
* Bundle CA certs
* Tests
* UUID helper - currently uuid is not being used, so you can just pass in an empty string


## Downloads

Until there is a proper
[release](https://github.com/black-sliver/lua-apclientpp/releases),
you can use the downloads from the latest build in the
[Actions tab](https://github.com/black-sliver/lua-apclientpp/actions).

Not all possible variations are being built yet.
