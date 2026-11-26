#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
using Clock = std::chrono::steady_clock;
constexpr int width = 1280, height = 720;
constexpr auto routeDuration = std::chrono::seconds(30);
constexpr double durationSeconds = routeDuration.count();
constexpr float solarHour = 14.75f;
constexpr char cameraId[] = "v7_pool_fresco";

enum class Backend { dx11, vulkan, lumaTrace };
struct Pass { Backend backend; const char* name; };
constexpr Pass passes[] = {{Backend::dx11, "DX11"}, {Backend::vulkan, "Vulkan"},
                           {Backend::lumaTrace, "LumaTrace"}};

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM key, LPARAM data) {
    if (message == WM_CLOSE || (message == WM_KEYDOWN && key == VK_ESCAPE)) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, key, data);
}

HWND openRouteWindow(std::wstring& error) {
    WNDCLASSW wc{};
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpfnWndProc = windowProc;
    wc.lpszClassName = L"CseWarpRoute30";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        error = L"RegisterClassW failed: " + std::to_wstring(GetLastError());
        return nullptr;
    }

    const int screenW = GetSystemMetrics(SM_CXSCREEN), screenH = GetSystemMetrics(SM_CYSCREEN);
    const int x = std::max(0, (screenW - width) / 2), y = std::max(0, (screenH - height) / 2);
    HWND window = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, L"CSE WARP - Parcours 720p",
        WS_POPUP, x, y, width, height, nullptr, nullptr, wc.hInstance, nullptr);
    if (!window) {
        error = L"CreateWindowExW failed: " + std::to_wstring(GetLastError());
        return nullptr;
    }
    // The first ShowWindow can inherit size/show flags from the console launcher.
    // Size the borderless client explicitly after showing it.
    ShowWindow(window, SW_SHOWNORMAL);
    if (!SetWindowPos(window, HWND_TOP, x, y, width, height, SWP_SHOWWINDOW | SWP_NOSENDCHANGING)) {
        error = L"SetWindowPos failed: " + std::to_wstring(GetLastError());
        DestroyWindow(window);
        return nullptr;
    }
    UpdateWindow(window);
    RECT client{};
    if (!GetClientRect(window, &client) || client.right != width || client.bottom != height) {
        error = L"Native client is " + std::to_wstring(client.right) + L"x" + std::to_wstring(client.bottom)
              + L" on a " + std::to_wstring(screenW) + L"x" + std::to_wstring(screenH)
              + L" desktop (1280x720 required).";
        DestroyWindow(window);
        return nullptr;
    }
    std::cout << "Native client " << client.right << 'x' << client.bottom
              << " on desktop " << screenW << 'x' << screenH << std::endl;
    return window;
}

bool pollWindow(HWND window) {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return false;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return IsWindowVisible(window) && !IsIconic(window);
}

struct Route {
    const m3d::v16::Runtime& runtime;
    const m3d::v16::CameraPreset& origin;
    const m3d::ShadowMap& shadow;
    HWND window;
    bool prepareGeometry;
};

bool presentFrame(const Route& route, Backend backend, double seconds,
                  m3d::Stats& stats, std::wstring& reason) {
    auto pose = route.origin;
    const float offset = static_cast<float>(2.7 * std::sin(6.283185307179586 * seconds / durationSeconds));
    pose.eye.x += offset;
    pose.target.x += offset;
    const auto camera = m3d::v16::makeCamera(pose, width, height);
    const auto& scene = route.runtime.renderScene;
    bool presented = false;
    switch (backend) {
    case Backend::dx11:
        presented = m3d::renderDirectX11Native(scene, route.shadow, camera, width, height,
                                               route.window, stats, reason);
        break;
    case Backend::vulkan:
        presented = m3d::renderVulkanDirectPresent(scene, route.shadow, camera, width, height,
                                                   route.window, stats, reason);
        break;
    case Backend::lumaTrace:
        presented = m3d::renderLumaTraceNative(scene, route.runtime.lumaTraceBvh4, route.shadow, camera,
                                               width, height, route.window, stats, reason);
        break;
    }
    if (presented && (!stats.gpu || stats.readbackBufferCount != 0)) {
        reason = L"Native GPU frame did not report zero pixel readback buffers.";
        return false;
    }
    if (presented && (stats.geometryPreparationEnabled != route.prepareGeometry ||
        stats.geometryIndexCount != (route.prepareGeometry ? stats.geometrySourceVertices : 0))) {
        reason = L"Native renderer did not apply the requested static geometry mode.";
        return false;
    }
    return presented;
}

struct Result {
    int framesStarted = 0, framesInWindow = 0;
    double elapsed = 0, renderSeconds = 0;
    std::wstring error;
    bool cancelled = false;
    m3d::Stats lastFrame{};
};

Result measure(const Route& route, const Pass& pass) {
    Result result;
    const QString title = QStringLiteral("CSE WARP - %1 - Parcours 1280 x 720").arg(QString::fromLatin1(pass.name));
    SetWindowTextW(route.window, reinterpret_cast<LPCWSTR>(title.utf16()));
    std::cout << pass.name << " warmup" << std::endl;
    auto frame = [&](double t) {
        if (!pollWindow(route.window)) {
            result.cancelled = true;
            result.error = L"Route window closed or minimized.";
            return false;
        }
        if (presentFrame(route, pass.backend, t, result.lastFrame, result.error)) return true;
        if (result.error.empty()) result.error = L"Native presentation failed.";
        return false;
    };
    for (int i = 0; i < 2; ++i) {
        if (!frame(0.0)) return result;
    }
    const auto start = Clock::now();
    const auto deadline = start + routeDuration;
    for (;;) {
        const auto before = Clock::now();
        if (before >= deadline) break;
        ++result.framesStarted;
        const double t = std::chrono::duration<double>(before - start).count();
        if (!frame(t)) return result;
        const auto end = Clock::now();
        result.renderSeconds += std::chrono::duration<double>(end - before).count();
        if (end <= deadline) ++result.framesInWindow;
    }
    result.elapsed = std::chrono::duration<double>(Clock::now() - start).count();
    return result;
}

void writeRow(QTextStream& csv, const Pass& pass, const Result& r, bool prepareGeometry) {
    csv << pass.name << ',' << cameraId << ',' << solarHour << ',' << width << ',' << height << ',';
    if (r.error.empty()) {
        csv << r.framesInWindow << ',' << r.elapsed << ',' << r.framesStarted << ','
            << (1000.0 * r.renderSeconds / r.framesStarted) << ',' << (r.framesInWindow / durationSeconds)
            << ",native_swapchain,off,0,ok,";
        std::cout << pass.name << " " << r.framesInWindow << " presents / 30 s" << std::endl;
    } else {
        QString detail = QString::fromStdWString(r.error);
        detail.replace('"', QStringLiteral("\"\"")).replace('\r', ' ').replace('\n', ' ');
        csv << ",,,,,native_swapchain,unconfirmed,," << (r.cancelled ? "cancelled" : "error") << ",\"" << detail << '"';
        std::cerr << pass.name << ": " << QString::fromStdWString(r.error).toStdString() << std::endl;
    }
    csv << ',' << (prepareGeometry ? "on" : "off");
    if (r.error.empty()) {
        const auto& g = r.lastFrame;
        csv << ',' << (g.geometryPreparationEnabled ? "true" : "false")
            << ',' << g.geometrySourceVertices << ',' << g.geometryUniqueVertices
            << ',' << g.geometryIndexCount;
    } else {
        csv << ",,,,";
    }
    csv << '\n';
    csv.flush();
}
} // namespace

int main(int argc, char** argv) {
    SetProcessDPIAware();  // Physical client pixels on the primary monitor.
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    bool prepareGeometry = true;
    if (args.size() == 5 && args[3] == QStringLiteral("--geometry-prep") &&
        (args[4] == QStringLiteral("on") || args[4] == QStringLiteral("off"))) {
        prepareGeometry = args[4] == QStringLiteral("on");
    } else if (args.size() != 3) {
        std::cerr << "Usage: route30.exe scene.json resultats.csv [--geometry-prep on|off]\n";
        return 2;
    }
    if (_putenv_s("M3D_V17_STATIC_GEOMETRY_PREP", prepareGeometry ? "1" : "0")) return 2;
    // Uncapped benchmark: never silently compare against a VSync-limited Vulkan pass.
    if (_putenv_s("CSE_WARP_DX_BENCH_PRESENT_INTERVAL", "0") ||
        _putenv_s("CSE_WARP_VK_BENCH_PRESENT_MODE", "IMMEDIATE")) return 2;
    auto scene = m3d::v16::loadScene(args[1]);
    if (!scene) { std::cerr << scene.error.toStdString() << std::endl; return 3; }
    auto built = m3d::v16::buildRuntime(scene.scene, 2048);
    if (!built) { std::cerr << built.error.toStdString() << std::endl; return 4; }
    const auto& rt = built.runtime;
    const auto it = std::find_if(rt.cameras.begin(), rt.cameras.end(), [](const auto& c) { return c.id == cameraId; });
    if (it == rt.cameras.end()) { std::cerr << "Missing " << cameraId << std::endl; return 5; }
    m3d::updateLighting(solarHour);
    m3d::setRasterCloudTimeScale(0.f);
    m3d::Stats stats{};
    const auto shadow = m3d::buildShadow(rt.renderScene, stats);
    QFile file(args[2]);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) { std::cerr << file.errorString().toStdString() << std::endl; return 6; }
    QTextStream csv(&file);
    csv << "backend,camera,hour,width,height,frames_completed_30s,elapsed_s,frames_started,mean_render_ms,fps_30s,"
           "presentation,vsync,readback_buffers,status,error,geometry_prep_requested,geometry_prep_enabled,"
           "geometry_source_vertices,geometry_unique_vertices,geometry_index_count\n";
    bool allPassed = true;
    for (const Pass& pass : passes) {
        Result result;
        // Each graphics API owns a dedicated HWND, as in the exploration window.
        HWND window = openRouteWindow(result.error);
        if (window) {
            const Route route{rt, *it, shadow, window, prepareGeometry};
            result = measure(route, pass);
            if (pass.backend == Backend::vulkan) m3d::releaseVulkanDirectForCurrentThread();
            else m3d::releaseDirectX11NativeForCurrentThread();
            DestroyWindow(window);
        }
        writeRow(csv, pass, result, prepareGeometry);
        allPassed = allPassed && result.error.empty();
        if (result.cancelled) break;
    }
    if (csv.status() != QTextStream::Ok || !file.flush()) return 6;
    return allPassed ? 0 : 7;
}
