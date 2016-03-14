#ifndef CLANG_MUTATE_HASH_H
#define CLANG_MUTATE_HASH_H

#include "Json.h"

class Hash
{
public:
    Hash() : m_hash(0) {}
    Hash(size_t hash) : m_hash(hash) {}
    Hash(const Hash & h) : m_hash(h.m_hash) {}

    bool operator<(const Hash & h) const
    { return m_hash < h.m_hash; }

    bool operator<=(const Hash & h) const
    { return m_hash <= h.m_hash; }

    bool operator>(const Hash & h) const
    { return m_hash > h.m_hash; }

    bool operator>=(const Hash & h) const
    { return m_hash >= h.m_hash; }

    bool operator==(const Hash & h) const
    { return m_hash == h.m_hash; }

    bool operator!=(const Hash & h) const
    { return m_hash != h.m_hash; }

    picojson::value toJSON() const;

    size_t hash() const { return m_hash; }

private:
    size_t m_hash;
};

template <> inline
picojson::value to_json(const Hash & h)
{ return h.toJSON(); }

template <typename K, typename V>
std::vector<V> map_values(const std::map<K,V> & m)
{
    std::vector<V> ans;
    for (typename std::map<K,V>::const_iterator
             it = m.begin(); it != m.end(); ++it)
    {
        ans.push_back(it->second);
    }
    return ans;
}

#endif
