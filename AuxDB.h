#ifndef AUX_DB_ENTRY_H
#define AUX_DB_ENTRY_H

#include "Json.h"

#include <map>
#include <string>

namespace clang_mutate
{

class AuxDBEntry
{
public:

    picojson::value toJSON() const;

    template <typename T>
    AuxDBEntry& set(const std::string & key,
                    const T & value)
    {
        m_obj[key] = to_json(value);
        return *this;
    }
    
private:
    std::map<std::string, picojson::value> m_obj;
};

class AuxDB
{
public:
    static picojson::array toJSON();
    static AuxDBEntry& create(const std::string & name);
private:
    typedef std::map<std::string, AuxDBEntry> AuxDatabase;
    static AuxDatabase aux_db;
};

} // end namespace clang_mutate

template <> inline
picojson::value to_json(const clang_mutate::AuxDBEntry& aux)
{ return aux.toJSON(); }

#endif
