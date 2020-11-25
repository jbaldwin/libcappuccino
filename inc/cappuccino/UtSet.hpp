#pragma once

#include "cappuccino/Allow.hpp"
#include "cappuccino/Lock.hpp"

#include <atomic>
#include <chrono>
#include <list>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace cappuccino {
/**
 * Uniform time aware associative set for keys. The associative set has no fixed
 * size and will dynamically resize as key values are added. The set is less
 * performant than the cache data structures, but offers but offers unlimited
 * size.
 *
 * Each key is evicted based on a global set TTL (time to live)
 * Expired keys are evicted on the first Insert, Delete, Find, or
 * CleanExpiredValues call after the TTL has elapsed. The set may have O(N)
 * performance during these operations if N elements are reaching their TTLs and
 * being evicted at once.
 *
 * This set is sync aware and can be used concurrently from multiple threads
 * safely. To remove locks/synchronization use Sync::NO when creating the cache.
 *
 * @tparam KeyType The key type.  Must support std::hash().
 * @tparam SyncType By default this set is thread safe, can be disabled for sets
 * specific to a single thread.
 */
template <typename KeyType, Sync SyncType = Sync::YES> class UtSet {
public:
  /**
   * @param uniform_ttl The uniform TTL of keys inserted into the set. 100ms
   * default.
   */
  UtSet(std::chrono::milliseconds uniform_ttl = std::chrono::milliseconds{100});

  /**
   * Inserts or updates the given key.  On update will reset the TTL.
   * @param key The key to store.
   * @param allow Allowed methods of insertion | update.  Defaults to allowing
   *              insertions and updates.
   * @return True if the operation was successful based on `allow`.
   */
  auto Insert(const KeyType &key, Allow allow = Allow::INSERT_OR_UPDATE)
      -> bool;

  /**
   * Inserts or updates a range of keys with uniform TTL.
   * @tparam RangeType A container of KeyTypes.
   * @param key_range The elements to insert or update into the set.
   * @param allow Allowed methods of insertion | update.  Defaults to allowing
   *              insertions and updates.
   * @return The number of elements inserted based on `allow`.
   */
  template <typename RangeType>
  auto InsertRange(RangeType &&key_range, Allow allow = Allow::INSERT_OR_UPDATE)
      -> size_t;

  /**
   * Attempts to delete the given key.
   * @param key The key to remove from the set.
   * @return True if the key was deleted, false if the key does not exist.
   */
  auto Delete(const KeyType &key) -> bool;

  /**
   * Attempts to delete all given keys.
   * @tparam RangeType A container with the set of keys to delete, e.g.
   * vector<k> or set<k>.
   * @param key_range The keys to delete from the set.
   * @return The number of items deleted from the set.
   */
  template <template <class...> typename RangeType>
  auto DeleteRange(const RangeType<KeyType> &key_range) -> size_t;

  /**
   * Attempts to find the given key.
   * @param key The key to lookup.
   * @return True if key exists, or false if it does not.
   */
  auto Find(const KeyType &key) -> bool;

  /**
   * Attempts to find all the given keys presence.
   * @tparam RangeType A container with the set of keys to lookup, e.g.
   * vector<KeyType>.
   * @param key_range A container with the set of keys to lookup.
   * @return All input keys with a bool indicating if it exists.
   */
  template <template <class...> typename RangeType>
  auto FindRange(const RangeType<KeyType> &key_range)
      -> std::vector<std::pair<KeyType, bool>>;

  /**
   * Attempts to find all given keys presence.
   *
   * The user should initialize this container with the keys to lookup with the
   * values all bools. The keys that are found will have the bools set
   * indicating presence in the set.
   *
   * @tparam RangeType A container with a pair of optional items,
   *                   e.g. vector<pair<k, bool>> or map<k, bool>.
   * @param key_bool_range The keys to bools to fill out.
   */
  template <typename RangeType>
  auto FindRangeFill(RangeType &key_bool_range) -> void;

  /**
   * Trims the TTL list of items and evicts all expired elements.
   * @return The number of elements pruned.
   */
  auto CleanExpiredValues() -> size_t;

  /**
   * @return If this set is currently empty.
   */
  auto empty() const -> bool { return size() == 0ul; }

  /**
   * @return The number of elements inside the set.
   */
  auto size() const -> size_t;

private:
  struct KeyedElement;
  struct TtlElement;

  using KeyedIterator = typename std::map<KeyType, KeyedElement>::iterator;
  using TtlIterator = typename std::list<TtlElement>::iterator;

  struct KeyedElement {
    /// The iterator into the ttl data structure.
    TtlIterator m_ttl_position;
  };

  struct TtlElement {
    TtlElement(std::chrono::steady_clock::time_point expire_time,
               KeyedIterator keyed_elements_position)
        : m_expire_time(expire_time),
          m_keyed_elements_position(keyed_elements_position) {}
    /// The point in time in  which this element's key expires.
    std::chrono::steady_clock::time_point m_expire_time;
    /// The iterator to the Key in the data structure.
    KeyedIterator m_keyed_elements_position;
  };

  auto doInsertUpdate(const KeyType &key,
                      std::chrono::steady_clock::time_point expire_time,
                      Allow allow) -> bool;

  auto doInsert(const KeyType &key,
                std::chrono::steady_clock::time_point expire_time) -> void;

  auto doUpdate(KeyedIterator keyed_position,
                std::chrono::steady_clock::time_point expire_time) -> void;

  auto doDelete(KeyedIterator keyed_elements_position) -> void;

  auto doFind(const KeyType &key) -> bool;

  auto doPrune(std::chrono::steady_clock::time_point now) -> size_t;

  /// Thread lock for all mutations.
  Lock<SyncType> m_lock;

  /// The keyed lookup data structure, the value is the KeyedElement struct
  /// which is an iterator to the associated m_ttl_list TTlElement.
  typename std::map<KeyType, KeyedElement> m_keyed_elements;

  /// The uniform ttl sorted list.
  typename std::list<TtlElement> m_ttl_list;

  /// The uniform TTL for every key inserted into the set.
  std::chrono::milliseconds m_uniform_ttl;
};
} // namespace cappuccino

namespace cappuccino {

template <typename KeyType, Sync SyncType>
UtSet<KeyType, SyncType>::UtSet(std::chrono::milliseconds uniform_ttl)
    : m_uniform_ttl(uniform_ttl) {}

template <typename KeyType, Sync SyncType>
auto UtSet<KeyType, SyncType>::Insert(const KeyType &key, Allow allow) -> bool {
  std::lock_guard guard{m_lock};
  const auto now = std::chrono::steady_clock::now();
  const auto expire_time = now + m_uniform_ttl;

  doPrune(now);

  return doInsertUpdate(key, expire_time, allow);
}

template <typename KeyType, Sync SyncType>
template <typename RangeType>
auto UtSet<KeyType, SyncType>::InsertRange(RangeType &&key_range, Allow allow)
    -> size_t {
  size_t inserted{0};
  std::lock_guard guard{m_lock};
  const auto now = std::chrono::steady_clock::now();
  const auto expire_time = now + m_uniform_ttl;

  doPrune(now);

  for (const auto &key : key_range) {
    if (doInsertUpdate(key, expire_time, allow)) {
      ++inserted;
    }
  }

  return inserted;
}

template <typename KeyType, Sync SyncType>
auto UtSet<KeyType, SyncType>::Delete(const KeyType &key) -> bool {
  std::lock_guard guard{m_lock};
  const auto now = std::chrono::steady_clock::now();

  doPrune(now);

  const auto keyed_position = m_keyed_elements.find(key);
  if (keyed_position != m_keyed_elements.end()) {
    doDelete(keyed_position);
    return true;
  } else {
    return false;
  }
};

template <typename KeyType, Sync SyncType>
template <template <class...> typename RangeType>
auto UtSet<KeyType, SyncType>::DeleteRange(const RangeType<KeyType> &key_range)
    -> size_t {
  size_t deleted{0};
  std::lock_guard guard{m_lock};
  const auto now = std::chrono::steady_clock::now();

  doPrune(now);

  for (const auto &key : key_range) {
    const auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
      doDelete(keyed_position);
      ++deleted;
    }
  }

  return deleted;
};

template <typename KeyType, Sync SyncType>
auto UtSet<KeyType, SyncType>::Find(const KeyType &key) -> bool {
  std::lock_guard guard{m_lock};
  const auto now = std::chrono::steady_clock::now();

  doPrune(now);

  return doFind(key);
}

template <typename KeyType, Sync SyncType>
template <template <class...> typename RangeType>
auto UtSet<KeyType, SyncType>::FindRange(const RangeType<KeyType> &key_range)
    -> std::vector<std::pair<KeyType, bool>> {
  std::vector<std::pair<KeyType, bool>> output;
  output.reserve(std::size(key_range));

  std::lock_guard guard{m_lock};
  const auto now = std::chrono::steady_clock::now();

  doPrune(now);

  for (const auto &key : key_range) {
    output.emplace_back(key, doFind(key));
  }

  return output;
}

template <typename KeyType, Sync SyncType>
template <typename RangeType>
auto UtSet<KeyType, SyncType>::FindRangeFill(RangeType &key_bool_range)
    -> void {
  std::lock_guard guard{m_lock};
  const auto now = std::chrono::steady_clock::now();

  doPrune(now);

  for (auto &[key, boolean] : key_bool_range) {
    boolean = doFind(key);
  }
}

template <typename KeyType, Sync SyncType>
auto UtSet<KeyType, SyncType>::CleanExpiredValues() -> size_t {
  std::lock_guard guard{m_lock};
  const auto now = std::chrono::steady_clock::now();
  return doPrune(now);
}

template <typename KeyType, Sync SyncType>
auto UtSet<KeyType, SyncType>::size() const -> size_t {
  std::atomic_thread_fence(std::memory_order_acquire);
  return m_keyed_elements.size();
}

template <typename KeyType, Sync SyncType>
auto UtSet<KeyType, SyncType>::doInsertUpdate(
    const KeyType &key, std::chrono::steady_clock::time_point expire_time,
    Allow allow) -> bool {
  const auto keyed_position = m_keyed_elements.find(key);
  if (keyed_position != m_keyed_elements.end()) {
    if (update_allowed(allow)) {
      doUpdate(keyed_position, expire_time);
      return true;
    }
  } else {
    if (insert_allowed(allow)) {
      doInsert(key, expire_time);
      return true;
    }
  }
  return false;
}

template <typename KeyType, Sync SyncType>
auto UtSet<KeyType, SyncType>::doInsert(
    const KeyType &key, std::chrono::steady_clock::time_point expire_time)
    -> void {
  KeyedElement element;

  auto keyed_position = m_keyed_elements.emplace(key, std::move(element)).first;

  m_ttl_list.emplace_back(expire_time, keyed_position);

  // Update the elements iterator to ttl_element.
  keyed_position->second.m_ttl_position = std::prev(m_ttl_list.end());
}

template <typename KeyType, Sync SyncType>
auto UtSet<KeyType, SyncType>::doUpdate(
    KeyedIterator keyed_position,
    std::chrono::steady_clock::time_point expire_time) -> void {
  auto &element = keyed_position->second;

  // Update the TtlElement's expire time.
  element.m_ttl_position->m_expire_time = expire_time;

  // Push to the end of the Ttl list.
  m_ttl_list.splice(m_ttl_list.end(), m_ttl_list, element.m_ttl_position);

  // Update the elements iterator to ttl_element.
  element.m_ttl_position = std::prev(m_ttl_list.end());
}

template <typename KeyType, Sync SyncType>
auto UtSet<KeyType, SyncType>::doDelete(KeyedIterator keyed_elements_position)
    -> void {
  m_ttl_list.erase(keyed_elements_position->second.m_ttl_position);

  m_keyed_elements.erase(keyed_elements_position);
}

template <typename KeyType, Sync SyncType>
auto UtSet<KeyType, SyncType>::doFind(const KeyType &key) -> bool {
  const auto keyed_position = m_keyed_elements.find(key);
  if (keyed_position != m_keyed_elements.end()) {
    return true;
  }

  return false;
}

template <typename KeyType, Sync SyncType>
auto UtSet<KeyType, SyncType>::doPrune(
    std::chrono::steady_clock::time_point now) -> size_t {
  const auto ttl_begin = m_ttl_list.begin();
  const auto ttl_end = m_ttl_list.end();
  TtlIterator ttl_iter;
  size_t deleted{0};

  // Delete the keyed elements from the set. Not using doDelete to take
  // advantage of iterator range delete for TTLs.
  for (ttl_iter = ttl_begin;
       ttl_iter != ttl_end && now >= ttl_iter->m_expire_time; ++ttl_iter) {
    m_keyed_elements.erase(ttl_iter->m_keyed_elements_position);
    ++deleted;
  }

  // Delete the range of TTLs.
  if (ttl_iter != ttl_begin) {
    m_ttl_list.erase(ttl_begin, ttl_iter);
  }

  return deleted;
}

} // namespace cappuccino
