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
    If you see "libgcc_*.dll" it's a mingw (msvcrXX.dll) or ucrt build (msvcrt.dll).
    If you see "msvcrXX.dll", we currently don't have builds for that, but you can still try the clang build.
    If you see "msvcrt.dll" or "api-ms-*.dll", the clang build should work
  * On Linux and Mac: you can check with ldd - likely to just work as long as the build machine/target is old enough
* If the DLL of the host application is not named lua5x.dll / liblua.so.5.x / liblua.5.x.dylib:
  * On Windows: the lua-apclient.dll will have to be modified to link to the correct name, see [here](#changing-target-dll-name).
  * On Linux: the dynamic builds should not be tied to a specific .so, but resolve symbols from the application
  * On Mac: either is possible. Need to investigate when we get there.
* If there is no DLL at all:
  * On Windows: you can try to add a Lua DLL or use a static build, but this is likely to crash
  * On Linux: the dynamic build should resolve the symbols from the application if libc matches
  * On Mac: see above


## Changing Target DLL name

### Windows

This requires stripping the DLL (which is currently not done for the automated builds).

```bash
pip install machomachomangler mingw_ldd
strip lua-apclientpp.dll  # strip the build (i686-w64-mingw32-strip or whatever)
mv lua-apclientpp.dll _lua-apclientpp.dll # rename the original
# replace lua53.dll and lua53.3r.dll with the original and correct names below
python -m machomachomangler.cmd.redll _lua-apclientpp.dll lua-apclientpp.dll lua53.dll Lua5.3.3r.dll
python -m mingw_ldd lua-apclientpp.dll --dll-lookup-dirs . # check the result, or use Dependency Walker
```

### Linux

Build without linking to a specific lib - the dynamic linker will then resolve symbols from the executable.

### Mac

Either build without linking to a specific lib and use `-Wl,-undefined,dynamic_lookup`
or use `install_name_tool` to change linkage after the build.


## API

See [sample.lua](https://github.com/black-sliver/lua-apclientpp/blob/main/sample.lua).
Follows the API of [apclientpp](https://github.com/black-sliver/apclientpp).


## Downloads

Until there is a proper release, you can use the downloads from the latest build in the "Actions" tab.

Not all possible variations are being built yet.
