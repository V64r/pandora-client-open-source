# Swift

## Build

Build the `Release|x64` configuration from Visual Studio or MSBuild.

The final `Swift.dll` and loader are written directly to `bin`. Intermediate
objects and generated build files stay under `bin\trash`. If `Swift.dll`
is loaded by a running process, unload it before rebuilding because Windows
locks loaded modules.
