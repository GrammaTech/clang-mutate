#include "AuxDB.h"

using namespace clang_mutate;

AuxDB::AuxDatabase AuxDB::aux_db;

picojson::value AuxDBEntry::toJSON() const
{ return to_json(m_obj); }

picojson::array AuxDB::toJSON()
{
    picojson::array ans;
    for (AuxDatabase::iterator it = aux_db.begin(); it != aux_db.end(); ++it)
        ans.push_back(to_json(it->second));
    return ans;
}

AuxDBEntry& AuxDB::create(const std::string & name)
{
    AuxDatabase::iterator search = aux_db.find(name);
    if (search != aux_db.end())
        return search->second;
    return (aux_db[name] = AuxDBEntry());
}
