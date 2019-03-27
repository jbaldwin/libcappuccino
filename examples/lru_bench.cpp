#include <cappuccino/Cappuccino.h>

#include <chrono>
#include <iostream>
#include <sstream>

template <typename ValueType>
auto to_string(ValueType value) -> std::string
{
    static std::stringstream ss;

    ss.clear();
    ss.str("");

    ss << value;
    return ss.str();
}

int main(int argc, char* argv[])
{
    using namespace std::chrono_literals;

    {
        auto start = std::chrono::steady_clock::now();
        cappuccino::LruCache<std::string, uint64_t> lru_cache { 100'000 };

        // Lets try inserting 1 million items.
        for (size_t i = 0; i < 1'000'000; ++i) {
            lru_cache.Insert(10s, to_string(i), i);
        }

        auto stop = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
        std::cout << "Inserting 1 million <string, uint64> items took " << elapsed << "ms.\n";
    }

    {
        auto start = std::chrono::steady_clock::now();
        cappuccino::LruCache<std::string, uint64_t> lru_cache { 100'000 };

        std::vector<cappuccino::LruCache<std::string, uint64_t>::KeyValue> items;
        items.reserve(1'000'000);
        for (size_t i = 0; i < 1'000'000; ++i) {
            items.emplace_back(10s, to_string(i), i);
        }
        lru_cache.InsertRange(items);

        auto stop = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
        std::cout << "Inserting 1 million <string, uint64> items batched took " << elapsed << "ms.\n";
    }

    {
        auto start = std::chrono::steady_clock::now();
        cappuccino::LruCache<uint64_t, uint64_t> lru_cache { 100'000 };

        // Lets try inserting 1 million items.
        for (size_t i = 0; i < 1'000'000; ++i) {
            lru_cache.Insert(10s, i, i);
        }

        auto stop = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
        std::cout << "Inserting 1 million <uint64, uint64> items took " << elapsed << "ms.\n";
    }

    {
        auto start = std::chrono::steady_clock::now();
        cappuccino::LruCache<uint64_t, uint64_t> lru_cache { 100'000 };

        std::vector<cappuccino::LruCache<uint64_t, uint64_t>::KeyValue> items;
        items.reserve(1'000'000);
        for (size_t i = 0; i < 1'000'000; ++i) {
            items.emplace_back(10s, i, i);
        }
        lru_cache.InsertRange(items);

        auto stop = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
        std::cout << "Inserting 1 million <uint64, uint64> items batched took " << elapsed << "ms.\n";
    }

    {
        auto start = std::chrono::steady_clock::now();
        cappuccino::LruCache<std::string, uint64_t> lru_cache { 100'000 };

        // Lets try inserting 1 million items.
        for (size_t i = 0; i < 1'000'000; ++i) {
            lru_cache.Insert(10s, to_string(1), i);
        }

        auto stop = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
        std::cout << "Updating 1 million <string, uint64> items took " << elapsed << "ms.\n";
    }

    {
        auto start = std::chrono::steady_clock::now();
        cappuccino::LruCache<std::string, uint64_t> lru_cache { 100'000 };

        std::vector<cappuccino::LruCache<std::string, uint64_t>::KeyValue> items;
        items.reserve(1'000'000);
        for (size_t i = 0; i < 1'000'000; ++i) {
            items.emplace_back(10s, to_string(1), i);
        }
        lru_cache.InsertRange(items);

        auto stop = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
        std::cout << "Updating 1 million <string, uint64> items batched took " << elapsed << "ms.\n";
    }

    {
        auto start = std::chrono::steady_clock::now();
        cappuccino::LruCache<uint64_t, uint64_t> lru_cache { 100'000 };

        // Lets try inserting 1 million items.
        for (size_t i = 0; i < 1'000'000; ++i) {
            lru_cache.Insert(10s, 1, i);
        }

        auto stop = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
        std::cout << "Updating 1 million <uint64, uint64> items took " << elapsed << "ms.\n";
    }

    {
        auto start = std::chrono::steady_clock::now();
        cappuccino::LruCache<uint64_t, uint64_t> lru_cache { 100'000 };

        std::vector<cappuccino::LruCache<uint64_t, uint64_t>::KeyValue> items;
        items.reserve(1'000'000);
        for (size_t i = 0; i < 1'000'000; ++i) {
            items.emplace_back(10s, 1, i);
        }
        lru_cache.InsertRange(items);

        auto stop = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
        std::cout << "Updating 1 million <uint64, uint64> items batched took " << elapsed << "ms.\n";
    }

    return 0;
}
