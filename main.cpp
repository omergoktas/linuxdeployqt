#include "shared.h"

#include <gnu/libc-version.h>
#include <sstream>
#include <stdlib.h>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSettings>

int main(int argc, char* argv[])
{
    // Establish app identity
    QCoreApplication::setApplicationName(APP_NAME);
    QCoreApplication::setApplicationVersion(APP_VERSION);
    QCoreApplication::setOrganizationName(APP_URL);
    QCoreApplication::setOrganizationDomain(APP_URL);

    QCoreApplication app(argc, argv);

    extern QString appBinaryPath;
    appBinaryPath = ""; // Cannot do it in one go due to "extern"

    QString firstArgument = QString::fromLocal8Bit(argv[1]);

    // print version statement
    std::stringstream version;
    version << APP_NAME << " v" APP_VERSION;
    qInfo().noquote() << QString::fromStdString(version.str());

    bool plugins = true;
    bool appimage = false;
    QString appImageOutputPath;
    extern bool runStripEnabled;
    extern bool bundleAllButBlacklistedLibs;
    extern bool bundleEverything;
    extern bool fhsLikeMode;
    extern QString fhsPrefix;
    extern QStringList librarySearchPath;
    extern bool alwaysOwerwriteEnabled;
    QStringList additionalExecutables;
    QStringList additionalExecutablesDir;
    bool qmldirArgumentUsed = false;
    bool skipTranslations = false;
    QStringList qmlDirs;
    QStringList qmlImportPaths;
    QString qmakeExecutable;
    extern QStringList extraQtPlugins;
    extern QStringList ignoreGlob;
    extern bool copyCopyrightFiles;
    extern QString updateInformation;
    extern QString qtLibInfix;

    // Check arguments
    // Due to the structure of the argument parser, we have to check all arguments at first to check whether the user
    // wants to get the version only
    // TODO: replace argument parser with position independent, less error prone version
    for (int i = 0; i < argc; i++) {
        QString argument = argv[i];
        if (argument == "-version" || argument == "-V" || argument == "--version") {
            // can just exit normally, version has been printed above
            return EXIT_SUCCESS;
        }
        if (argument == QByteArray("-show-exclude-libs")) {
            qInfo() << excludedLibraries;
            return EXIT_SUCCESS;
        }
    }
    for (int i = 2; i < argc; ++i) {
        QByteArray argument = QByteArray(argv[i]);

        if (argument == QByteArray("-no-plugins")) {
            LogDebug() << "Argument found:" << argument;
            plugins = false;
        } else if (argument == QByteArray("-appimage")) {
            LogDebug() << "Argument found:" << argument;
            appimage = true;
            bundleAllButBlacklistedLibs = true;
        } else if (argument.startsWith(QByteArray("-appimage-output="))) {
            LogDebug() << "Argument found:" << argument;
            appImageOutputPath = QString::fromLocal8Bit(
                argument.mid(QByteArray("-appimage-output=").size()));
            if (appImageOutputPath.isEmpty()) {
                LogError() << "Missing AppImage output path\n";
                return EXIT_FAILURE;
            }
        } else if (argument == QByteArray("-bundle-everything")) {
            LogDebug() << "Argument found:" << argument;
            bundleEverything = true;
        } else if (argument == QByteArray("-no-strip")) {
            LogDebug() << "Argument found:" << argument;
            runStripEnabled = false;
        } else if (argument == QByteArray("-bundle-non-qt-libs")) {
            LogDebug() << "Argument found:" << argument;
            bundleAllButBlacklistedLibs = true;
        } else if (argument.startsWith(QByteArray("-verbose"))) {
            LogDebug() << "Argument found:" << argument;
            int index = argument.indexOf("=");
            bool ok = false;
            int number = argument.mid(index + 1).toInt(&ok);
            if (!ok)
                LogError() << "Could not parse verbose level";
            else
                logLevel = number;
        } else if (argument.startsWith(QByteArray("-executable-dir"))) {
            LogDebug() << "Argument found:" << argument;
            int index = argument.indexOf('=');
            if (index == -1)
                LogError() << "Missing executable folder path";
            else
                additionalExecutablesDir << argument.mid(index + 1);
        } else if (argument.startsWith(QByteArray("-executable"))) {
            LogDebug() << "Argument found:" << argument;
            int index = argument.indexOf('=');
            if (index == -1)
                LogError() << "Missing executable path";
            else
                additionalExecutables << argument.mid(index + 1);
        } else if (argument.startsWith(QByteArray("-qmldir"))) {
            LogDebug() << "Argument found:" << argument;
            qmldirArgumentUsed = true;
            int index = argument.indexOf('=');
            if (index == -1)
                LogError() << "Missing qml directory path";
            else
                qmlDirs << argument.mid(index + 1);
        } else if (argument.startsWith(QByteArray("-qmlimport"))) {
            LogDebug() << "Argument found:" << argument;
            int index = argument.indexOf('=');
            if (index == -1)
                LogError() << "Missing qml import path";
            else
                qmlImportPaths << argument.mid(index + 1);
        } else if (argument.startsWith("-no-copy-copyright-files")) {
            LogDebug() << "Argument found:" << argument;
            copyCopyrightFiles = false;
        } else if (argument == QByteArray("-always-overwrite")) {
            LogDebug() << "Argument found:" << argument;
            alwaysOwerwriteEnabled = true;
        } else if (argument.startsWith("-qmake=")) {
            LogDebug() << "Argument found:" << argument;
            int index = argument.indexOf("=");
            qmakeExecutable = argument.mid(index + 1);
        } else if (argument == QByteArray("-no-translations")) {
            LogDebug() << "Argument found:" << argument;
            skipTranslations = true;
        } else if (argument.startsWith("-extra-plugins=")) {
            LogDebug() << "Argument found:" << argument;
            int index = argument.indexOf("=");
            extraQtPlugins = QString(argument.mid(index + 1)).split(",");
        } else if (argument.startsWith("-exclude-libs=")) {
            LogDebug() << "Argument found:" << argument;
            int index = argument.indexOf("=");
            excludedLibraries.append(QString(argument.mid(index + 1)).split(","));
        } else if (argument.startsWith("-ignore-glob=")) {
            LogDebug() << "Argument found:" << argument;
            int index = argument.indexOf("=");
            ignoreGlob += argument.mid(index + 1);
        } else if (argument.startsWith("-updateinformation=")) {
            LogDebug() << "Argument found:" << argument;
            int index = argument.indexOf("=");
            updateInformation = QString(argument.mid(index + 1));
        } else if (argument.startsWith("-qtlibinfix=")) {
            LogDebug() << "Argument found:" << argument;
            int index = argument.indexOf("=");
            qtLibInfix = QString(argument.mid(index + 1));
        } else if (argument.startsWith("--")) {
            LogError() << "Error: arguments must not start with --, only -:" << argument
                       << "\n";
            return EXIT_FAILURE;
        } else {
            LogError() << "Unknown argument:" << argument << "\n";
            return EXIT_FAILURE;
        }
    }

    // Warn about use of new glibc
    const char* glcv = gnu_get_libc_version();
    if (strverscmp(glcv, "2.32") >= 0) {
        qInfo() << "WARNING: The host system is too new (glibc > 2.31). To ensure "
                   "compatibility with older systems,";
        qInfo() << "         please consider running this tool on an older mainstream "
                   "Linux distribution. This will";
        qInfo() << "         allow the resulting bundle to work on most Linux "
                   "distributions, including the older ones.";
    }

    if (argc < 2 || (firstArgument.startsWith("-"))) {
        qInfo() << "";
        qInfo() << "Usage:" << APP_NAME << "<app-binary|desktop file> [options]";
        qInfo() << "";
        qInfo() << "Options:";
        qInfo() << "   -always-overwrite        : Copy files even if the target file "
                   "exists.";
        qInfo() << "   -appimage                : Create an AppImage (implies "
                   "-bundle-non-qt-libs).";
        qInfo() << "   -appimage-output=<path>  : Write the AppImage to the given path "
                   "(requires -appimage).";
        qInfo()
            << "   -bundle-non-qt-libs      : Also bundle non-core, non-Qt libraries.";
        qInfo() << "   -bundle-everything       : Bundle everything including system "
                   "libraries.";
        qInfo() << "   -exclude-libs=<list>     : List of libraries which should be "
                   "excluded,";
        qInfo() << "                              separated by comma.";
        qInfo() << "   -ignore-glob=<glob>      : Glob pattern relative to appdir to "
                   "ignore when";
        qInfo() << "                              searching for libraries.";
        qInfo() << "   -executable=<path>       : Let the given executable use the "
                   "deployed libraries";
        qInfo() << "                              too";
        qInfo() << "   -executable-dir=<path>   : Let all the executables in the "
                   "folder (recursive) use";
        qInfo() << "                              the deployed libraries too";
        qInfo() << "   -extra-plugins=<list>    : List of extra plugins which should "
                   "be deployed,";
        qInfo() << "                              separated by comma.";
        qInfo() << "   -no-copy-copyright-files : Skip deployment of copyright files.";
        qInfo() << "   -no-plugins              : Skip plugin deployment.";
        qInfo() << "   -no-strip                : Don't run 'strip' on the binaries.";
        qInfo() << "   -no-translations         : Skip deployment of translations.";
        qInfo() << "   -qmake=<path>            : The qmake executable to use.";
        qInfo()
            << "   -qmldir=<path>           : Scan for QML imports in the given path.";
        qInfo() << "   -qmlimport=<path>        : Add the given path to QML module "
                   "search locations.";
        qInfo() << "   -show-exclude-libs       : Print exclude libraries list.";
        qInfo() << "   -verbose=<0-3>           : 0 = no output, 1 = error/warning "
                   "(default),";
        qInfo() << "                              2 = normal, 3 = debug.";
        qInfo() << "   -updateinformation=<update string>        : Embed update "
                   "information STRING; if zsyncmake is installed, generate zsync file";
        qInfo() << "   -qtlibinfix=<infix>      : Adapt the .so search if your Qt "
                   "distribution has infix.";
        qInfo() << "   -version                 : Print version statement and exit.";
        qInfo() << "";
        qInfo() << APP_NAME << "takes an application as input and makes it";
        qInfo() << "self-contained by copying in the Qt libraries and plugins that";
        qInfo() << "the application uses.";
        qInfo() << "";
        qInfo() << "By default it deploys the Qt instance that qmake on the $PATH "
                   "points to.";
        qInfo() << "The '-qmake' option can be used to point to the qmake executable";
        qInfo() << "to be used instead.";
        qInfo() << "";
        qInfo() << "Plugins related to a Qt library are copied in with the library.";
        /* TODO: To be implemented
        qDebug() << "The accessibility, image formats, and text codec";
        qDebug() << "plugins are always copied, unless \"-no-plugins\" is specified.";
        */
        qInfo() << "";
        qInfo() << "See the \"Deploying Applications on Linux\" topic in the";
        qInfo() << "documentation for more information about deployment on Linux.";

        return EXIT_FAILURE;
    }

    if (!appImageOutputPath.isEmpty() && !appimage) {
        LogError() << "-appimage-output requires -appimage\n";
        return EXIT_FAILURE;
    }

    QString desktopFile = "";
    QString desktopExecEntry = "";
    QString desktopIconEntry = "";

    if (argc > 2) {
        /* If we got a desktop file as the argument, try to figure out the application binary from it.
         * This has the advantage that we can also figure out the icon file this way, and have less work
         * to do when using linuxdeployqt. */
        if (firstArgument.endsWith(".desktop")) {
            qDebug() << "Desktop file as first argument:" << firstArgument;

            /* Check if the desktop file really exists */
            if (!QFile::exists(firstArgument)) {
                LogError() << "Desktop file in first argument does not exist!";
                return EXIT_FAILURE;
            }
            QSettings* settings = 0;
            settings = new QSettings(firstArgument, QSettings::IniFormat);
            desktopExecEntry = settings->value("Desktop Entry/Exec", "r")
                                   .toString()
                                   .split(' ')
                                   .first()
                                   .split('/')
                                   .last()
                                   .trimmed();
            qDebug() << "desktopExecEntry:" << desktopExecEntry;
            desktopFile = firstArgument;
            desktopIconEntry = settings->value("Desktop Entry/Icon", "r")
                                   .toString()
                                   .split(' ')
                                   .first();
            qDebug() << "desktopIconEntry:" << desktopIconEntry;

            QString candidateBin = QDir::cleanPath(
                QFileInfo(firstArgument).absolutePath()
                + desktopExecEntry); // Not FHS-like

            /* Search directory for an executable with the name in the Exec= key */
            QString directoryToBeSearched;
            directoryToBeSearched = QDir::cleanPath(
                QFileInfo(firstArgument).absolutePath());

            QDirIterator it(directoryToBeSearched, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                if ((it.fileName() == desktopExecEntry) && (it.fileInfo().isFile())
                    && (it.fileInfo().isExecutable())) {
                    qDebug() << "Found binary from desktop file:"
                             << it.fileInfo().canonicalFilePath();
                    appBinaryPath = it.fileInfo().absoluteFilePath();
                    break;
                }
            }

            /* Only if we could not find it below the directory in which the desktop file resides, search above */
            if (appBinaryPath == "") {
                if (QFile::exists(
                        QDir::cleanPath(QFileInfo(firstArgument).absolutePath()
                                        + "/../../bin/" + desktopExecEntry))) {
                    directoryToBeSearched = QDir::cleanPath(
                        QFileInfo(firstArgument).absolutePath() + "/../../");
                } else {
                    directoryToBeSearched = QDir::cleanPath(
                        QFileInfo(firstArgument).absolutePath() + "/../");
                }
                QDirIterator it2(directoryToBeSearched, QDirIterator::Subdirectories);
                while (it2.hasNext()) {
                    it2.next();
                    if ((it2.fileName() == desktopExecEntry)
                        && (it2.fileInfo().isFile())
                        && (it2.fileInfo().isExecutable())) {
                        qDebug() << "Found binary from desktop file:"
                                 << it2.fileInfo().canonicalFilePath();
                        appBinaryPath = it2.fileInfo().absoluteFilePath();
                        break;
                    }
                }
            }

            if (appBinaryPath == "") {
                if ((QFileInfo(candidateBin).isFile())
                    && (QFileInfo(candidateBin).isExecutable())) {
                    appBinaryPath = QFileInfo(candidateBin).absoluteFilePath();
                } else {
                    LogError() << "Could not determine the path to the executable "
                                  "based on the desktop file\n";
                    return EXIT_FAILURE;
                }
            }

        } else {
            appBinaryPath = firstArgument;
            appBinaryPath = QFileInfo(QDir::cleanPath(appBinaryPath)).absoluteFilePath();
        }
    }

    // Allow binaries next to linuxdeployqt to be found; this is useful for bundling
    // this application itself together with helper binaries such as patchelf
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString oldPath = env.value("PATH");
    QString newPath = QCoreApplication::applicationDirPath() + ":" + oldPath;
    LogDebug() << newPath;
    setenv("PATH", newPath.toUtf8().constData(), 1);

    QString appName = QDir::cleanPath(QFileInfo(appBinaryPath).fileName());

    QString appDir = QDir::cleanPath(appBinaryPath + "/../");
    if (QDir().exists(appDir) == false) {
        qDebug() << "Error: Could not find AppDir" << appDir;
        return EXIT_FAILURE;
    }

    /* FHS-like mode is for an application that has been installed to a $PREFIX which is otherwise empty, e.g., /path/to/usr.
     * In this case, we want to construct an AppDir in /path/to. */
    if (QDir().exists((QDir::cleanPath(appBinaryPath + "/../../bin"))) == true) {
        fhsPrefix = QDir::cleanPath(appBinaryPath + "/../../");
        qDebug() << "FHS-like mode with PREFIX, fhsPrefix:" << fhsPrefix;
        fhsLikeMode = true;
    } else {
        qDebug() << "Not using FHS-like mode";
    }

    if (QDir().exists(appBinaryPath)) {
        qDebug() << "app-binary:" << appBinaryPath;
    } else {
        LogError() << "Error: Could not find app-binary" << appBinaryPath;
        return EXIT_FAILURE;
    }

    QString appDirPath;
    QString relativeBinPath;
    if (fhsLikeMode == false) {
        appDirPath = appDir;
        relativeBinPath = appName;
    } else {
        appDirPath = QDir::cleanPath(fhsPrefix + "/../");
        QString relativePrefix = fhsPrefix.replace(appDirPath + "/", "");
        relativeBinPath = relativePrefix + "/bin/" + appName;
    }
    if (appDirPath == "/") {
        LogError() << "'/' is not a valid AppDir. Please refer to the documentation.";
        LogError() << "Consider adding INSTALL_ROOT or DESTDIR to your install steps.";
        return EXIT_FAILURE;
    }
    qDebug() << "appDirPath:" << appDirPath;
    qDebug() << "relativeBinPath:" << relativeBinPath;

    QFile appRun(appDirPath + "/AppRun");
    if (appRun.exists()) {
        qDebug() << "Keeping existing AppRun";
    } else {
        if (QFile::copy(":/assets/AppRun", appDirPath + "/AppRun")) {
            if (!QFile::setPermissions(appDirPath + "/AppRun",
                                       QFile::ReadOwner | QFile::ReadGroup
                                           | QFile::ReadOther | QFile::ExeOwner
                                           | QFile::ExeGroup | QFile::WriteOwner
                                           | QFile::WriteGroup)) {
                LogError() << "Could not set permissions on AppRun";
            }
        } else {
            LogError() << "Could not copy AppRun";
        }
    }

    /* Copy the desktop file in place, into the top level of the AppDir */
    if (desktopFile != "") {
        QString destination = QDir::cleanPath(appDirPath + "/"
                                              + QFileInfo(desktopFile).fileName());
        if (QFile::exists(destination) == false) {
            if (QFile::copy(desktopFile, destination)) {
                qDebug() << "Copied" << desktopFile << "to" << destination;
            }
        }
        if (QFileInfo(destination).isFile() == false) {
            LogError() << destination
                       << "does not exist and could not be copied there\n";
            return EXIT_FAILURE;
        }
    }

    /* To make an AppDir, we need to find the icon and copy it in place */
    QStringList candidates;
    QString iconToBeUsed = "";
    if (desktopIconEntry != "") {
        QDirIterator it3(appDirPath, QDirIterator::Subdirectories);
        while (it3.hasNext()) {
            it3.next();
            if ((it3.fileName().startsWith(desktopIconEntry))
                && ((it3.fileName().endsWith(".png"))
                    || (it3.fileName().endsWith(".svg"))
                    || (it3.fileName().endsWith(".svgz"))
                    || (it3.fileName().endsWith(".xpm")))) {
                candidates.append(it3.filePath());
            }
        }
        qDebug() << "Found icons from desktop file:" << candidates;

        /* Select the main icon from the candidates */
        if (candidates.length() == 1) {
            iconToBeUsed = candidates.at(0); // The only choice
        } else if (candidates.length() > 1) {
            const QStringList iconPriorities{"256",
                                             "128",
                                             "svg",
                                             "svgz",
                                             "512",
                                             "1024",
                                             "64",
                                             "48",
                                             "xpm"};
            foreach (const QString& iconPriority, iconPriorities) {
                const auto filteredCandidates = candidates.filter(iconPriority);
                if (!filteredCandidates.isEmpty()) {
                    iconToBeUsed = filteredCandidates.first();
                    break;
                }
            }
        }

        /* Copy in place */
        if (iconToBeUsed != "") {
            /* Check if there is already an icon and only if there is not, copy it to the AppDir.
             * As per the ROX AppDir spec, also copying to .DirIcon. */
            QString preExistingToplevelIcon = "";
            if (QFile::exists(appDirPath + "/" + desktopIconEntry + ".xpm") == true) {
                preExistingToplevelIcon = appDirPath + "/" + desktopIconEntry + ".xpm";
                if (QFile::exists(appDirPath + "/.DirIcon") == false)
                    QFile::copy(preExistingToplevelIcon, appDirPath + "/.DirIcon");
            }
            if (QFile::exists(appDirPath + "/" + desktopIconEntry + ".svgz") == true) {
                preExistingToplevelIcon = appDirPath + "/" + desktopIconEntry + ".svgz";
                if (QFile::exists(appDirPath + "/.DirIcon") == false)
                    QFile::copy(preExistingToplevelIcon, appDirPath + "/.DirIcon");
            }
            if (QFile::exists(appDirPath + "/" + desktopIconEntry + ".svg") == true) {
                preExistingToplevelIcon = appDirPath + "/" + desktopIconEntry + ".svg";
                if (QFile::exists(appDirPath + "/.DirIcon") == false)
                    QFile::copy(preExistingToplevelIcon, appDirPath + "/.DirIcon");
            }
            if (QFile::exists(appDirPath + "/" + desktopIconEntry + ".png") == true) {
                preExistingToplevelIcon = appDirPath + "/" + desktopIconEntry + ".png";
                if (QFile::exists(appDirPath + "/.DirIcon") == false)
                    QFile::copy(preExistingToplevelIcon, appDirPath + "/.DirIcon");
            }

            if (preExistingToplevelIcon != "") {
                qDebug() << "preExistingToplevelIcon:" << preExistingToplevelIcon;
            } else {
                qDebug() << "iconToBeUsed:" << iconToBeUsed;
                QString targetIconPath = appDirPath + "/"
                                         + QFileInfo(iconToBeUsed).fileName();
                if (QFile::copy(iconToBeUsed, targetIconPath)) {
                    qDebug() << "Copied" << iconToBeUsed << "to" << targetIconPath;
                    QFile::copy(targetIconPath, appDirPath + "/.DirIcon");
                } else {
                    LogError() << "Could not copy" << iconToBeUsed << "to"
                               << targetIconPath << "\n";
                    exit(1);
                }
            }
        }
    }

    if (appimage) {
        if (checkAppImagePrerequisites(appDirPath) == false) {
            LogError() << "checkAppImagePrerequisites failed\n";
            return EXIT_FAILURE;
        }
    }

    // recurse folders for additional executables
    for (const auto& folder : additionalExecutablesDir) {
        QString directoryToBeSearched = QDir::cleanPath(
            QFileInfo(folder).absolutePath());
        QDirIterator it(directoryToBeSearched, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            if ((it.fileInfo().isFile()) && (it.fileInfo().isExecutable())) {
                qDebug() << "Found additional executable:"
                         << it.fileInfo().canonicalFilePath();
                additionalExecutables << it.fileInfo().absoluteFilePath();
            }
        }
    }

    DeploymentInfo deploymentInfo = deployQtLibraries(appDirPath,
                                                      additionalExecutables,
                                                      qmakeExecutable);

    // Convenience: Look for .qml files in the current directoty if no -qmldir specified.
    if (qmlDirs.isEmpty()) {
        QDir dir;
        if (!dir.entryList(QStringList() << QStringLiteral("*.qml")).isEmpty()) {
            qmlDirs += QStringLiteral(".");
        }
    }

    if (!qmlDirs.isEmpty()) {
        bool ok = deployQmlImports(appDirPath, deploymentInfo, qmlDirs, qmlImportPaths);
        if (!ok && qmldirArgumentUsed)
            return EXIT_FAILURE; // exit if the user explicitly asked for qml import deployment
        // Update deploymentInfo.deployedLibraries - the QML imports
        // may have brought in extra libraries as dependencies.
        deploymentInfo.deployedLibraries += findAppLibraries(appDirPath);
        deploymentInfo.deployedLibraries.removeDuplicates();
    }

    deploymentInfo.usedModulesMask = 0;
    findUsedModules(deploymentInfo);

    if (plugins && !deploymentInfo.qtPath.isEmpty()) {
        if (deploymentInfo.pluginPath.isEmpty())
            deploymentInfo.pluginPath = QDir::cleanPath(deploymentInfo.qtPath
                                                        + "/../plugins");
        deployPlugins(appDirPath, deploymentInfo);
        createQtConf(appDirPath);
    }

    if (runStripEnabled)
        stripAppBinary(appDirPath);

    if (!skipTranslations) {
        deployTranslations(appDirPath, deploymentInfo.usedModulesMask);
    }

    if (appimage) {
        int result = createAppImage(appDirPath, appImageOutputPath);
        LogDebug() << "result:" << result;
        exit(result);
    }
    exit(0);
}
