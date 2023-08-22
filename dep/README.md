# lua-apclientpp dep

We store some dependencies for build automation here since there is no easy way to grab these files without a
package manager.

## lua51

Contains files required to build a 32bit MSVC v141 / VS2017 DLL that links Lua5.1.
The included .lib is a stub and part of a MSVC build output. The actual DLL has to be provided by the application.
