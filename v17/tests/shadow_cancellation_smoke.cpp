#include "m3d_engine.h"

#include <iostream>

int main()
{
    m3d::Scene scene;
    m3d::Stats stats;
    std::atomic<uint64_t> generation{1};
    const auto cancelled = m3d::buildShadow(scene, stats, &generation, 0);
    if (!cancelled.d.empty() || !cancelled.min.empty() || !cancelled.max.empty())
    {
        std::cerr << "Cancelled shadow generation returned partial data.\n";
        return 1;
    }
    return 0;
}
