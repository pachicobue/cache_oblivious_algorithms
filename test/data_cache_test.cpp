#include <gtest/gtest.h>

#include "data_cache.hpp"
#include "rng_utility.hpp"
#include "safe_array.hpp"

namespace {
struct Data
{
    int a                = 0;
    double b             = 0;
    unsigned long long c = 0;
    std::vector<int> d{};
    friend bool operator==(const Data& data1, const Data& data2) { return data1.a == data2.a and data1.b == data2.b and data1.c == data2.c and data1.d == data2.d; }
};
Data randomData()
{
    Data data;
    data.a = rng.val<int>(-10, 10);
    data.b = static_cast<double>(rng.val<int>(0, 100)) / 100;
    data.c = rng.val<unsigned long long>(0, 1ULL << 60);
    data.d = std::vector<int>(rng.val<int>(1, 3));
    for (auto& e : data.d) { e = rng.val<int>(-10, 10); }
    return data;
}
}  // anonymous namespace

TEST(DataCacheTest, Constructor)
{
    constexpr std::size_t B = 8;
    constexpr std::size_t M = 40;
    data_cache<B, M> dcache;
    ASSERT_EQ(dcache.PageSize, B);
    ASSERT_EQ(dcache.CacheSize, M);
    ASSERT_EQ(dcache.CacheLineNum, M / B);
    ASSERT_EQ(dcache.statistic().disk_read_count, 0);
    ASSERT_EQ(dcache.statistic().disk_write_count, 0);
}
TEST(DataCacheTest, Read_Sequential)
{
    constexpr std::size_t B = 8;
    constexpr std::size_t M = 120;
    data_cache<B, M> dcache;
    const std::size_t N = 100;
    safe_array<Data, B> datas(N);
    for (std::size_t i = 0; i < N; i++) { datas[i] = randomData(); }
    const std::size_t T = 1000;
    for (std::size_t t = 0; t < T; t++) {
        const std::size_t index = t % N;
        const auto data         = dcache.disk_read<Data>(reinterpret_cast<uintptr_t>(&datas[index]));
        ASSERT_EQ(datas[index], data);
    }
}
TEST(DataCacheTest, Read_Random)
{
    constexpr std::size_t B = 8;
    constexpr std::size_t M = 120;
    data_cache<B, M> dcache;
    const std::size_t N = 100;
    safe_array<Data, B> datas(N);
    for (std::size_t i = 0; i < N; i++) { datas[i] = randomData(); }
    const std::size_t T = 1000;
    for (std::size_t t = 0; t < T; t++) {
        const std::size_t index = rng.val<std::size_t>(0, N - 1);
        const auto data         = dcache.disk_read<Data>(reinterpret_cast<uintptr_t>(&datas[index]));
        ASSERT_EQ(datas[index], data);
    }
}
TEST(DataCacheTest, Write_Sequential)
{
    constexpr std::size_t B = 16;
    constexpr std::size_t M = 160;
    data_cache<B, M> dcache;
    const std::size_t N = 100;
    std::vector<Data> datas(N);
    safe_array<Data, B> dests(N);
    for (std::size_t i = 0; i < N; i++) { datas[i] = randomData(); }
    const std::size_t T = 1000;
    for (std::size_t t = 0; t < T; t++) {
        const std::size_t index = t % N;
        dcache.disk_write(reinterpret_cast<uintptr_t>(&dests[index]), datas[index]);
        const auto data = dcache.disk_read<Data>(reinterpret_cast<uintptr_t>(&dests[index]));
        ASSERT_EQ(datas[index], data);
    }
    for (std::size_t t = 0; t < T; t++) {
        const std::size_t index = t % N;
        const auto data         = dcache.disk_read<Data>(reinterpret_cast<uintptr_t>(&dests[index]));
        ASSERT_EQ(datas[index], data);
        ASSERT_EQ(datas[index], dests[index]);
    }
    dcache.flush();
}

TEST(DataCacheTest, Write_Random)
{
    constexpr std::size_t B = 16;
    constexpr std::size_t M = 160;
    data_cache<B, M> dcache;
    const std::size_t N = 100;
    std::vector<Data> datas(N);
    safe_array<Data, B> dests(N);
    for (std::size_t i = 0; i < N; i++) { datas[i] = randomData(); }
    const std::size_t T = 1000;
    for (std::size_t t = 0; t < T; t++) {
        const std::size_t index = rng.val<std::size_t>(0, N - 1);
        dcache.disk_write(reinterpret_cast<uintptr_t>(&dests[index]), datas[index]);
    }
    for (std::size_t t = 0; t < T; t++) {
        const std::size_t index = rng.val<std::size_t>(0, N - 1);
        const auto data         = dcache.disk_read<Data>(reinterpret_cast<uintptr_t>(&dests[index]));
        ASSERT_EQ(datas[index], data);
        ASSERT_EQ(datas[index], dests[index]);
    }
    dcache.flush();
}

TEST(DataCacheTest, ReadWrite)
{
    constexpr std::size_t B = 16;
    constexpr std::size_t M = 160;
    data_cache<B, M> dcache;
    const std::size_t N = 100;
    safe_array<Data, B> datas(N);
    std::vector<Data> actuals(N);
    for (std::size_t i = 0; i < N; i++) {
        const Data data = randomData();
        datas[i]        = data;
        actuals[i]      = data;
    }
    const std::size_t T = 10000;
    for (std::size_t t = 0; t < T; t++) {
        const std::size_t type  = rng.val<std::size_t>(0, 1);
        const std::size_t index = rng.val<std::size_t>(0, N - 1);
        if (type == 0) {
            const auto data   = dcache.disk_read<Data>(reinterpret_cast<uintptr_t>(&datas[index]));
            const auto actual = actuals[index];
            ASSERT_EQ(data, actual);
        } else {
            const auto data = randomData();
            dcache.disk_write(reinterpret_cast<uintptr_t>(&datas[index]), data);
            actuals[index] = data;
        }
    }
}
