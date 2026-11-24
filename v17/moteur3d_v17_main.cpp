#include "moteur3d_v17_window.h"

QString defaultPath()
{
    const QString app = QCoreApplication::applicationDirPath();
    const QStringList candidates{// The clean package always opens the latest validated V8/V440 scene.
                                 // Explicit CLI scene arguments still override the default.
                                 app + QStringLiteral("/benchmark-civic/scene.json"),
                                 app + QStringLiteral("/benchmark-civic/scene-v440-audit.json"),
                                 app + QStringLiteral("/scenes/v16/benchmark-civic/scene-v4.json"),
                                 QDir::currentPath() + QStringLiteral("/scenes/v16/benchmark-civic/scene-v4.json"),
                                 app + QStringLiteral("/pavillon01/scene.json"),
                                 app + QStringLiteral("/scenes/v16/pavillon01/scene.json"),
                                 QDir::currentPath() + QStringLiteral("/scenes/v16/pavillon01/scene.json")};
    for (const auto &path : candidates)
        if (QFileInfo::exists(path))
            return path;
    return QString();
}

int main(int argc, char **argv)
{
    auto shutdownComplete = std::make_shared<std::atomic_bool>(false);
    const int exitCode = [&] {
        QApplication app(argc, argv);
        app.setQuitOnLastWindowClosed(true);
        // Separate benchmark utilities need access to Qt and the V16 renderer
        // DLL in this installation root, even when launched from a subdirectory.
        const QByteArray installRoot = QCoreApplication::applicationDirPath().toLocal8Bit();
        qputenv("PATH", installRoot + ";" + qgetenv("PATH"));
        qputenv("QT_PLUGIN_PATH", installRoot);
        // GUI animation is intentionally opt-in; offline GPU benchmarks remain
        // deterministic for strict RGB parity and regression comparisons.
        qputenv("M3D_V17_ANIMATE_CLOUDS", "1");
        // Split only interactive raster output into a sky backdrop and an
        // alpha-preserving architecture foreground. Benchmarks stay unchanged.
        qputenv("M3D_V17_SPLIT_SKY", "1");
        QCoreApplication::setApplicationName(QStringLiteral("CSE WARP Beta X V%1").arg(CSE_WARP_PRODUCT_VERSION));
        QCoreApplication::setApplicationVersion(QStringLiteral("%1").arg(CSE_WARP_PRODUCT_VERSION));
        const bool switchSmoke = argc >= 4 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--smoke-scene-switch");
        const bool repeatSmoke =
            argc >= 3 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--smoke-repeat-raster");
        const bool walkingSmoke =
            argc >= 3 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--smoke-walking-collisions");
        const bool scopeSmoke = argc >= 3 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--smoke-scope-mouse");
        const bool immersiveSmoke =
            argc >= 3 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--smoke-immersive-mouse");
        const bool nativeUiSmoke = argc >= 2 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--smoke-native-ui");
        const bool closeSmoke = argc >= 2 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--smoke-close");
        // Use the same Windows V17 brand icon for Qt titlebar/Alt-Tab/taskbar.
        // The ICO is also embedded in the EXE's native Win32 resources.
        const QString iconPath = QCoreApplication::applicationDirPath() + QStringLiteral("/cse_warp_v17.ico");
        if (QFileInfo::exists(iconPath))
            app.setWindowIcon(QIcon(iconPath));
        const QString path =
            nativeUiSmoke ? QString()
                          : (closeSmoke ? (argc > 2 ? QString::fromLocal8Bit(argv[2]) : QString())
                                        : (switchSmoke || repeatSmoke || walkingSmoke || scopeSmoke || immersiveSmoke
                                               ? QString::fromLocal8Bit(argv[2])
                                               : (argc > 1 ? QString::fromLocal8Bit(argv[1]) : defaultPath())));
        // The smoke test uses reduced texture resolution to stay viable on
        // CI's software-only WARP device. Normal V17 keeps full 2048 PBR.
        V17Window window(
            path,
            (switchSmoke || repeatSmoke || walkingSmoke || scopeSmoke || immersiveSmoke || closeSmoke) ? 256 : 2048,
            shutdownComplete);
        if (switchSmoke)
            window.smokeSceneSwitch(QString::fromLocal8Bit(argv[3]));
        if (repeatSmoke)
            window.smokeRepeatRaster();
        if (walkingSmoke)
            window.smokeWalkingCollisionPreference();
        if (scopeSmoke)
            window.smokeScopedAim();
        if (immersiveSmoke)
            window.smokeImmersiveMouse();
        if (nativeUiSmoke)
            window.smokeNativeUi();
        if (closeSmoke)
        {
            const int closeDelay = argc > 3 ? QString::fromLocal8Bit(argv[3]).toInt() : 250;
            QTimer::singleShot(std::max(0, closeDelay), &window, &QWidget::close);
        }
        if (!app.windowIcon().isNull())
            window.setWindowIcon(app.windowIcon());
        window.show();
        return app.exec();
    }();
    shutdownComplete->store(true, std::memory_order_release);
    return exitCode;
}
