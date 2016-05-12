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
                              const std::string & _file,
                              const unsigned int & _line,
                              const unsigned int & _col,
                              const std::set<Hash> & _reqs);

    static TypeDBEntry mkFwdDecl(const std::string & _name,
                                 const std::string & _kind,
                                 const std::string & _file,
                                 const unsigned int & _line,
                                 const unsigned int & _col);

    static TypeDBEntry mkInclude(const std::string & _name,
                                 const std::string & _header,
                                 const std::string & _file,
                                 const unsigned int & _line,
                                 const unsigned int & _col,
                                 const std::vector<std::string> & _ifile,
                                 const std::vector<unsigned int> & _iline,
                                 const std::vector<unsigned int> & _icol);
    
    std::string name() const { return m_name; }
    std::string text() const { return m_text; }
    std::string file() const { return m_file; }
    unsigned int line() const { return m_line; }
    unsigned int col() const { return m_col; }
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
    std::string m_file;
    unsigned int m_line;
    unsigned int m_col;
    std::vector<std::string> m_ifile;
    std::vector<unsigned int> m_iline;
    std::vector<unsigned int> m_icol;
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
