#ifndef TYPEDBENTRY_H
#define TYPEDBENTRY_H

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
                              const std::set<size_t> & _reqs);

    static TypeDBEntry mkFwdDecl(const std::string & _name,
                                 const std::string & _kind);

    static TypeDBEntry mkInclude(const std::string & _name,
                                 const std::string & _header);
    
    std::string name() const { return m_name; }
    std::string text() const { return m_text; }
    bool in_include_file() const { return m_is_include; }
    size_t hash() const { return m_hash; }
    std::string hash_as_str() const;

    picojson::value toJSON() const;
    static picojson::array databaseToJSON();
    
private:
    void compute_hash();

    static std::map<size_t, TypeDBEntry> type_db;
    
    std::string m_name;
    std::string m_text;
    size_t      m_hash;
    bool        m_is_include;
    std::set<size_t> m_reqs;
};

size_t hash_type(const clang::Type * t,
                 clang::CompilerInstance * ci);

} // end namespace clang_mutate

#endif
