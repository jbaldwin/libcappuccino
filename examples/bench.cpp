#include <cappuccino/Cappuccino.h>

#include <chrono>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <typeinfo>

using namespace cappuccino;
using namespace std::chrono_literals;

enum class BatchInsertEnum {
    NO,
    YES
};

template <typename ValueType>
auto to_string(ValueType value) -> std::string
{
    thread_local std::stringstream ss;

    ss.clear();
    ss.str("");

    ss << value;
    return ss.str();
}

template <
    size_t ITERATIONS,
    size_t WORKER_COUNT,
    size_t CACHE_SIZE,
    typename KeyType,
    typename ValueType,
    SyncImplEnum SyncType,
    BatchInsertEnum BatchType>
static auto tlru_cache_bench_test(std::chrono::seconds ttl) -> void
{
    std::mutex cout_lock {};
    TlruCache<KeyType, ValueType, SyncType> lru_cache { CACHE_SIZE };

    std::cout << "TLRU ";
    if constexpr (SyncType == SyncImplEnum::SYNC) {
        std::cout << "SYNC ";
    } else {
        std::cout << "UNSYNC ";
    }

    auto key_name = "string";
    if constexpr (std::is_same<KeyType, uint64_t>::value) {
        key_name = "uint64";
    }
    auto value_name = "string";
    if constexpr (std::is_same<ValueType, uint64_t>::value) {
        value_name = "uint64";
    }

    if constexpr (BatchType == BatchInsertEnum::NO) {
        std::cout << "Individual ";
    } else if constexpr (BatchType == BatchInsertEnum::YES) {
        std::cout << "Batch ";
    }

    std::cout << "<" << key_name << ", " << value_name << "> ";

    std::vector<std::thread> workers;
    workers.reserve(WORKER_COUNT);

    auto func = [&]() mutable -> void {
        size_t worker_iterations = ITERATIONS / WORKER_COUNT;

        std::chrono::milliseconds insert_elapsed { 0 };
        std::chrono::milliseconds find_elapsed { 0 };

        auto start = std::chrono::steady_clock::now();

        if constexpr (BatchType == BatchInsertEnum::NO) {
            for (size_t i = 0; i < worker_iterations; ++i) {

                if constexpr (
                    std::is_same<KeyType, std::string>::value
                    && std::is_same<ValueType, std::string>::value) {
                    auto s = to_string(i);
                    lru_cache.Insert(ttl, s, s);
                } else if constexpr (
                    std::is_same<KeyType, std::string>::value
                    && std::is_same<ValueType, uint64_t>::value) {
                    lru_cache.Insert(ttl, to_string(i), i);
                } else if constexpr (
                    std::is_same<KeyType, uint64_t>::value
                    && std::is_same<ValueType, uint64_t>::value) {
                    lru_cache.Insert(ttl, i, i);
                } else if constexpr (
                    std::is_same<KeyType, uint64_t>::value
                    && std::is_same<ValueType, std::string>::value) {
                    lru_cache.Insert(ttl, i, to_string(i));
                } else {
                    throw std::runtime_error("invalid type parameters");
                }
            }
        } else {

            std::vector<typename TlruCache<KeyType, ValueType, SyncType>::KeyValue> data;
            data.reserve(worker_iterations);

            for (size_t i = 0; i < worker_iterations; ++i) {
                if constexpr (
                    std::is_same<KeyType, std::string>::value
                    && std::is_same<ValueType, std::string>::value) {
                    auto s = to_string(i);
                    data.emplace_back(ttl, s, s);
                } else if constexpr (
                    std::is_same<KeyType, std::string>::value
                    && std::is_same<ValueType, uint64_t>::value) {
                    data.emplace_back(ttl, to_string(i), i);
                } else if constexpr (
                    std::is_same<KeyType, uint64_t>::value
                    && std::is_same<ValueType, uint64_t>::value) {
                    data.emplace_back(ttl, i, i);
                } else if constexpr (
                    std::is_same<KeyType, uint64_t>::value
                    && std::is_same<ValueType, std::string>::value) {
                    data.emplace_back(ttl, i, to_string(i));
                } else {
                    throw std::runtime_error("invalid type parameters");
                }
            }

            lru_cache.InsertRange(std::move(data));
        }

        auto stop = std::chrono::steady_clock::now();

        insert_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

        start = std::chrono::steady_clock::now();

        if constexpr (BatchType == BatchInsertEnum::NO) {
            for (size_t i = 0; i < worker_iterations; ++i) {
                if constexpr (std::is_same<KeyType, std::string>::value) {
                    lru_cache.Find(to_string(i));
                } else if constexpr (std::is_same<KeyType, uint64_t>::value) {
                    lru_cache.Find(i);
                } else {
                    throw std::runtime_error("invalid type parameters");
                }
            }
        } else {

            std::unordered_map<KeyType, std::optional<ValueType>> data;
            data.reserve(worker_iterations);

            for (size_t i = 0; i < worker_iterations; ++i) {
                if constexpr (std::is_same<KeyType, std::string>::value) {
                    data.emplace(to_string(i), std::optional<ValueType> {});
                } else if constexpr (std::is_same<KeyType, uint64_t>::value) {
                    data.emplace(i, std::optional<ValueType> {});
                } else {
                    throw std::runtime_error("invalid type parameters");
                }
            }

            lru_cache.FindRangeFill(data);
        }

        stop = std::chrono::steady_clock::now();

        find_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

        std::lock_guard guard { cout_lock };
        std::cout << "[" << insert_elapsed.count() << ", " << find_elapsed.count() << "] ";
    };

    for (size_t i = 0; i < WORKER_COUNT; ++i) {
        workers.emplace_back(func);
    }

    for (auto& worker : workers) {
        worker.join();
    }

    std::cout << "\n";
}

template <
    size_t ITERATIONS,
    size_t WORKER_COUNT,
    size_t CACHE_SIZE,
    typename KeyType,
    typename ValueType,
    SyncImplEnum SyncType,
    BatchInsertEnum BatchType>
static auto utlru_cache_bench_test(std::chrono::seconds ttl) -> void
{
    std::mutex cout_lock {};
    UtlruCache<KeyType, ValueType, SyncType> lru_cache { ttl, CACHE_SIZE };

    std::cout << "ULRU ";
    if constexpr (SyncType == SyncImplEnum::SYNC) {
        std::cout << "SYNC ";
    } else {
        std::cout << "UNSYNC ";
    }

    auto key_name = "string";
    if constexpr (std::is_same<KeyType, uint64_t>::value) {
        key_name = "uint64";
    }
    auto value_name = "string";
    if constexpr (std::is_same<ValueType, uint64_t>::value) {
        value_name = "uint64";
    }

    if constexpr (BatchType == BatchInsertEnum::NO) {
        std::cout << "Individual ";
    } else if constexpr (BatchType == BatchInsertEnum::YES) {
        std::cout << "Batch ";
    }
    std::cout << "<" << key_name << ", " << value_name << "> ";

    std::vector<std::thread> workers;
    workers.reserve(WORKER_COUNT);

    auto func = [&]() mutable -> void {
        size_t worker_iterations = ITERATIONS / WORKER_COUNT;

        std::chrono::milliseconds insert_elapsed { 0 };
        std::chrono::milliseconds find_elapsed { 0 };

        auto start = std::chrono::steady_clock::now();

        if constexpr (BatchType == BatchInsertEnum::NO) {
            for (size_t i = 0; i < worker_iterations; ++i) {

                if constexpr (
                    std::is_same<KeyType, std::string>::value
                    && std::is_same<ValueType, std::string>::value) {
                    auto s = to_string(i);
                    lru_cache.Insert(s, s);
                } else if constexpr (
                    std::is_same<KeyType, std::string>::value
                    && std::is_same<ValueType, uint64_t>::value) {
                    lru_cache.Insert(to_string(i), i);
                } else if constexpr (
                    std::is_same<KeyType, uint64_t>::value
                    && std::is_same<ValueType, uint64_t>::value) {
                    lru_cache.Insert(i, i);
                } else if constexpr (
                    std::is_same<KeyType, uint64_t>::value
                    && std::is_same<ValueType, std::string>::value) {
                    lru_cache.Insert(i, to_string(i));
                } else {
                    throw std::runtime_error("invalid type parameters");
                }
            }
        } else {

            std::vector<typename UtlruCache<KeyType, ValueType, SyncType>::KeyValue> data;
            data.reserve(worker_iterations);

            for (size_t i = 0; i < worker_iterations; ++i) {
                if constexpr (
                    std::is_same<KeyType, std::string>::value
                    && std::is_same<ValueType, std::string>::value) {
                    auto s = to_string(i);
                    data.emplace_back(s, s);
                } else if constexpr (
                    std::is_same<KeyType, std::string>::value
                    && std::is_same<ValueType, uint64_t>::value) {
                    data.emplace_back(to_string(i), i);
                } else if constexpr (
                    std::is_same<KeyType, uint64_t>::value
                    && std::is_same<ValueType, uint64_t>::value) {
                    data.emplace_back(i, i);
                } else if constexpr (
                    std::is_same<KeyType, uint64_t>::value
                    && std::is_same<ValueType, std::string>::value) {
                    data.emplace_back(i, to_string(i));
                } else {
                    throw std::runtime_error("invalid type parameters");
                }
            }

            lru_cache.InsertRange(std::move(data));
        }

        auto stop = std::chrono::steady_clock::now();

        insert_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

        start = std::chrono::steady_clock::now();

        if constexpr (BatchType == BatchInsertEnum::NO) {
            for (size_t i = 0; i < worker_iterations; ++i) {
                if constexpr (std::is_same<KeyType, std::string>::value) {
                    lru_cache.Find(to_string(i));
                } else if constexpr (std::is_same<KeyType, uint64_t>::value) {
                    lru_cache.Find(i);
                } else {
                    throw std::runtime_error("invalid type parameters");
                }
            }
        } else {

            std::unordered_map<KeyType, std::optional<ValueType>> data;
            data.reserve(worker_iterations);

            for (size_t i = 0; i < worker_iterations; ++i) {
                if constexpr (std::is_same<KeyType, std::string>::value) {
                    data.emplace(to_string(i), std::optional<ValueType> {});
                } else if constexpr (std::is_same<KeyType, uint64_t>::value) {
                    data.emplace(i, std::optional<ValueType> {});
                } else {
                    throw std::runtime_error("invalid type parameters");
                }
            }

            lru_cache.FindRangeFill(data);
        }

        stop = std::chrono::steady_clock::now();

        find_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

        std::lock_guard guard { cout_lock };
        std::cout << "[" << insert_elapsed.count() << ", " << find_elapsed.count() << "] ";
    };

    for (size_t i = 0; i < WORKER_COUNT; ++i) {
        workers.emplace_back(func);
    }

    for (auto& worker : workers) {
        worker.join();
    }

    std::cout << "\n";
}

template <
    size_t ITERATIONS,
    size_t WORKER_COUNT,
    size_t CACHE_SIZE,
    typename KeyType,
    typename ValueType,
    SyncImplEnum SyncType,
    BatchInsertEnum BatchType>
static auto lru_cache_bench_test() -> void
{
    std::mutex cout_lock {};
    LruCache<KeyType, ValueType, SyncType> lru_cache { CACHE_SIZE };

    std::cout << "LRU ";
    if constexpr (SyncType == SyncImplEnum::SYNC) {
        std::cout << "SYNC ";
    } else {
        std::cout << "UNSYNC ";
    }

    auto key_name = "string";
    if constexpr (std::is_same<KeyType, uint64_t>::value) {
        key_name = "uint64";
    }
    auto value_name = "string";
    if constexpr (std::is_same<ValueType, uint64_t>::value) {
        value_name = "uint64";
    }

    if constexpr (BatchType == BatchInsertEnum::NO) {
        std::cout << "Individual ";
    } else if constexpr (BatchType == BatchInsertEnum::YES) {
        std::cout << "Batch ";
    }
    std::cout << "<" << key_name << ", " << value_name << "> ";

    std::vector<std::thread> workers;
    workers.reserve(WORKER_COUNT);

    auto func = [&]() mutable -> void {
        size_t worker_iterations = ITERATIONS / WORKER_COUNT;

        std::chrono::milliseconds insert_elapsed { 0 };
        std::chrono::milliseconds find_elapsed { 0 };

        auto start = std::chrono::steady_clock::now();

        if constexpr (BatchType == BatchInsertEnum::NO) {
            for (size_t i = 0; i < worker_iterations; ++i) {

                if constexpr (
                    std::is_same<KeyType, std::string>::value
                    && std::is_same<ValueType, std::string>::value) {
                    auto s = to_string(i);
                    lru_cache.Insert(s, s);
                } else if constexpr (
                    std::is_same<KeyType, std::string>::value
                    && std::is_same<ValueType, uint64_t>::value) {
                    lru_cache.Insert(to_string(i), i);
                } else if constexpr (
                    std::is_same<KeyType, uint64_t>::value
                    && std::is_same<ValueType, uint64_t>::value) {
                    lru_cache.Insert(i, i);
                } else if constexpr (
                    std::is_same<KeyType, uint64_t>::value
                    && std::is_same<ValueType, std::string>::value) {
                    lru_cache.Insert(i, to_string(i));
                } else {
                    throw std::runtime_error("invalid type parameters");
                }
            }
        } else {

            std::vector<typename LruCache<KeyType, ValueType, SyncType>::KeyValue> data;
            data.reserve(worker_iterations);

            for (size_t i = 0; i < worker_iterations; ++i) {
                if constexpr (
                    std::is_same<KeyType, std::string>::value
                    && std::is_same<ValueType, std::string>::value) {
                    auto s = to_string(i);
                    data.emplace_back(s, s);
                } else if constexpr (
                    std::is_same<KeyType, std::string>::value
                    && std::is_same<ValueType, uint64_t>::value) {
                    data.emplace_back(to_string(i), i);
                } else if constexpr (
                    std::is_same<KeyType, uint64_t>::value
                    && std::is_same<ValueType, uint64_t>::value) {
                    data.emplace_back(i, i);
                } else if constexpr (
                    std::is_same<KeyType, uint64_t>::value
                    && std::is_same<ValueType, std::string>::value) {
                    data.emplace_back(i, to_string(i));
                } else {
                    throw std::runtime_error("invalid type parameters");
                }
            }

            lru_cache.InsertRange(std::move(data));
        }

        auto stop = std::chrono::steady_clock::now();

        insert_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

        start = std::chrono::steady_clock::now();

        if constexpr (BatchType == BatchInsertEnum::NO) {
            for (size_t i = 0; i < worker_iterations; ++i) {
                if constexpr (std::is_same<KeyType, std::string>::value) {
                    lru_cache.Find(to_string(i));
                } else if constexpr (std::is_same<KeyType, uint64_t>::value) {
                    lru_cache.Find(i);
                } else {
                    throw std::runtime_error("invalid type parameters");
                }
            }
        } else {

            std::unordered_map<KeyType, std::optional<ValueType>> data;
            data.reserve(worker_iterations);

            for (size_t i = 0; i < worker_iterations; ++i) {
                if constexpr (std::is_same<KeyType, std::string>::value) {
                    data.emplace(to_string(i), std::optional<ValueType> {});
                } else if constexpr (std::is_same<KeyType, uint64_t>::value) {
                    data.emplace(i, std::optional<ValueType> {});
                } else {
                    throw std::runtime_error("invalid type parameters");
                }
            }

            lru_cache.FindRangeFill(data);
        }

        stop = std::chrono::steady_clock::now();

        find_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

        std::lock_guard guard { cout_lock };
        std::cout << "[" << insert_elapsed.count() << ", " << find_elapsed.count() << "] ";
    };

    for (size_t i = 0; i < WORKER_COUNT; ++i) {
        workers.emplace_back(func);
    }

    for (auto& worker : workers) {
        worker.join();
    }

    std::cout << "\n";
}

int main(int argc, char* argv[])
{
    constexpr size_t ITERATIONS = 1'000'000;
    constexpr size_t WORKER_COUNT = 12;
    constexpr size_t CACHE_SIZE = 100'000;

    /**
     * SYNC INDIVIDUAL
     */
    tlru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, std::string, std::string, SyncImplEnum::SYNC, BatchInsertEnum::NO>(10s);
    utlru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, std::string, std::string, SyncImplEnum::SYNC, BatchInsertEnum::NO>(10s);
    lru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, std::string, std::string, SyncImplEnum::SYNC, BatchInsertEnum::NO>();
    std::cout << "\n";

    tlru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, std::string, uint64_t, SyncImplEnum::SYNC, BatchInsertEnum::NO>(10s);
    utlru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, std::string, uint64_t, SyncImplEnum::SYNC, BatchInsertEnum::NO>(10s);
    lru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, std::string, uint64_t, SyncImplEnum::SYNC, BatchInsertEnum::NO>();
    std::cout << "\n";

    tlru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, uint64_t, uint64_t, SyncImplEnum::SYNC, BatchInsertEnum::NO>(10s);
    utlru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, uint64_t, uint64_t, SyncImplEnum::SYNC, BatchInsertEnum::NO>(10s);
    lru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, uint64_t, uint64_t, SyncImplEnum::SYNC, BatchInsertEnum::NO>();
    std::cout << "\n";

    tlru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, uint64_t, std::string, SyncImplEnum::SYNC, BatchInsertEnum::NO>(10s);
    utlru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, uint64_t, std::string, SyncImplEnum::SYNC, BatchInsertEnum::NO>(10s);
    lru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, uint64_t, std::string, SyncImplEnum::SYNC, BatchInsertEnum::NO>();
    std::cout << "\n";

    /**
     * SYNC BATCH
     */
    tlru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, std::string, std::string, SyncImplEnum::SYNC, BatchInsertEnum::YES>(10s);
    utlru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, std::string, std::string, SyncImplEnum::SYNC, BatchInsertEnum::YES>(10s);
    lru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, std::string, std::string, SyncImplEnum::SYNC, BatchInsertEnum::YES>();
    std::cout << "\n";

    tlru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, std::string, uint64_t, SyncImplEnum::SYNC, BatchInsertEnum::YES>(10s);
    utlru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, std::string, uint64_t, SyncImplEnum::SYNC, BatchInsertEnum::YES>(10s);
    lru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, std::string, uint64_t, SyncImplEnum::SYNC, BatchInsertEnum::YES>();
    std::cout << "\n";

    tlru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, uint64_t, uint64_t, SyncImplEnum::SYNC, BatchInsertEnum::YES>(10s);
    utlru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, uint64_t, uint64_t, SyncImplEnum::SYNC, BatchInsertEnum::YES>(10s);
    lru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, uint64_t, uint64_t, SyncImplEnum::SYNC, BatchInsertEnum::YES>();
    std::cout << "\n";

    tlru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, uint64_t, std::string, SyncImplEnum::SYNC, BatchInsertEnum::YES>(10s);
    utlru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, uint64_t, std::string, SyncImplEnum::SYNC, BatchInsertEnum::YES>(10s);
    lru_cache_bench_test<ITERATIONS, WORKER_COUNT, CACHE_SIZE, uint64_t, std::string, SyncImplEnum::SYNC, BatchInsertEnum::YES>();
    std::cout << "\n";

    /**
     * UNSYNC INDIVIDUAL
     */
    tlru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, std::string, std::string, SyncImplEnum::UNSYNC, BatchInsertEnum::NO>(10s);
    utlru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, std::string, std::string, SyncImplEnum::UNSYNC, BatchInsertEnum::NO>(10s);
    lru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, std::string, std::string, SyncImplEnum::UNSYNC, BatchInsertEnum::NO>();
    std::cout << "\n";

    tlru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, std::string, uint64_t, SyncImplEnum::UNSYNC, BatchInsertEnum::NO>(10s);
    utlru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, std::string, uint64_t, SyncImplEnum::UNSYNC, BatchInsertEnum::NO>(10s);
    lru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, std::string, uint64_t, SyncImplEnum::UNSYNC, BatchInsertEnum::NO>();
    std::cout << "\n";

    tlru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, uint64_t, uint64_t, SyncImplEnum::UNSYNC, BatchInsertEnum::NO>(10s);
    utlru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, uint64_t, uint64_t, SyncImplEnum::UNSYNC, BatchInsertEnum::NO>(10s);
    lru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, uint64_t, uint64_t, SyncImplEnum::UNSYNC, BatchInsertEnum::NO>();
    std::cout << "\n";

    tlru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, uint64_t, std::string, SyncImplEnum::UNSYNC, BatchInsertEnum::NO>(10s);
    utlru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, uint64_t, std::string, SyncImplEnum::UNSYNC, BatchInsertEnum::NO>(10s);
    lru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, uint64_t, std::string, SyncImplEnum::UNSYNC, BatchInsertEnum::NO>();
    std::cout << "\n";

    /**
     * UNSYNC BATCH
     */
    tlru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, std::string, std::string, SyncImplEnum::UNSYNC, BatchInsertEnum::YES>(10s);
    utlru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, std::string, std::string, SyncImplEnum::UNSYNC, BatchInsertEnum::YES>(10s);
    lru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, std::string, std::string, SyncImplEnum::UNSYNC, BatchInsertEnum::YES>();
    std::cout << "\n";

    tlru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, std::string, uint64_t, SyncImplEnum::UNSYNC, BatchInsertEnum::YES>(10s);
    utlru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, std::string, uint64_t, SyncImplEnum::UNSYNC, BatchInsertEnum::YES>(10s);
    lru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, std::string, uint64_t, SyncImplEnum::UNSYNC, BatchInsertEnum::YES>();
    std::cout << "\n";

    tlru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, uint64_t, uint64_t, SyncImplEnum::UNSYNC, BatchInsertEnum::YES>(10s);
    utlru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, uint64_t, uint64_t, SyncImplEnum::UNSYNC, BatchInsertEnum::YES>(10s);
    lru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, uint64_t, uint64_t, SyncImplEnum::UNSYNC, BatchInsertEnum::YES>();
    std::cout << "\n";

    tlru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, uint64_t, std::string, SyncImplEnum::UNSYNC, BatchInsertEnum::YES>(10s);
    utlru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, uint64_t, std::string, SyncImplEnum::UNSYNC, BatchInsertEnum::YES>(10s);
    lru_cache_bench_test<ITERATIONS, 1, CACHE_SIZE, uint64_t, std::string, SyncImplEnum::UNSYNC, BatchInsertEnum::YES>();
    std::cout << "\n";

    return 0;
}
