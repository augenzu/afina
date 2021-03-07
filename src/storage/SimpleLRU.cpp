#include <utility>

#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) { 
    if (key.size() + value.size() > _max_size) {
        return false;
    }

    auto search = _lru_index.find(key);

    if (search == _lru_index.end()) {
        PutIfDefinitelyAbsent(key, value);
        return true;
    } else {
        SetNode(key, value, search);
        return true;
    }
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    if (key.size() + value.size() > _max_size) {
        return false;
    }

    auto search = _lru_index.find(key);
    if (search != _lru_index.end()) {
        return false;
    }

    PutIfDefinitelyAbsent(key, value);
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    if (key.size() + value.size() > _max_size) {
        return false;
    }

    auto search = _lru_index.find(key);
    if (search == _lru_index.end()) {
        return false;
    }
        
    SetNode(key, value, search);
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

void SimpleLRU::PutIfDefinitelyAbsent(const std::string &key, const std::string &value) {
    size_t new_node_size = key.size() + value.size();

    while (_size + new_node_size > _max_size) {
        DeleteTailNode();
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
}

void SimpleLRU::SetNode(const std::string &key,
        const std::string &value,
        decltype(_lru_index)::iterator node_it) {
    auto node = &node_it->second.get();
    MoveNodeToHead(node);

    size_t old_value_size = node->value.size();
    size_t new_value_size = value.size();

    while (_size - old_value_size + new_value_size > _max_size) {
        DeleteTailNode();
    }

    node->value = value;
    _size += new_value_size - old_value_size;
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

void SimpleLRU::DeleteTailNode(/*std::map<std::reference_wrapper<const std::string>, 
            std::reference_wrapper<lru_node>, 
            std::less<const std::string>>::const_iterator tail_it*/) {
    if (_lru_head.get() == nullptr) {  // empty list hence no node to delete
        return;
    }

    auto search = _lru_index.find(_lru_tail->key);

    //remove from index
    _lru_index.erase(search);
    _size -= (_lru_tail->key.size() + _lru_tail->value.size());

    std::unique_ptr<lru_node> holder_to_die;

    if (_lru_tail == _lru_head.get()) {  // list contains only one node
        holder_to_die = std::move(_lru_head);
        _lru_tail = nullptr;
    } else {
        auto prev = _lru_tail->prev;
        holder_to_die = std::move(prev->next);
        _lru_tail = prev;
    }
}

} // namespace Backend
} // namespace Afina
