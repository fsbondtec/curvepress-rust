#include <curvepress/curvepress.hpp>

#include <cstdint>
#include <cstdio>
#include <vector>

int main() {
    std::vector<int64_t> ts{0, 1'000'000, 2'000'000, 3'000'000};
    std::vector<double>  val{0.0, 1.0, 0.0, 1.0};

    auto data = curvepress::compress_rdp(ts, val, 0.5);
    auto dec  = curvepress::decompress(data);

    std::printf("curvepress %s: kept %zu of %zu points\n",
                curvepress::version(), dec.timestamps_ns.size(), ts.size());
    return 0;
}
