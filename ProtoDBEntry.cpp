#include "ProtoDBEntry.h"
#include "Utils.h"

using namespace clang_mutate;
using namespace Utils;

std::map<std::string, ProtoDBEntry> ProtoDBEntry::proto_db;

ProtoDBEntry::ProtoDBEntry(
    const std::string & name,
    const std::string & text,
    unsigned int body,
    size_t ret,
    bool ret_void,
    const std::vector<Argument> & args,
    bool has_varargs)
    : m_name(name)
    , m_text(text)
    , m_body(body)
    , m_ret(ret)
    , m_ret_void(ret_void)
    , m_args(args)
    , m_has_varargs(has_varargs)
{
    register_proto();
}

picojson::value ProtoDBEntry::toJSON() const
{
    picojson::object jsonObj;

    jsonObj["name"] = picojson::value(m_name);
    jsonObj["text"] = picojson::value(m_text);
    jsonObj["body"] = picojson::value(static_cast<int64_t>(m_body));
    jsonObj["ret"] = picojson::value(hash_to_str(m_ret));
    jsonObj["void_ret"] = picojson::value(m_ret_void);

    std::vector<picojson::value> args;
    for (std::vector<Argument>::const_iterator
             it = m_args.begin(); it != m_args.end(); ++it)
    {
        std::vector<picojson::value> arg;
        arg.push_back(picojson::value(it->first));
        arg.push_back(picojson::value(hash_to_str(it->second)));
        args.push_back(picojson::value(arg));
    }
    jsonObj["args"] = picojson::value(args);
    jsonObj["varargs"] = picojson::value(m_has_varargs);
    
    return picojson::value(jsonObj);
}

void ProtoDBEntry::register_proto()
{
    if (proto_db.find(m_name) == proto_db.end())
        proto_db[m_name] = *this;
}

picojson::array ProtoDBEntry::databaseToJSON()
{
    picojson::array array;
    for (std::map<std::string, ProtoDBEntry>::iterator
             it = proto_db.begin(); it != proto_db.end(); ++it)
    {
        array.push_back(it->second.toJSON());
    }
    return array;
}
