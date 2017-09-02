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
                              const uint64_t & _size,
                              const bool & _pointer,
                              const bool & _const,
                              const bool & _volatile,
                              const bool & _restrict,
                              const std::string & _array_size,
                              const std::string & _text,
                              const std::string & _file,
                              const unsigned int & _line,
                              const unsigned int & _col,
                              const std::set<Hash> & _reqs);

    static TypeDBEntry mkFwdDecl(const std::string & _name,
                                 const bool & _pointer,
                                 const std::string & _array_size,
                                 const std::string & _kind,
                                 const std::string & _file,
                                 const unsigned int & _line,
                                 const unsigned int & _col);

    static TypeDBEntry mkInclude(const std::string & _name,
                                 const uint64_t & _size,
                                 const bool & _pointer,
                                 const bool & _const,
                                 const bool & _volatile,
                                 const bool & _restrict,
                                 const std::string & _array_size,
                                 const std::string & _file,
                                 const unsigned int & _line,
                                 const unsigned int & _col,
                                 const std::string & _ifile);

    static TypeDBEntry find_type(Hash hash);

    std::string name() const { return m_name; }
    uint64_t size() const { return m_size; }
    bool is_pointer() const { return m_pointer; }
    bool is_const() const { return m_const; }
    bool is_volatile() const { return m_volatile; }
    bool is_restrict() const { return m_restrict; }
    std::string array_size() const { return m_array_size; }
    std::string text() const { return m_text; }
    std::string file() const { return m_file; }
    unsigned int line() const { return m_line; }
    unsigned int col() const { return m_col; }
    std::string include_file() const { return m_ifile; }
    std::set<Hash> reqs() const { return m_reqs; }

    bool in_include_file() const { return !m_ifile.empty(); }

    Hash hash() const { return m_hash; }
    std::string hash_as_str() const;

    typedef TypeDBEntry Serializable;
    picojson::value toJSON() const;
    static picojson::array databaseToJSON();

private:
    void compute_hash();

    static std::map<Hash, TypeDBEntry> type_db;

    std::string m_name;
    uint64_t m_size;
    bool m_pointer;
    bool m_const;
    bool m_volatile;
    bool m_restrict;
    std::string m_array_size;
    std::string m_text;
    std::string m_file;
    unsigned int m_line;
    unsigned int m_col;
    std::string m_ifile;
    std::set<Hash> m_reqs;
    Hash        m_hash;
};

Hash hash_type(const clang::QualType & t,
               clang::CompilerInstance * ci,
               clang::ASTContext *context);

} // end namespace clang_mutate

template <> inline
picojson::value to_json(const clang_mutate::TypeDBEntry & entry)
{ return entry.toJSON(); }

#endif
