#include <cappuccino/cappuccino.hpp>

#include <chrono>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <typeinfo>

using namespace cappuccino;
using namespace std::chrono_literals;

enum class batch_insert
{
    no,
    yes
};

template<typename value_type>
auto to_string(value_type value) -> std::string
{
    thread_local std::stringstream ss;

    ss.clear();
    ss.str("");

    ss << value;
    return ss.str();
}

template<
    size_t iterations,
    size_t worker_count,
    size_t cache_size,
    typename key_type,
    typename value_type,
    thread_safe  thread_safe_type,
    batch_insert batch_type>
static auto tlru_cache_bench_test(std::chrono::seconds ttl) -> void
{
    std::mutex                                         cout_lock{};
    tlru_cache<key_type, value_type, thread_safe_type> cache{cache_size};

    std::cout << "TLRU ";
    if constexpr (thread_safe_type == thread_safe::yes)
    {
        std::cout << "thread_safe::yes ";
    }
    else
    {
        std::cout << "thread_safe::no ";
    }

    auto key_name = "string";
    if constexpr (std::is_same<key_type, uint64_t>::value)
    {
        key_name = "uint64";
    }
    auto value_name = "string";
    if constexpr (std::is_same<value_type, uint64_t>::value)
    {
        value_name = "uint64";
    }

    if constexpr (batch_type == batch_insert::no)
    {
        std::cout << "Individual ";
    }
    else if constexpr (batch_type == batch_insert::yes)
    {
        std::cout << "Batch ";
    }

    std::cout << "<" << key_name << ", " << value_name << "> ";

    std::vector<std::thread> workers;
    workers.reserve(worker_count);

    auto func = [&]() mutable -> void
    {
        size_t worker_iterations = iterations / worker_count;

        std::chrono::milliseconds insert_elapsed{0};
        std::chrono::milliseconds find_elapsed{0};

        auto start = std::chrono::steady_clock::now();

        if constexpr (batch_type == batch_insert::no)
        {
            for (size_t i = 0; i < worker_iterations; ++i)
            {
                if constexpr (
                    std::is_same<key_type, std::string>::value && std::is_same<value_type, std::string>::value)
                {
                    auto s = to_string(i);
                    cache.insert(ttl, s, s);
                }
                else if constexpr (
                    std::is_same<key_type, std::string>::value && std::is_same<value_type, uint64_t>::value)
                {
                    cache.insert(ttl, to_string(i), i);
                }
                else if constexpr (std::is_same<key_type, uint64_t>::value && std::is_same<value_type, uint64_t>::value)
                {
                    cache.insert(ttl, i, i);
                }
                else if constexpr (
                    std::is_same<key_type, uint64_t>::value && std::is_same<value_type, std::string>::value)
                {
                    cache.insert(ttl, i, to_string(i));
                }
                else
                {
                    throw std::runtime_error("invalid type parameters");
                }
            }
        }
        else
        {
            std::vector<std::tuple<std::chrono::milliseconds, key_type, value_type>> data;
            data.reserve(worker_iterations);

            for (size_t i = 0; i < worker_iterations; ++i)
            {
                if constexpr (
                    std::is_same<key_type, std::string>::value && std::is_same<value_type, std::string>::value)
                {
                    auto s = to_string(i);
                    data.emplace_back(ttl, s, s);
                }
                else if constexpr (
                    std::is_same<key_type, std::string>::value && std::is_same<value_type, uint64_t>::value)
                {
                    data.emplace_back(ttl, to_string(i), i);
                }
                else if constexpr (std::is_same<key_type, uint64_t>::value && std::is_same<value_type, uint64_t>::value)
                {
                    data.emplace_back(ttl, i, i);
                }
                else if constexpr (
                    std::is_same<key_type, uint64_t>::value && std::is_same<value_type, std::string>::value)
                {
                    data.emplace_back(ttl, i, to_string(i));
                }
                else
                {
                    throw std::runtime_error("invalid type parameters");
                }
            }

            cache.insert_range(std::move(data));
        }

        auto stop = std::chrono::steady_clock::now();

        insert_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

        start = std::chrono::steady_clock::now();

        if constexpr (batch_type == batch_insert::no)
        {
            for (size_t i = 0; i < worker_iterations; ++i)
            {
                if constexpr (std::is_same<key_type, std::string>::value)
                {
                    cache.find(to_string(i));
                }
                else if constexpr (std::is_same<key_type, uint64_t>::value)
                {
                    cache.find(i);
                }
                else
                {
                    throw std::runtime_error("invalid type parameters");
                }
            }
        }
        else
        {
            std::unordered_map<key_type, std::optional<value_type>> data;
            data.reserve(worker_iterations);

            for (size_t i = 0; i < worker_iterations; ++i)
            {
                if constexpr (std::is_same<key_type, std::string>::value)
                {
                    data.emplace(to_string(i), std::optional<value_type>{});
                }
                else if constexpr (std::is_same<key_type, uint64_t>::value)
                {
                    data.emplace(i, std::optional<value_type>{});
                }
                else
                {
                    throw std::runtime_error("invalid type parameters");
                }
            }

            cache.find_range_fill(data);
        }

        stop = std::chrono::steady_clock::now();

        find_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

        std::lock_guard guard{cout_lock};
        std::cout << "[" << insert_elapsed.count() << ", " << find_elapsed.count() << "] ";
    };

    for (size_t i = 0; i < worker_count; ++i)
    {
        workers.emplace_back(func);
    }

    for (auto& worker : workers)
    {
        worker.join();
    }

    std::cout << "\n";
}

template<
    size_t iterations,
    size_t worker_count,
    size_t cache_size,
    typename key_type,
    typename value_type,
    thread_safe  thread_safe_type,
    batch_insert batch_type>
static auto utlru_cache_bench_test(std::chrono::seconds ttl) -> void
{
    std::mutex                                          cout_lock{};
    utlru_cache<key_type, value_type, thread_safe_type> cache{ttl, cache_size};

    std::cout << "ULRU ";
    if constexpr (thread_safe_type == thread_safe::yes)
    {
        std::cout << "thread_safe::yes ";
    }
    else
    {
        std::cout << "thread_safe::no ";
    }

    auto key_name = "string";
    if constexpr (std::is_same<key_type, uint64_t>::value)
    {
        key_name = "uint64";
    }
    auto value_name = "string";
    if constexpr (std::is_same<value_type, uint64_t>::value)
    {
        value_name = "uint64";
    }

    if constexpr (batch_type == batch_insert::no)
    {
        std::cout << "Individual ";
    }
    else if constexpr (batch_type == batch_insert::yes)
    {
        std::cout << "Batch ";
    }
    std::cout << "<" << key_name << ", " << value_name << "> ";

    std::vector<std::thread> workers;
    workers.reserve(worker_count);

    auto func = [&]() mutable -> void
    {
        size_t worker_iterations = iterations / worker_count;

        std::chrono::milliseconds insert_elapsed{0};
        std::chrono::milliseconds find_elapsed{0};

        auto start = std::chrono::steady_clock::now();

        if constexpr (batch_type == batch_insert::no)
        {
            for (size_t i = 0; i < worker_iterations; ++i)
            {
                if constexpr (
                    std::is_same<key_type, std::string>::value && std::is_same<value_type, std::string>::value)
                {
                    auto s = to_string(i);
                    cache.insert(s, s);
                }
                else if constexpr (
                    std::is_same<key_type, std::string>::value && std::is_same<value_type, uint64_t>::value)
                {
                    cache.insert(to_string(i), i);
                }
                else if constexpr (std::is_same<key_type, uint64_t>::value && std::is_same<value_type, uint64_t>::value)
                {
                    cache.insert(i, i);
                }
                else if constexpr (
                    std::is_same<key_type, uint64_t>::value && std::is_same<value_type, std::string>::value)
                {
                    cache.insert(i, to_string(i));
                }
                else
                {
                    throw std::runtime_error("invalid type parameters");
                }
            }
        }
        else
        {
            std::vector<std::pair<key_type, value_type>> data;
            data.reserve(worker_iterations);

            for (size_t i = 0; i < worker_iterations; ++i)
            {
                if constexpr (
                    std::is_same<key_type, std::string>::value && std::is_same<value_type, std::string>::value)
                {
                    auto s = to_string(i);
                    data.emplace_back(s, s);
                }
                else if constexpr (
                    std::is_same<key_type, std::string>::value && std::is_same<value_type, uint64_t>::value)
                {
                    data.emplace_back(to_string(i), i);
                }
                else if constexpr (std::is_same<key_type, uint64_t>::value && std::is_same<value_type, uint64_t>::value)
                {
                    data.emplace_back(i, i);
                }
                else if constexpr (
                    std::is_same<key_type, uint64_t>::value && std::is_same<value_type, std::string>::value)
                {
                    data.emplace_back(i, to_string(i));
                }
                else
                {
                    throw std::runtime_error("invalid type parameters");
                }
            }

            cache.insert_range(std::move(data));
        }

        auto stop = std::chrono::steady_clock::now();

        insert_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

        start = std::chrono::steady_clock::now();

        if constexpr (batch_type == batch_insert::no)
        {
            for (size_t i = 0; i < worker_iterations; ++i)
            {
                if constexpr (std::is_same<key_type, std::string>::value)
                {
                    cache.find(to_string(i));
                }
                else if constexpr (std::is_same<key_type, uint64_t>::value)
                {
                    cache.find(i);
                }
                else
                {
                    throw std::runtime_error("invalid type parameters");
                }
            }
        }
        else
        {
            std::unordered_map<key_type, std::optional<value_type>> data;
            data.reserve(worker_iterations);

            for (size_t i = 0; i < worker_iterations; ++i)
            {
                if constexpr (std::is_same<key_type, std::string>::value)
                {
                    data.emplace(to_string(i), std::optional<value_type>{});
                }
                else if constexpr (std::is_same<key_type, uint64_t>::value)
                {
                    data.emplace(i, std::optional<value_type>{});
                }
                else
                {
                    throw std::runtime_error("invalid type parameters");
                }
            }

            cache.find_range_fill(data);
        }

        stop = std::chrono::steady_clock::now();

        find_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

        std::lock_guard guard{cout_lock};
        std::cout << "[" << insert_elapsed.count() << ", " << find_elapsed.count() << "] ";
    };

    for (size_t i = 0; i < worker_count; ++i)
    {
        workers.emplace_back(func);
    }

    for (auto& worker : workers)
    {
        worker.join();
    }

    std::cout << "\n";
}

template<
    size_t iterations,
    size_t worker_count,
    size_t cache_size,
    typename key_type,
    typename value_type,
    thread_safe  thread_safe_type,
    batch_insert batch_type>
static auto lru_cache_bench_test() -> void
{
    std::mutex                                        cout_lock{};
    lru_cache<key_type, value_type, thread_safe_type> cache{cache_size};

    std::cout << "LRU ";
    if constexpr (thread_safe_type == thread_safe::yes)
    {
        std::cout << "thread_safe::yes ";
    }
    else
    {
        std::cout << "thread_safe::no ";
    }

    auto key_name = "string";
    if constexpr (std::is_same<key_type, uint64_t>::value)
    {
        key_name = "uint64";
    }
    auto value_name = "string";
    if constexpr (std::is_same<value_type, uint64_t>::value)
    {
        value_name = "uint64";
    }

    if constexpr (batch_type == batch_insert::no)
    {
        std::cout << "Individual ";
    }
    else if constexpr (batch_type == batch_insert::yes)
    {
        std::cout << "Batch ";
    }
    std::cout << "<" << key_name << ", " << value_name << "> ";

    std::vector<std::thread> workers;
    workers.reserve(worker_count);

    auto func = [&]() mutable -> void
    {
        size_t worker_iterations = iterations / worker_count;

        std::chrono::milliseconds insert_elapsed{0};
        std::chrono::milliseconds find_elapsed{0};

        auto start = std::chrono::steady_clock::now();

        if constexpr (batch_type == batch_insert::no)
        {
            for (size_t i = 0; i < worker_iterations; ++i)
            {
                if constexpr (
                    std::is_same<key_type, std::string>::value && std::is_same<value_type, std::string>::value)
                {
                    auto s = to_string(i);
                    cache.insert(s, s);
                }
                else if constexpr (
                    std::is_same<key_type, std::string>::value && std::is_same<value_type, uint64_t>::value)
                {
                    cache.insert(to_string(i), i);
                }
                else if constexpr (std::is_same<key_type, uint64_t>::value && std::is_same<value_type, uint64_t>::value)
                {
                    cache.insert(i, i);
                }
                else if constexpr (
                    std::is_same<key_type, uint64_t>::value && std::is_same<value_type, std::string>::value)
                {
                    cache.insert(i, to_string(i));
                }
                else
                {
                    throw std::runtime_error("invalid type parameters");
                }
            }
        }
        else
        {
            std::vector<std::pair<key_type, value_type>> data;
            data.reserve(worker_iterations);

            for (size_t i = 0; i < worker_iterations; ++i)
            {
                if constexpr (
                    std::is_same<key_type, std::string>::value && std::is_same<value_type, std::string>::value)
                {
                    auto s = to_string(i);
                    data.emplace_back(s, s);
                }
                else if constexpr (
                    std::is_same<key_type, std::string>::value && std::is_same<value_type, uint64_t>::value)
                {
                    data.emplace_back(to_string(i), i);
                }
                else if constexpr (std::is_same<key_type, uint64_t>::value && std::is_same<value_type, uint64_t>::value)
                {
                    data.emplace_back(i, i);
                }
                else if constexpr (
                    std::is_same<key_type, uint64_t>::value && std::is_same<value_type, std::string>::value)
                {
                    data.emplace_back(i, to_string(i));
                }
                else
                {
                    throw std::runtime_error("invalid type parameters");
                }
            }

            cache.insert_range(std::move(data));
        }

        auto stop = std::chrono::steady_clock::now();

        insert_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

        start = std::chrono::steady_clock::now();

        if constexpr (batch_type == batch_insert::no)
        {
            for (size_t i = 0; i < worker_iterations; ++i)
            {
                if constexpr (std::is_same<key_type, std::string>::value)
                {
                    cache.find(to_string(i));
                }
                else if constexpr (std::is_same<key_type, uint64_t>::value)
                {
                    cache.find(i);
                }
                else
                {
                    throw std::runtime_error("invalid type parameters");
                }
            }
        }
        else
        {
            std::unordered_map<key_type, std::optional<value_type>> data;
            data.reserve(worker_iterations);

            for (size_t i = 0; i < worker_iterations; ++i)
            {
                if constexpr (std::is_same<key_type, std::string>::value)
                {
                    data.emplace(to_string(i), std::optional<value_type>{});
                }
                else if constexpr (std::is_same<key_type, uint64_t>::value)
                {
                    data.emplace(i, std::optional<value_type>{});
                }
                else
                {
                    throw std::runtime_error("invalid type parameters");
                }
            }

            cache.find_range_fill(data);
        }

        stop = std::chrono::steady_clock::now();

        find_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

        std::lock_guard guard{cout_lock};
        std::cout << "[" << insert_elapsed.count() << ", " << find_elapsed.count() << "] ";
    };

    for (size_t i = 0; i < worker_count; ++i)
    {
        workers.emplace_back(func);
    }

    for (auto& worker : workers)
    {
        worker.join();
    }

    std::cout << "\n";
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    constexpr size_t iterations   = 1'000'000;
    constexpr size_t worker_count = 12;
    constexpr size_t cache_size   = 100'000;

    /**
     * thread_safe::yes INDIVIDUAL
     */
    tlru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        std::string,
        std::string,
        thread_safe::yes,
        batch_insert::no>(10s);
    utlru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        std::string,
        std::string,
        thread_safe::yes,
        batch_insert::no>(10s);
    lru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        std::string,
        std::string,
        thread_safe::yes,
        batch_insert::no>();
    std::cout << "\n";

    tlru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        std::string,
        uint64_t,
        thread_safe::yes,
        batch_insert::no>(10s);
    utlru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        std::string,
        uint64_t,
        thread_safe::yes,
        batch_insert::no>(10s);
    lru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        std::string,
        uint64_t,
        thread_safe::yes,
        batch_insert::no>();
    std::cout << "\n";

    tlru_cache_bench_test<iterations, worker_count, cache_size, uint64_t, uint64_t, thread_safe::yes, batch_insert::no>(
        10s);
    utlru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        uint64_t,
        uint64_t,
        thread_safe::yes,
        batch_insert::no>(10s);
    lru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        uint64_t,
        uint64_t,
        thread_safe::yes,
        batch_insert::no>();
    std::cout << "\n";

    tlru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        uint64_t,
        std::string,
        thread_safe::yes,
        batch_insert::no>(10s);
    utlru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        uint64_t,
        std::string,
        thread_safe::yes,
        batch_insert::no>(10s);
    lru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        uint64_t,
        std::string,
        thread_safe::yes,
        batch_insert::no>();
    std::cout << "\n";

    /**
     * SYNC BATCH
     */
    tlru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        std::string,
        std::string,
        thread_safe::yes,
        batch_insert::yes>(10s);
    utlru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        std::string,
        std::string,
        thread_safe::yes,
        batch_insert::yes>(10s);
    lru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        std::string,
        std::string,
        thread_safe::yes,
        batch_insert::yes>();
    std::cout << "\n";

    tlru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        std::string,
        uint64_t,
        thread_safe::yes,
        batch_insert::yes>(10s);
    utlru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        std::string,
        uint64_t,
        thread_safe::yes,
        batch_insert::yes>(10s);
    lru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        std::string,
        uint64_t,
        thread_safe::yes,
        batch_insert::yes>();
    std::cout << "\n";

    tlru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        uint64_t,
        uint64_t,
        thread_safe::yes,
        batch_insert::yes>(10s);
    utlru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        uint64_t,
        uint64_t,
        thread_safe::yes,
        batch_insert::yes>(10s);
    lru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        uint64_t,
        uint64_t,
        thread_safe::yes,
        batch_insert::yes>();
    std::cout << "\n";

    tlru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        uint64_t,
        std::string,
        thread_safe::yes,
        batch_insert::yes>(10s);
    utlru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        uint64_t,
        std::string,
        thread_safe::yes,
        batch_insert::yes>(10s);
    lru_cache_bench_test<
        iterations,
        worker_count,
        cache_size,
        uint64_t,
        std::string,
        thread_safe::yes,
        batch_insert::yes>();
    std::cout << "\n";

    /**
     * thread_safe::no INDIVIDUAL
     */
    tlru_cache_bench_test<iterations, 1, cache_size, std::string, std::string, thread_safe::no, batch_insert::no>(10s);
    utlru_cache_bench_test<iterations, 1, cache_size, std::string, std::string, thread_safe::no, batch_insert::no>(10s);
    lru_cache_bench_test<iterations, 1, cache_size, std::string, std::string, thread_safe::no, batch_insert::no>();
    std::cout << "\n";

    tlru_cache_bench_test<iterations, 1, cache_size, std::string, uint64_t, thread_safe::no, batch_insert::no>(10s);
    utlru_cache_bench_test<iterations, 1, cache_size, std::string, uint64_t, thread_safe::no, batch_insert::no>(10s);
    lru_cache_bench_test<iterations, 1, cache_size, std::string, uint64_t, thread_safe::no, batch_insert::no>();
    std::cout << "\n";

    tlru_cache_bench_test<iterations, 1, cache_size, uint64_t, uint64_t, thread_safe::no, batch_insert::no>(10s);
    utlru_cache_bench_test<iterations, 1, cache_size, uint64_t, uint64_t, thread_safe::no, batch_insert::no>(10s);
    lru_cache_bench_test<iterations, 1, cache_size, uint64_t, uint64_t, thread_safe::no, batch_insert::no>();
    std::cout << "\n";

    tlru_cache_bench_test<iterations, 1, cache_size, uint64_t, std::string, thread_safe::no, batch_insert::no>(10s);
    utlru_cache_bench_test<iterations, 1, cache_size, uint64_t, std::string, thread_safe::no, batch_insert::no>(10s);
    lru_cache_bench_test<iterations, 1, cache_size, uint64_t, std::string, thread_safe::no, batch_insert::no>();
    std::cout << "\n";

    /**
     * thread_safe::no BATCH
     */
    tlru_cache_bench_test<iterations, 1, cache_size, std::string, std::string, thread_safe::no, batch_insert::yes>(10s);
    utlru_cache_bench_test<iterations, 1, cache_size, std::string, std::string, thread_safe::no, batch_insert::yes>(
        10s);
    lru_cache_bench_test<iterations, 1, cache_size, std::string, std::string, thread_safe::no, batch_insert::yes>();
    std::cout << "\n";

    tlru_cache_bench_test<iterations, 1, cache_size, std::string, uint64_t, thread_safe::no, batch_insert::yes>(10s);
    utlru_cache_bench_test<iterations, 1, cache_size, std::string, uint64_t, thread_safe::no, batch_insert::yes>(10s);
    lru_cache_bench_test<iterations, 1, cache_size, std::string, uint64_t, thread_safe::no, batch_insert::yes>();
    std::cout << "\n";

    tlru_cache_bench_test<iterations, 1, cache_size, uint64_t, uint64_t, thread_safe::no, batch_insert::yes>(10s);
    utlru_cache_bench_test<iterations, 1, cache_size, uint64_t, uint64_t, thread_safe::no, batch_insert::yes>(10s);
    lru_cache_bench_test<iterations, 1, cache_size, uint64_t, uint64_t, thread_safe::no, batch_insert::yes>();
    std::cout << "\n";

    tlru_cache_bench_test<iterations, 1, cache_size, uint64_t, std::string, thread_safe::no, batch_insert::yes>(10s);
    utlru_cache_bench_test<iterations, 1, cache_size, uint64_t, std::string, thread_safe::no, batch_insert::yes>(10s);
    lru_cache_bench_test<iterations, 1, cache_size, uint64_t, std::string, thread_safe::no, batch_insert::yes>();
    std::cout << "\n";

    return 0;
}
