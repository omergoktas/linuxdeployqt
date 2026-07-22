# linuxdeployqt for Qt 6

This tool simplifies the deployment of dependencies for Linux applications and enables you to build [AppImages](https://appimage.org) for your app.

> Provides bug fixes and improvements on top of [qt/macdeployqt](https://github.com/qt/qtbase/tree/dev/src/tools/macdeployqt) and [probonopd/linuxdeployqt](https://github.com/probonopd/linuxdeployqt).

## Download
You can download the latest pre-built binary from [**here**](https://github.com/omergoktas/linuxdeployqt/releases/download/latest/linuxdeployqt-x86_64.AppImage), or you can build your own from the source code using the instructions below:

```bash
# CMAKE_PREFIX_PATH should point to your Qt installation
git clone https://github.com/omergoktas/linuxdeployqt
cmake -S linuxdeployqt -B build -DCMAKE_PREFIX_PATH=/path/to/qt/6.5.0/gcc_64
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
## Calling external software from within an app image

Authors of an app-imaged software should know that we [modify](https://github.com/omergoktas/linuxdeployqt/blob/dd07ded19e4c4710da37a17eefd11b58e63ac303/deploy/Template.AppDir/AppRun) system environment variables to establish a sandbox before calling the app-imaged software. This way the app-imaged software prefers the libraries shipped with the app image over the libraries installed on the end user's system when loading its dependencies. On the other hand, these changes could cause conflicting libraries when calling external software from within the app image. Therefore it is important that the calling software restore the system environment before executing external software via QProcess, etc. All modified environment variables available through a `SYS_<modified_var>`-prefixed name, i.e. `SYS_PATH` for `PATH`. Check out the example code below:

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
   -version                 : Print version statement and exit.

linuxdeployqt takes an application as input and makes it
self-contained by copying in the Qt libraries and plugins that
the application uses.

By default it deploys the Qt instance that qmake on the $PATH points to.
The '-qmake' option can be used to point to the qmake executable
to be used instead.

Plugins related to a Qt library are copied in with the library.
```
