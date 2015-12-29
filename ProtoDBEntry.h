#ifndef PROTODBENTRY_H
#define PROTODBENTRY_H

#include "Json.h"

namespace clang_mutate {

class ProtoDBEntry
{
public:
    typedef std::pair<std::string, size_t> Argument;

    ProtoDBEntry() {}
    
    ProtoDBEntry(const std::string & _name,
                 const std::string & _text,
                 unsigned int _body,
                 size_t _ret,
                 bool m_ret_void,
                 const std::vector<Argument> & _args,
                 bool _has_varargs);

    picojson::value toJSON() const;
    static picojson::array databaseToJSON();

private:
    void register_proto();
    
    static std::map<std::string, ProtoDBEntry> proto_db;

    std::string   m_name;
    std::string   m_text;
    unsigned int  m_body;
    size_t m_ret;
    bool m_ret_void;
    std::vector<Argument> m_args;
    bool          m_has_varargs;
};
    
}

#endif
