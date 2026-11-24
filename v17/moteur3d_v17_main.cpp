#include "moteur3d_v17_window.h"
#include <algorithm>
#include <array>
#include <string_view>

namespace
{
enum class StartupMode
{
    Normal,
    SceneSwitch,
    RepeatRaster,
    WalkingCollisions,
    ScopeMouse,
    ImmersiveMouse,
    NativeUi,
    Close
};

struct StartupOption
{
    std::string_view name;
    StartupMode mode;
};

constexpr std::array<StartupOption, 7> startupOptions{{
    {"--smoke-scene-switch", StartupMode::SceneSwitch},
    {"--smoke-repeat-raster", StartupMode::RepeatRaster},
    {"--smoke-walking-collisions", StartupMode::WalkingCollisions},
    {"--smoke-scope-mouse", StartupMode::ScopeMouse},
    {"--smoke-immersive-mouse", StartupMode::ImmersiveMouse},
    {"--smoke-native-ui", StartupMode::NativeUi},
    {"--smoke-close", StartupMode::Close},
}};

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
    return {};
}

StartupMode startupModeFromArguments(int argc, char **argv)
{
    if (argc < 2)
        return StartupMode::Normal;

    const std::string_view option(argv[1]);
    const auto found = std::find_if(startupOptions.begin(), startupOptions.end(),
                                    [option](const StartupOption &candidate) { return candidate.name == option; });
    if (found == startupOptions.end())
        return StartupMode::Normal;

    switch (found->mode)
    {
    case StartupMode::SceneSwitch:
        return argc >= 4 ? found->mode : StartupMode::Normal;
    case StartupMode::RepeatRaster:
    case StartupMode::WalkingCollisions:
    case StartupMode::ScopeMouse:
    case StartupMode::ImmersiveMouse:
        return argc >= 3 ? found->mode : StartupMode::Normal;
    case StartupMode::NativeUi:
    case StartupMode::Close:
        return found->mode;
    case StartupMode::Normal:
        return StartupMode::Normal;
    }
    return StartupMode::Normal;
}
} // namespace

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
        // GUI animation is intentionally opt-in; offline GPU benchmarks remain deterministic for strict RGB parity and
        // regression comparisons.
        qputenv("M3D_V17_ANIMATE_CLOUDS", "1");
        // Split only interactive raster output into a sky backdrop and an alpha-preserving architecture foreground.
        // Benchmarks stay unchanged.
        qputenv("M3D_V17_SPLIT_SKY", "1");
        QCoreApplication::setApplicationName(QStringLiteral("CSE WARP Beta X V%1").arg(CSE_WARP_PRODUCT_VERSION));
        QCoreApplication::setApplicationVersion(QStringLiteral("%1").arg(CSE_WARP_PRODUCT_VERSION));
        const StartupMode mode = startupModeFromArguments(argc, argv);
        // Use the same Windows V17 brand icon for Qt titlebar/Alt-Tab/taskbar.
        // The ICO is also embedded in the EXE's native Win32 resources.
        const QString iconPath = QCoreApplication::applicationDirPath() + QStringLiteral("/cse_warp_v17.ico");
        if (QFileInfo::exists(iconPath))
            app.setWindowIcon(QIcon(iconPath));
        QString path;
        int textureResolution = 2048;
        switch (mode)
        {
        case StartupMode::Normal:
            path = argc > 1 ? QString::fromLocal8Bit(argv[1]) : defaultPath();
            break;
        case StartupMode::SceneSwitch:
        case StartupMode::RepeatRaster:
        case StartupMode::WalkingCollisions:
        case StartupMode::ScopeMouse:
        case StartupMode::ImmersiveMouse:
            path = QString::fromLocal8Bit(argv[2]);
            textureResolution = 256;
            break;
        case StartupMode::NativeUi:
            break;
        case StartupMode::Close:
            if (argc > 2)
                path = QString::fromLocal8Bit(argv[2]);
            textureResolution = 256;
            break;
        }
        // The smoke test uses reduced texture resolution to stay viable on
        // CI's software-only WARP device. Normal V17 keeps full 2048 PBR.
        V17Window window(path, textureResolution, shutdownComplete);
        switch (mode)
        {
        case StartupMode::SceneSwitch:
            window.smokeSceneSwitch(QString::fromLocal8Bit(argv[3]));
            break;
        case StartupMode::RepeatRaster:
            window.smokeRepeatRaster();
            break;
        case StartupMode::WalkingCollisions:
            window.smokeWalkingCollisionPreference();
            break;
        case StartupMode::ScopeMouse:
            window.smokeScopedAim();
            break;
        case StartupMode::ImmersiveMouse:
            window.smokeImmersiveMouse();
            break;
        case StartupMode::NativeUi:
            window.smokeNativeUi();
            break;
        case StartupMode::Close: {
            const int closeDelay = argc > 3 ? QString::fromLocal8Bit(argv[3]).toInt() : 250;
            QTimer::singleShot(std::max(0, closeDelay), &window, &QWidget::close);
            break;
        }
        case StartupMode::Normal:
            break;
        }
        if (!app.windowIcon().isNull())
            window.setWindowIcon(app.windowIcon());
        window.show();
        return app.exec();
    }();
    shutdownComplete->store(true, std::memory_order_release);
    return exitCode;
}
