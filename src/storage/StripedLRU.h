#ifndef AFINA_STORAGE_STRIPED_LRU_H
#define AFINA_STORAGE_STRIPED_LRU_H

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <unistd.h>
#include <vector>

#include "ThreadSafeSimpleLRU.h"
#include <afina/Storage.h>

namespace Afina {
namespace Backend {

class StripedLRU : public Afina::Storage {
public:

    friend StripedLRU* Make_stor(std::size_t count, size_t max_size);
     ~StripedLRU() {}
    bool Put(const std::string &key, const std::string &value) override {
        return shards[hash(key) % count]->Put(key, value);
    }

    bool PutIfAbsent(const std::string &key, const std::string &value) override {
        return shards[hash(key) % count]->PutIfAbsent(key, value);
    }

    bool Set(const std::string &key, const std::string &value) override {
        return shards[hash(key) % count]->Set(key, value);
    }

    bool Delete(const std::string &key) override {
        return shards[hash(key) % count]->Delete(key);
    }

    bool Get(const std::string &key, std::string &value) override {
        return shards[hash(key) % count]->Get(key, value);
    }

private:
    std::size_t count;
    std::vector<std::unique_ptr<ThreadSafeSimplLRU>> shards;
    std::hash<std::string> hash;
    StripedLRU(std::size_t count, size_t str_max_size)
        : count(count)
    {
        for (std::size_t i = 0; i < count; ++i) {
            shards.emplace_back(new ThreadSafeSimplLRU(str_max_size));
        }
    }
   
};
    StripedLRU* Make_stor(std::size_t count, std::size_t max_size = 2*1024*1024)
    {
        std::size_t limit = max_size / count;
        if (limit < 2*1024*1024){
            throw std::runtime_error("Small storage!");
        }
        return new StripedLRU(count, limit);
    }

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_THREAD_SAFE_STRIPED_LRU_H
