# linuxdeployqt for Qt 6

This tool simplifies the deployment of dependencies for Linux applications and enables you to build [AppImages](https://appimage.org) for your app.

> Provides bug fixes and improvements on top of [qt/macdeployqt](https://github.com/qt/qtbase/tree/dev/src/tools/macdeployqt) and [probonopd/linuxdeployqt](https://github.com/probonopd/linuxdeployqt).

## Download
You can download the latest pre-built binary for [**x86_64**](https://github.com/omergoktas/linuxdeployqt/releases/download/latest/linuxdeployqt-x86_64.AppImage) or [**arm64**](https://github.com/omergoktas/linuxdeployqt/releases/download/latest/linuxdeployqt-arm64.AppImage), or you can build your own from the source code using the instructions below:

```bash
# CMAKE_PREFIX_PATH should point to your Qt installation (Qt 6.7.3 or newer);
# patchelf must be installed
git clone https://github.com/omergoktas/linuxdeployqt
cmake -S linuxdeployqt -B build -DCMAKE_PREFIX_PATH=/path/to/qt/6.7.3/gcc_64
cmake --build build --parallel
```

## Usage

A few examples demonstrating different use cases:

```bash
# Either put location of your Qt installation into PATH variable, i.e.:
#     export QT_PATH=/path/to/qt/6.5.0/gcc_64
#     export PATH=$PATH:$QT_PATH/bin:$QT_PATH/lib
# Or use the -qmake option as shown below:

export QMAKE=/path/to/qt/6.5.0/gcc_64/bin/qmake

# 1. Deploy Qt dependencies only (minimal)
./linuxdeployqt-x86_64.AppImage /path/to/your/executable -qmake=$QMAKE

# 2. Deploy everything except essential system libraries (that come with all Linux distributions out of the box).
./linuxdeployqt-x86_64.AppImage /path/to/your/executable -qmake=$QMAKE -bundle-non-qt-libs

# 3. Deploy everything except essential system libraries and build an AppImage.
./linuxdeployqt-x86_64.AppImage /path/to/your/executable -qmake=$QMAKE -appimage

# 4. Build an AppImage at an explicitly selected output path.
./linuxdeployqt-x86_64.AppImage /path/to/your/executable -qmake=$QMAKE -appimage -appimage-output=/path/to/MyApplication.AppImage

# 5. Deploy everything (including essential system libraries)
./linuxdeployqt-x86_64.AppImage /path/to/your/executable -qmake=$QMAKE -bundle-everything
```
## Bundled C++ runtime

A C++ program needs a C++ runtime, `libstdc++.so.6` and `libgcc_s.so.1`, at least as new as the one it was built with. Without it the program stops at startup with an error such as `` version `GLIBCXX_3.4.32' not found ``. Whenever linuxdeployqt bundles non-Qt libraries (`-appimage` or `-bundle-non-qt-libs`), it therefore also bundles the runtime the deployed binaries use on the build machine, so the app runs on systems whose own runtime is older.

The runtime cannot simply go into `usr/lib` with the other libraries. A process can load only one copy of each library, and the system libraries that get loaded into the app, such as GPU drivers (Mesa, NVIDIA), input methods and platform themes, may need the system's own runtime when it is newer than the bundled one. A newer runtime runs everything built for an older one, so the right copy is always the newer of the two, and that can only be decided on the user's machine:

- linuxdeployqt copies the libraries into `usr/optional/libstdc++/` and `usr/optional/libgcc_s/`. No RPATH points there, so nothing loads them by default.
- It also writes `usr/optional/checkrt`, a small program that needs nothing but the C library. At startup `AppRun` runs it, and it compares each bundled library with the system's copy by the highest version the library defines (`GLIBCXX_*` for libstdc++, `GCC_*` for libgcc_s), as read from its ELF version definitions. It finds the system's copy the way the app would, through the dynamic linker.
- `AppRun` puts the directories of the bundled libraries that are newer, or that the system lacks, in front of `LD_LIBRARY_PATH`. When the system's copies are at least as new, nothing changes.

Set `APPIMAGE_CHECKRT_DEBUG=1` when starting the AppImage to see each decision. Pass `-no-bundle-cxx-runtime` to leave the runtime out. An `AppRun` the AppDir already has is kept as it is; linuxdeployqt warns when it does not run `usr/optional/checkrt`, because the bundled runtime is then never used. `checkrt` needs the glibc version that the machine linuxdeployqt was built on requires of every program (2.34 for the pre-built release), which never exceeds what the deployed app itself requires.

Programs the app starts inherit `LD_LIBRARY_PATH`, including a bundled runtime directory. This is harmless, since the bundled runtime is only there when it is newer than the system's, and restoring `SYS_LD_LIBRARY_PATH` as shown below removes it along with the rest.

## Calling external software from within an app image

Authors of an app-imaged software should know that we [modify](https://github.com/omergoktas/linuxdeployqt/blob/master/assets/AppRun) system environment variables to establish a sandbox before calling the app-imaged software. This way the app-imaged software prefers the libraries shipped with the app image over the libraries installed on the end user's system when loading its dependencies. On the other hand, these changes could cause conflicting libraries when calling external software from within the app image. Therefore it is important that the calling software restore the system environment before executing external software via QProcess, etc. All modified environment variables available through a `SYS_<modified_var>`-prefixed name, i.e. `SYS_PATH` for `PATH`. Check out the example code below:

```cpp
auto env = QProcessEnvironment::systemEnvironment();
env.insert("PATH", env.value("SYS_PATH"));
env.insert("LD_LIBRARY_PATH", env.value("SYS_LD_LIBRARY_PATH"));
env.insert("PYTHONPATH", env.value("SYS_PYTHONPATH"));
env.insert("XDG_DATA_DIRS", env.value("SYS_XDG_DATA_DIRS"));
env.insert("PERLLIB", env.value("SYS_PERLLIB"));
env.insert("GSETTINGS_SCHEMA_DIR", env.value("SYS_GSETTINGS_SCHEMA_DIR"));
env.insert("QT_PLUGIN_PATH", env.value("SYS_QT_PLUGIN_PATH"));

QProcess process;
process.setProcessEnvironment(env);
process.start("external_app", arguments);
process.waitForFinished();
```

## Advanced usage

```
Usage: linuxdeployqt <app-binary|desktop file> [options]

Options:
   -always-overwrite        : Copy files even if the target file exists.
   -appimage                : Create an AppImage (implies -bundle-non-qt-libs).
   -appimage-output=<path>  : Write the AppImage to the given path (requires
                              -appimage).
   -bundle-non-qt-libs      : Also bundle non-core, non-Qt libraries.
   -bundle-everything       : Bundle everything including system libraries.
   -exclude-libs=<list>     : List of libraries which should be excluded,
                              separated by comma.
   -ignore-glob=<glob>      : Glob pattern relative to appdir to ignore when
                              searching for libraries.
   -executable=<path>       : Let the given executable use the deployed libraries
                              too
   -executable-dir=<path>   : Let all the executables in the folder (recursive) use
                              the deployed libraries too
   -extra-plugins=<list>    : List of extra plugins which should be deployed,
                              separated by comma.
   -no-bundle-cxx-runtime   : Don't bundle libstdc++ and libgcc_s, which
                              -appimage and -bundle-non-qt-libs add for use
                              when newer than the system's.
   -no-copy-copyright-files : Skip deployment of copyright files.
   -no-plugins              : Skip plugin deployment.
   -no-strip                : Don't run 'strip' on the binaries.
   -no-translations         : Skip deployment of translations.
   -qmake=<path>            : The qmake executable to use.
   -qmldir=<path>           : Scan for QML imports in the given path.
   -qmlimport=<path>        : Add the given path to QML module search locations.
   -show-exclude-libs       : Print exclude libraries list.
   -verbose=<0-3>           : 0 = no output, 1 = error/warning (default),
                              2 = normal, 3 = debug.
   -updateinformation=<update string>        : Embed update information STRING; if zsyncmake is installed, generate zsync file
   -qtlibinfix=<infix>      : Adapt the .so search if your Qt distribution has infix.
   -version                 : Print version statement and exit.

linuxdeployqt takes an application as input and makes it
self-contained by copying in the Qt libraries and plugins that
the application uses.

By default it deploys the Qt instance that qmake on the $PATH points to.
The '-qmake' option can be used to point to the qmake executable
to be used instead.

Plugins related to a Qt library are copied in with the library.
```
