#pragma once
// Real Windows GPU Engine utilization counters. Do not infer GPU load from
// render milliseconds: sample live WDDM busy-engine percentages instead.
// Reports the busiest GPU engine (not an arithmetic sum of engines).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <pdh.h>
#endif

namespace m3d {
struct GpuLoadSample {
    double systemBusyPercent = -1.;
    double processBusyPercent = -1.;
};
class GpuLoadMeter {
public:
    GpuLoadMeter() {
#ifdef _WIN32
        pidTag_ = L"pid_" + std::to_wstring(GetCurrentProcessId()) + L"_";
        if (PdhOpenQueryW(nullptr, 0, &query_) != ERROR_SUCCESS) return;
        if (PdhAddEnglishCounterW(query_, L"\\GPU Engine(*)\\Utilization Percentage", 0, &counter_) != ERROR_SUCCESS) {
            PdhCloseQuery(query_); query_ = nullptr; return;
        }
        if (PdhCollectQueryData(query_) != ERROR_SUCCESS) {
            PdhCloseQuery(query_); query_ = nullptr; counter_ = nullptr; return;
        }
        ready_ = true;
#endif
    }
    GpuLoadMeter(const GpuLoadMeter&) = delete;
    GpuLoadMeter& operator=(const GpuLoadMeter&) = delete;
    ~GpuLoadMeter() {
#ifdef _WIN32
        if (query_) PdhCloseQuery(query_);
#endif
    }
    bool available() const { return ready_; }
    GpuLoadSample sample() {
        GpuLoadSample result;
#ifdef _WIN32
        if (!ready_ || PdhCollectQueryData(query_) != ERROR_SUCCESS) return result;
        DWORD bytes = 0, count = 0;
        const PDH_STATUS status = PdhGetFormattedCounterArrayW(counter_, PDH_FMT_DOUBLE, &bytes, &count, nullptr);
        if (status != PDH_MORE_DATA || !bytes) return result;
        // Some Windows drivers advertise many GPU engines and process instances.
        std::vector<unsigned char> storage(bytes + alignof(PDH_FMT_COUNTERVALUE_ITEM_W));
        void* address = storage.data();
        size_t space = storage.size();
        if (std::align(alignof(PDH_FMT_COUNTERVALUE_ITEM_W), bytes, address, space) == nullptr) return result;
        auto* values = static_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(address);
        if (PdhGetFormattedCounterArrayW(counter_, PDH_FMT_DOUBLE, &bytes, &count, values) != ERROR_SUCCESS) return result;
        for (DWORD i = 0; i < count; ++i) {
            const PDH_FMT_COUNTERVALUE& item = values[i].FmtValue;
            if (item.CStatus != PDH_CSTATUS_VALID_DATA && item.CStatus != PDH_CSTATUS_NEW_DATA) continue;
            if (!std::isfinite(item.doubleValue) || item.doubleValue < 0) continue;
            const double pct = std::clamp(item.doubleValue, 0., 100.);
            result.systemBusyPercent = std::max(result.systemBusyPercent, pct);
            const wchar_t* instance = values[i].szName;
            if (instance && std::wstring(instance).find(pidTag_) != std::wstring::npos)
                result.processBusyPercent = std::max(result.processBusyPercent, pct);
        }
#endif
        return result;
    }
private:
    bool ready_ = false;
#ifdef _WIN32
    PDH_HQUERY query_ = nullptr;
    PDH_HCOUNTER counter_ = nullptr;
    std::wstring pidTag_;
#endif
};
}
