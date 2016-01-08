#ifndef TYPEDBENTRY_H
#define TYPEDBENTRY_H

#include "Utils.h"
#include "Json.h"

#include "clang/Basic/LLVM.h"
#include "clang/Basic/SourceManager.h"
#include "clang/AST/AST.h"
#include "clang/Frontend/CompilerInstance.h"

#include <string>
#include <set>

namespace clang_mutate {
    
class TypeDBEntry
{
public:
    static TypeDBEntry mkType(const std::string & _name,
                              const std::string & _text,
                              const std::set<Hash> & _reqs);

    static TypeDBEntry mkFwdDecl(const std::string & _name,
                                 const std::string & _kind);

    static TypeDBEntry mkInclude(const std::string & _name,
                                 const std::string & _header);
    
    std::string name() const { return m_name; }
    std::string text() const { return m_text; }
    bool in_include_file() const { return m_is_include; }
    Hash hash() const { return m_hash; }
    std::string hash_as_str() const;

    typedef TypeDBEntry Serializable;
    picojson::value toJSON() const;
    static picojson::array databaseToJSON();
    
private:
    void compute_hash();

    static std::map<Hash, TypeDBEntry> type_db;
    
    std::string m_name;
    std::string m_text;
    Hash        m_hash;
    bool        m_is_include;
    std::set<Hash> m_reqs;
};

Hash hash_type(const clang::Type * t,
               clang::CompilerInstance * ci);

} // end namespace clang_mutate

template <> inline
picojson::value to_json(const clang_mutate::TypeDBEntry & entry)
{ return entry.toJSON(); }

#endif
