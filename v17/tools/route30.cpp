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
#include <vector>
#include <iterator>

namespace {
using Clock = std::chrono::steady_clock;
constexpr int width = 1280, height = 720;
constexpr auto routeDuration = std::chrono::seconds(30);
constexpr double durationSeconds = routeDuration.count();
constexpr float solarHour = 14.75f;
constexpr char cameraId[] = "v7_pool_fresco";

enum class Backend { dx11, vulkan, lumaTrace };
struct Pass { Backend backend; const char* name; bool adaptive=false; int profile=0; int repetition=0; };
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
    const std::vector<m3d::ShadowMap>* transitionShadows=nullptr;
    bool adaptiveCampaign=false;
    bool adaptive=false;
    mutable size_t lastPhase=~size_t(0);
};

bool presentFrame(const Route& route, Backend backend, double seconds,
                  m3d::Stats& stats, std::wstring& reason) {
    auto pose = route.origin;
    const float offset = static_cast<float>(2.7 * std::sin(6.283185307179586 * seconds / durationSeconds));
    pose.eye.x += offset;
    pose.target.x += offset;
    const auto camera = m3d::v16::makeCamera(pose, width, height);
    const auto& scene = route.runtime.renderScene;
    const m3d::ShadowMap* activeShadow=&route.shadow;
    if(route.transitionShadows){
        const size_t phase=std::min<size_t>(5,size_t(seconds/5.0));
        if(route.lastPhase!=phase){m3d::updateLighting(17.f+phase*.8f);route.lastPhase=phase;}
        activeShadow=&(*route.transitionShadows)[phase];
    }
    bool presented = false;
    switch (backend) {
    case Backend::dx11:
        presented = m3d::renderDirectX11Native(scene, *activeShadow, camera, width, height,
                                               route.window, stats, reason);
        break;
    case Backend::vulkan:
        presented = m3d::renderVulkanDirectPresent(scene, *activeShadow, camera, width, height,
                                                   route.window, stats, reason);
        break;
    case Backend::lumaTrace:
        presented = m3d::renderLumaTraceNative(scene, route.runtime.lumaTraceBvh4, *activeShadow, camera,
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
    if(presented&&backend==Backend::lumaTrace&&(!stats.planarReflectionRendered||!stats.lumaProbeLightingEnabled)){
        reason=L"LumaTrace pool benchmark requires planar reflection and indirect probe lighting.";return false;
    }
    if(presented&&route.adaptiveCampaign&&stats.lumaAdaptiveEnabled!=route.adaptive){reason=L"Adaptive 0/1 mode mismatch.";return false;}
    return presented;
}

struct Result {
    int framesStarted = 0, framesInWindow = 0;
    double elapsed = 0, renderSeconds = 0;
    std::wstring error;
    bool cancelled = false;
    m3d::Stats lastFrame{};
    std::vector<double> frameMs;
    uint64_t zeroSum=0,triangleSum=0;
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
        const double seconds=std::chrono::duration<double>(end - before).count();
        result.renderSeconds += seconds;
        if(route.adaptiveCampaign){result.frameMs.push_back(seconds*1000.);result.zeroSum+=result.lastFrame.lumaAdaptiveZeroTriangles;result.triangleSum+=result.lastFrame.lumaAdaptiveTriangleCount;}
        if (end <= deadline) ++result.framesInWindow;
    }
    result.elapsed = std::chrono::duration<double>(Clock::now() - start).count();
    return result;
}

void writeRow(QTextStream& csv, const Pass& pass, const Result& r, bool prepareGeometry, bool adaptiveCampaign) {
    const float hour=pass.profile==1?23.f:pass.profile==2?17.f:solarHour;
    csv << pass.name << ',' << cameraId << ',' << hour << ',' << width << ',' << height << ',';
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
            << ',' << g.geometryIndexCount
            << ',' << (g.planarReflectionRendered?"true":"false")
            << ',' << (g.lumaProbeLightingEnabled?"true":"false");
    } else {
        csv << ",,,,,,";
    }
    if(adaptiveCampaign){
        csv << ',' << (pass.adaptive?"on":"off") << ',' << (pass.profile==0?"day":pass.profile==1?"night":"transition") << ',' << pass.repetition;
        if(r.error.empty()&&!r.frameMs.empty()){
            auto values=r.frameMs;std::sort(values.begin(),values.end());
            auto pct=[&](double q){return values[size_t(std::ceil(q*values.size()))-1];};
            csv << ',' << pct(.5) << ',' << pct(.95) << ',' << (r.triangleSum?100.*double(r.zeroSum)/double(r.triangleSum):0.) << ',' << r.lastFrame.lumaAdaptiveTriangleCount;
        }else csv << ",,,,";
    }
    csv << '\n';
    csv.flush();
}
} // namespace

int main(int argc, char** argv) {
    SetProcessDPIAware();  // Physical client pixels on the primary monitor.
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    bool prepareGeometry = true, adaptiveCampaign=false;
    if(args.size()<3)return 2;
    for(int i=3;i<args.size();++i){
        if(args[i]==QStringLiteral("--adaptive-ab"))adaptiveCampaign=true;
        else if(args[i]==QStringLiteral("--geometry-prep")&&i+1<args.size()&&
                (args[i+1]==QStringLiteral("on")||args[i+1]==QStringLiteral("off")))prepareGeometry=args[++i]==QStringLiteral("on");
        else {std::cerr<<"Usage: route30.exe scene.json resultats.csv [--geometry-prep on|off] [--adaptive-ab]\n";return 2;}
    }
    if(_putenv_s("M3D_V17_LUMA_ADAPTIVE","0"))return 2;
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
    m3d::setRasterHybridLightingEnabled(true);
    m3d::Stats stats{};
    const auto shadow = m3d::buildShadow(rt.renderScene, stats);
    QFile file(args[2]);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) { std::cerr << file.errorString().toStdString() << std::endl; return 6; }
    QTextStream csv(&file);
    csv << "backend,camera,hour,width,height,frames_completed_30s,elapsed_s,frames_started,mean_render_ms,fps_30s,"
           "presentation,vsync,readback_buffers,status,error,geometry_prep_requested,geometry_prep_enabled,"
           "geometry_source_vertices,geometry_unique_vertices,geometry_index_count,planar_reflection,luma_probe_lighting";
    if(adaptiveCampaign)csv<<",adaptive_requested,profile,repetition,p50_present_ms,p95_present_ms,mean_zero_percent,triangle_count";
    csv<<'\n';
    std::vector<Pass> campaign;
    if(adaptiveCampaign){
        for(int profile=0;profile<3;++profile)for(int repeat=1;repeat<=4;++repeat)
            for(bool on:{false,true})campaign.push_back({Backend::lumaTrace,on?"LumaTrace-ON":"LumaTrace-OFF",on!=(repeat%2==0),profile,repeat});
        // Alternate A/B order to reduce thermal/clock drift. Name follows actual mode.
        for(auto& pass:campaign)pass.name=pass.adaptive?"LumaTrace-ON":"LumaTrace-OFF";
    }else campaign.assign(std::begin(passes),std::end(passes));
    std::vector<m3d::ShadowMap> transitionShadows;
    m3d::ShadowMap nightShadow;
    if(adaptiveCampaign){
        m3d::updateLighting(23.f);nightShadow=m3d::buildShadow(rt.renderScene,stats);
        for(int phase=0;phase<6;++phase){m3d::updateLighting(17.f+phase*.8f);transitionShadows.push_back(m3d::buildShadow(rt.renderScene,stats));}
    }
    bool allPassed = true;
    for (const Pass& pass : campaign) {
        if(_putenv_s("M3D_V17_LUMA_ADAPTIVE",pass.adaptive?"1":"0"))return 2;
        m3d::updateLighting(pass.profile==1?23.f:pass.profile==2?17.f:solarHour);
        Result result;
        // Each graphics API owns a dedicated HWND, as in the exploration window.
        HWND window = openRouteWindow(result.error);
        if (window) {
            const Route route{rt,*it,pass.profile==1?nightShadow:shadow,window,prepareGeometry,
                pass.profile==2?&transitionShadows:nullptr,adaptiveCampaign,pass.adaptive};
            result = measure(route, pass);
            if (pass.backend == Backend::vulkan) m3d::releaseVulkanDirectForCurrentThread();
            else m3d::releaseDirectX11NativeForCurrentThread();
            DestroyWindow(window);
        }
        writeRow(csv, pass, result, prepareGeometry, adaptiveCampaign);
        allPassed = allPassed && result.error.empty();
        if (result.cancelled) break;
    }
    if (csv.status() != QTextStream::Ok || !file.flush()) return 6;
    return allPassed ? 0 : 7;
}
