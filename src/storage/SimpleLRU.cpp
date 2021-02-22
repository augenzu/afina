#include <utility>

#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) { 
    return PutIfAbsent(key, value) || Set(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    auto search = _lru_index.find(key);

    if (search != _lru_index.end()
            || key.size() + value.size() > _max_size) {
        return false;
    }

    size_t new_node_size = key.size() + value.size();

    while (_size + new_node_size > _max_size) {
        Delete(_lru_tail->key);
    }

    // create new node right in the head

    auto head_holder = std::move(_lru_head);
    _lru_head = std::unique_ptr<lru_node>(new lru_node{ 
            key, value, nullptr, std::move(head_holder) 
    });

    auto next = _lru_head->next.get();

    if (next) {
        next->prev = _lru_head.get();
    } else {
        _lru_tail = _lru_head.get();
    }

    _lru_index.emplace(std::make_pair(std::ref(_lru_head->key), std::ref(*_lru_head)));
    _size += new_node_size;

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    auto search = _lru_index.find(key);

    if (search == _lru_index.end()
            || key.size() + value.size() > _max_size) {
        return false;
    }

    auto node = &search->second.get();

    size_t old_value_size = node->value.size();
    size_t new_value_size = value.size();

    while (_size - old_value_size + new_value_size > _max_size) {
        if (node != _lru_tail) {
            Delete(_lru_tail->key);
        } else {
            Delete(_lru_head->prev->key);
        }
    }

    node->value = value;
    _size += new_value_size - old_value_size;

    MoveNodeToHead(node);

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto search = _lru_index.find(key);

    if (search == _lru_index.end()) {
        return false;
    }

    auto node = &search->second.get();

    // delete from index
    _lru_index.erase(search);

    std::unique_ptr<lru_node> holder_to_die;

    // delete from list
    if (_lru_head.get() == _lru_tail) {  // node is the only one element
        holder_to_die = std::move(_lru_head);
        _lru_tail = nullptr;
    } else if (node == _lru_head.get()) {  // node is the first element
        holder_to_die = std::move(_lru_head);
        _lru_head = std::move(node->next);
        _lru_head->prev = nullptr;
    } else if (node == _lru_tail) {  // node is the last element
        auto prev = node->prev;
        holder_to_die = std::move(prev->next);
        _lru_tail = prev;
    } else {  // node has non-null prev & next
        auto prev = node->prev;
        holder_to_die = std::move(prev->next);
        auto next = node->next.get();
        next->prev = prev;
        prev->next = std::move(node->next);
    }

    _size -= (node->key.size() + node->value.size());

    return true; 
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto search = _lru_index.find(key);

    if (search == _lru_index.end()) {
        return false;
    }

    auto node = &search->second.get();
    MoveNodeToHead(node);

    value = node->value;

    return true;
}

void SimpleLRU::MoveNodeToHead(lru_node *node) {
    if (node == _lru_head.get()) {
        return;
    }

    auto prev = node->prev;
    auto tmp_holder = std::move(prev->next);

    auto next = node->next.get();

    if (node == _lru_tail) {
        _lru_tail = prev;
    } else {
        next->prev = prev;
    }

    prev->next = std::move(node->next);

    node->prev = nullptr;
    _lru_head->prev = node;
    node->next = std::move(_lru_head);
    _lru_head = std::move(tmp_holder);
}

} // namespace Backend
} // namespace Afina
