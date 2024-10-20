/*
 * d4
 * Copyright (C) 2020  Univ. Artois & CNRS
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include "src/hashing/hasher.hpp"
#include <array>
#include <boost/program_options.hpp>
#include <optional>
#include <vector>

#include "BucketManager.hpp"
#include "CacheCleaningManager.hpp"
#include "CachedBucket.hpp"
#include "sparsepp/spp.h"
#include "src/caching/CacheCleaningManager.hpp"
#include "src/caching/cnf/BucketManagerCnf.hpp"
#include "src/hashing/HashString.hpp"
#include "src/specs/SpecManager.hpp"
#include <set>

namespace d4 {
// (Probalistic) LRU cache implementations, the integration into D4's
// interface is very cluncky and prone to bugs since it 
// depends on depth-first free/alloc behavior from DPLL. 
// Should be rewritten...

namespace po = boost::program_options;
template <class T> class CacheListLRU : public CacheManager<T> {
private:
  const unsigned SIZE_HASH = 1024 * 1024;
  struct Node {
    CachedBucket<T> bucket;
    size_t last_use;
  };

  std::vector<std::vector<Node>> hashTable;
  size_t counter = 0;
  size_t max_mem = (1ull << 30) * 5; // 5GB

public:
  /**
   * @brief Construct a new Cache List object
   *
   * @param vm is a map to get the option.
   * @param nbVar is the number of variables.
   * @param specs is a structure to get data about the formula.
   * @param out is the stream where are printed out the logs.
   */
  CacheListLRU(po::variables_map &vm, unsigned nbVar, SpecManager *specs,
               std::ostream &out)
      : CacheManager<T>(vm, nbVar, specs, out) {
    out << "c [CACHE LIST CONSTRUCTOR]\n";
    max_mem = (1ull << 30ull) * vm["cache-fixed-size"].as<int>();
    initHashTable(nbVar);
  } // constructor

  /**
   * @brief Destroy the Cache List object
   */
  ~CacheListLRU() {} // destructor

  /**
   * @brief Add an entry in the cache.
   *
   * @param cb is the bucket we want to add.
   * @param hashValue is the hash value of the bucket.
   * @param val is the value we associate with the bucket.
   */
  inline void pushInHashTable(CachedBucket<T> &cb, unsigned hashValue, T val) {
    hashTable[hashValue % SIZE_HASH].push_back(Node{cb, counter});
    CachedBucket<T> &cbIn = (hashTable[hashValue % SIZE_HASH].back().bucket);
    cbIn.lockedBucket(val);
    this->m_nbCreationBucket++;
    this->m_sumDataSize += cb.szData();
    this->m_nbEntry++;
    this->counter++;
    if (this->m_bucketManager->usedMemory() > max_mem) {

      std::vector<size_t> last_use;
      for (auto &v : hashTable) {
        for (auto &b : v) {
          last_use.push_back(b.last_use);
        }
      }
      if (last_use.size() < 1) {
        return;
      }
      std::sort(last_use.begin(), last_use.end());
      size_t median = last_use[last_use.size() / 3];
      size_t removed = 0;

      for (auto &v : hashTable) {
        for (auto &e : v) {
          if (e.last_use < median) {
            this->m_bucketManager->releaseMemory(e.bucket.data,
                                                 e.bucket.szData());
            removed++;
          }
        }
      }

      for (auto &v : hashTable) {
        std::erase_if(v, [&](const Node &n) {
          if (n.last_use < median) {
            return true;
          }
          return false;
        });
      }
      std::cout << "Removed: " << removed << " Mem"
                << this->m_bucketManager->usedMemory() << std::endl;
    }

  } // pushinhashtable

  /**
   * @brief Research in the set of buckets if the bucket pointed by i already
   * exist.
   *
   * @param cb is the bucket we are looking for.
   * @param hashValue is the hash value computed from the bucket.
   * @return a valid entry if it is in the cache, null otherwise.
   */
  CachedBucket<T> *bucketAlreadyExist(CachedBucket<T> &cb, unsigned hashValue) {
    char *refData = cb.data;
    std::vector<Node> &listCollision = hashTable[hashValue % SIZE_HASH];
    for (auto &cbi : listCollision) {
      if (!cb.sameHeader(cbi.bucket))
        continue;

      if (!memcmp(refData, cbi.bucket.data, cbi.bucket.szData())) {
        this->m_nbPositiveHit++;
        cbi.last_use = this->counter;
        return &cbi.bucket;
      }
    }

    this->m_nbNegativeHit++;
    return NULL;
  } // bucketAlreadyExist

  /**
   * Create a bucket and store it in the cache.
   *
   * @param varConnected is the set of variable.
   * @param c is the value we want to store.
   */
  inline void createAndStoreBucket(std::vector<Var> &varConnected, T &c) {
    CachedBucket<T> *formulaBucket =
        this->m_bucketManager->collectBuckect(varConnected);
    unsigned int hashValue = computeHash(*formulaBucket);
    pushInHashTable(*formulaBucket, hashValue, c);
  } // createBucket

  /**
   * @brief Init the hashTable.
   *
   * @param maxVar is the number of variable.
   */
  void initHashTable(unsigned maxVar) override {
    this->setInfoFormula(maxVar);
    // init hash tables
    hashTable.clear();
    hashTable.resize(SIZE_HASH, {});
  } // initHashTable

  /**
   * @brief Clean up the cache.
   *
   * @param test is a function that is used to decide if an entry must be
   * removed.
   *
   * @return the number of entry we removed.
   */
  unsigned removeEntry(std::function<bool(CachedBucket<T> &c)> test) {
    unsigned nbRemoveEntry = 0;
    for (auto &list : hashTable) {
      unsigned j = 0;
      for (unsigned i = 0; i < list.size(); i++) {
        CachedBucket<T> &cb = list[i].bucket;

        if (test(cb)) {
          assert((int)cb.szData() > 0);
          this->releaseMemory(cb.data, cb.szData());
          cb.reset();
          nbRemoveEntry++;
        } else
          list[j++] = list[i];
      }
      list.resize(j);
    }
    return nbRemoveEntry;
  } // removeEntry
};

/**
 *
 */
template <class T> class CacheListProbLRU : public CacheManager<T> {
private:
  struct Node {
    unsigned key;
    unsigned last_use;
    T data;
  };
  std::vector<uint64_t> key_mem;
  unsigned key_cnt = 0;
  std::vector<unsigned> free_keys;
  struct Hasher {
    const CacheListProbLRU<T> *owner;
    size_t operator()(const Node &n) const { return owner->get_key(n.key)[0]; }
  };

  friend struct Hasher;
  struct KeyEq {
    const CacheListProbLRU<T> *owner;
    bool operator()(const Node &lhs, const Node &rhs) const {
      return !memcmp(owner->get_key(lhs.key), owner->get_key(rhs.key),
                     owner->key_len * sizeof(uint64_t));
    }
  };
  friend struct KeyEq;
  spp::sparse_hash_set<Node, Hasher, KeyEq> table;
  size_t max_mem = (1ull << 30) * 8; // 8gb
  unsigned max_elmts;
  unsigned key_len = 2;
  unsigned time = 0;
  unsigned min_time = 0;
  static std::mt19937_64 gen;
  CLHasher hasher;
  CachedBucket<T> tmp;

public:
  const uint64_t *get_key(unsigned i) const {
    size_t k = size_t(i) * key_len;
    return key_mem.data() + k;
  }
  void destroy_key(unsigned i) { free_keys.push_back(i); }
  std::pair<unsigned, uint64_t *> create_key() {
    if (free_keys.size()) {
      unsigned idx = free_keys.back();
      free_keys.pop_back();
      return {idx, key_mem.data() + idx * key_len};
    } else {
      if (key_cnt == key_mem.size() / key_len) {
        std::cout << "Realloc keys" << std::endl;
        key_mem.resize(key_mem.size() * 1.5);
      }
      unsigned idx = key_cnt++;
      return {idx, key_mem.data() + idx * key_len};
    }
  }
  /**
   * @brief Construct a new Cache List object
   *
   * @param vm is a map to get the option.
   * @param nbVar is the number of variables.
   * @param specs is a structure to get data about the formula.
   * @param out is the stream where are printed out the logs.
   */
  CacheListProbLRU(po::variables_map &vm, unsigned nbVar, SpecManager *specs,
                   std::ostream &out)
      : CacheManager<T>(vm, nbVar, specs, out),
        table(0, Hasher{this}, KeyEq{this}) {
    out << "c [CACHE LIST LRU PROB CONSTRUCTOR]\n";
    max_mem = (1ull << 30ull) * vm["cache-fixed-size"].as<int>();
    max_elmts = max_mem /
                (sizeof(unsigned) + sizeof(uint64_t) * key_len + sizeof(Node));
    key_mem.resize(max_elmts * 1.5);
    out << "[CACH LIST LRU ]Fixed Cache: " << max_elmts << std::endl;
    initHashTable(nbVar);
    hasher.init(gen, key_len);

  } // constructor
  CacheListProbLRU(const CacheListProbLRU &) = delete;

  CacheListProbLRU &operator=(const CacheListProbLRU &) = delete;

  /**
   * @brief Destroy the Cache List object
   */
  ~CacheListProbLRU() {} // destructor

  /**
   * @brief Add an entry in the cache.
   *
   * @param cb is the bucket we want to add.
   * @param hashValue is the hash value of the bucket.
   * @param val is the value we associate with the bucket.
   */
  size_t computeHash(CachedBucket<T> &bucket) final {
    auto [id, key] = create_key();
    hasher.Hash(bucket.data, bucket.szData(), key);
    this->m_bucketManager->releaseMemory(bucket.data, bucket.szData());
    bucket.data = nullptr;
    return id;
  }
  inline void pushInHashTable(CachedBucket<T> &_cb, unsigned hashValue, T val) {
    table.insert(Node{hashValue, time, val});
    this->m_nbCreationBucket++;
    this->m_sumDataSize += 1;
    this->m_nbEntry++;
    this->time++;
    if (table.size() > max_elmts) {
      std::cout << "Start clean " << table.size() << std::endl;

      if (table.size() < 8) {
        return;
      }
      std::array<unsigned, 256> hist = {};
      double dist = time - min_time;
      for (auto &b : table) {
        unsigned rtime = b.last_use - min_time;
        unsigned id = (rtime / dist) * hist.size();
        hist[id]++;
      }
      unsigned acc = 0;
      unsigned i = 0;
      while (acc < table.size() / 2 && i < hist.size()) {
        acc += hist[i];
        i++;
      }
      size_t median = (i * (dist / 256)) + min_time;
      size_t removed = 0;
      for (auto it = table.begin(); it != table.end();) {
        if (it->last_use < median) {
          destroy_key(it->key);
          it = table.erase(it);
          removed++;
        } else {
          it++;
        }
      }
      min_time = median;
      std::cout << "Removed: " << removed << " Cnt" << table.size()
                << std::endl;
    }

  } // pushinhashtable

  /**
   * @brief Research in the set of buckets if the bucket pointed by i already
   * exist.
   *
   * @param cb is the bucket we are looking for.
   * @param hashValue is the hash value computed from the bucket.
   * @return a valid entry if it is in the cache, null otherwise.
   */
  CachedBucket<T> *bucketAlreadyExist(CachedBucket<T> &_cb,
                                      unsigned hashValue) {
    auto it = table.find(Node{hashValue});
    if (it == table.end()) {
      this->m_nbNegativeHit++;
      return 0;
    } else {
      // remove const ...
      *((unsigned *)&it->last_use) = time;
      tmp.fc = it->data;

      this->m_nbPositiveHit++;

      destroy_key(hashValue);

      return &tmp;
    }
    return NULL;
  } // bucketAlreadyExist

  /**
   * Create a bucket and store it in the cache.
   *
   * @param varConnected is the set of variable.
   * @param c is the value we want to store.
   */
  inline void createAndStoreBucket(std::vector<Var> &varConnected, T &c) {
    assert(0);
  } // createBucket

  /**
   * @brief Init the hashTable.
   *
   * @param maxVar is the number of variable.
   */
  void initHashTable(unsigned maxVar) override {
    this->setInfoFormula(maxVar);
  } // initHashTable

  /**
   * @brief Clean up the cache.
   *
   * @param test is a function that is used to decide if an entry must be
   * removed.
   *
   * @return the number of entry we removed.
   */
  unsigned removeEntry(std::function<bool(CachedBucket<T> &c)> test) {
    assert(0);
  } // removeEntry
};

template <typename T>
std::mt19937_64 CacheListProbLRU<T>::gen = std::mt19937_64(43);

} // namespace d4
  //
