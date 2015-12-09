
#include "TypeDBEntry.h"

#include "clang/Lex/Lexer.h"

#include <sstream>

using namespace clang_mutate;
using namespace clang;

std::map<size_t, TypeDBEntry> TypeDBEntry::type_db;

TypeDBEntry TypeDBEntry::mkType(const std::string & _name,
                                const std::string & _text,
                                const std::set<size_t> & _reqs)
{
    TypeDBEntry ti;
    ti.m_name = _name;
    ti.m_text = _text;
    ti.m_reqs = _reqs;
    ti.m_is_include = false;
    ti.compute_hash();
    return ti;
}

TypeDBEntry TypeDBEntry::mkFwdDecl(const std::string & _name,
                                   const std::string & _kind)
{
    TypeDBEntry ti;
    ti.m_name = _name;
    ti.m_text = _kind;
    ti.m_text += " ";
    ti.m_text += _name;
    ti.m_text += ";";
    ti.m_is_include = false;
    ti.compute_hash();
    return ti;
}

TypeDBEntry TypeDBEntry::mkInclude(const std::string & _name,
                                   const std::string & _header)
    
{
    TypeDBEntry ti;
    ti.m_is_include = true;
    ti.m_name = _name;
    ti.m_text = _header;
    ti.compute_hash();
    return ti;
}

void TypeDBEntry::compute_hash()
{
    std::hash<std::string> hasher;
    m_hash = hasher(m_name + m_text);
    for (std::set<size_t>::iterator it = m_reqs.begin();
         it != m_reqs.end();
         ++it)
    {
        // C++11 decided to add a std::hash, but not a
        // std::hash_combine?! This is the 64-bit combiner
        // from Google's CityHash, as in
        // http://stackoverflow.com/questions/8513911/how-to-create-a-good-hash-combine-with-64-bit-output-inspired-by-boosthash-co
        const size_t kMul = 0x9ddfea08eb382d69ULL;
        size_t a = (*it ^ m_hash) * kMul;
        a ^= (a >> 47);
        size_t b = (m_hash ^ a) * kMul;
        b ^= (b >> 47);
        m_hash = b * kMul;
    }
    
    if (type_db.find(m_hash) != type_db.end())
        return;
    type_db[m_hash] = *this;
}

static std::string hash_to_str(size_t hash)
{
    std::ostringstream ss;
    ss << std::hex << hash << std::dec;
    return ss.str();
}

std::string TypeDBEntry::hash_as_str() const
{ return hash_to_str(m_hash); }

picojson::value TypeDBEntry::toJSON() const
{
    picojson::object jsonObj;

    std::vector<picojson::value> j_reqs;
    for (std::set<size_t>::iterator it = m_reqs.begin();
         it != m_reqs.end();
         ++it)
    {
        if (*it == 0)
            continue;
        j_reqs.push_back(picojson::value(hash_to_str(*it)));
    }
    
    jsonObj["hash"] = picojson::value(hash_as_str());
    jsonObj["type"] = picojson::value(escape_for_json(m_name));
    if (m_is_include) {
        jsonObj["include"] = picojson::value(escape_for_json(m_text));
    }
    else {
        jsonObj["decl"] = picojson::value(escape_for_json(m_text));
    }
    jsonObj["reqs"] = picojson::value(j_reqs);
    
    return picojson::value(jsonObj);
}

picojson::array TypeDBEntry::databaseToJSON()
{
    picojson::array array;
    for (std::map<size_t, TypeDBEntry>::iterator it = type_db.begin();
         it != type_db.end();
         ++it)
    {
        array.push_back(it->second.toJSON());
    }
    return array;
}

static bool is_system_header(
    const std::string & fullpath,
    std::string & header)
{
    static std::vector<std::string> prefixes;
    if (prefixes.empty()) {
        prefixes.push_back("/usr/include/x86_64-linux-gnu/");
        prefixes.push_back("/usr/include/");
        prefixes.push_back("/usr/lib/gcc/x86_64-linux-gnu/4.9/include/");
    }

    for (std::vector<std::string>::iterator prefix = prefixes.begin();
         prefix != prefixes.end();
         ++prefix)
    {
        if (fullpath.find(*prefix) == 0) {
            header = fullpath.substr(prefix->size());
            return true;
        }
    }

    return false;
}

static size_t define_type(
    const Type * t,
    std::map<const Type*, size_t> & seen,
    CompilerInstance * ci)
{
    if (t == NULL)
        return 0;
    
    std::map<const Type*, size_t>::iterator search = seen.find(t);
    if (search != seen.end())
        return search->second;

    if (t->getAs<TypedefType>()) {
        TypedefNameDecl * td = t->getAs<TypedefType>()->getDecl();
        
        std::string header;
        SourceManager & sm = ci->getSourceManager();
        SourceLocation loc = sm.getSpellingLoc(td->getSourceRange().getBegin());
        if (is_system_header(sm.getFilename(loc).str(), header)) {
            size_t hash = TypeDBEntry::mkInclude(td->getNameAsString(), header).hash();
            seen[t] = hash;
            return hash;
        }
        else {
            // User-defined typedef
            const Type * u_tt = td->getUnderlyingType().getTypePtr();

            // Emit underlying type
            std::set<size_t> reqs;
            reqs.insert(define_type(u_tt, seen, ci));

            // Emit the typedef itself
            // Exception: in the case of an underlying composite type,
            //  just emit a one-line typedef so that we don't repeat
            //  the definition of the struct/class/union itself.
            //  Exception to the exception: If the underlying struct
            //   is anonymous, we must emit the type here after all!
            bool one_line = false;
            std::string one_line_name, name;
            if (u_tt->isRecordType()) {
                one_line = true;
                const RecordDecl * rd = NULL;
                if (u_tt->isStructureType()) {
                    rd = u_tt->getAsStructureType()
                             ->getDecl()->getDefinition();
                    one_line_name = "typedef struct ";
                }
                else if (u_tt->isUnionType()) {
                    rd = u_tt->getAsUnionType()
                             ->getDecl()->getDefinition();
                    one_line_name = "typedef union ";
                }

                // TODO: C++ class/struct/union?

                name = td->getNameAsString();
                if (rd == NULL || rd->getNameAsString() == "") {
                    one_line = false;
                    one_line_name = "";
                }
                else {
                    one_line = true;
                    one_line_name += rd->getNameAsString();
                    one_line_name += " ";
                    one_line_name += name;
                    one_line_name += ";";
                }
            }
            if (one_line) {
                size_t hash = TypeDBEntry::mkType(name, one_line_name, reqs).hash();
                seen[t] = hash;
                return hash;
            }
            else {
                SourceRange r = td->getSourceRange();
                SourceManager & sm = ci->getSourceManager();
                SourceLocation it = r.getBegin();
                SourceLocation end = r.getEnd();
                end = end.getLocWithOffset(
                    Lexer::MeasureTokenLength(end, sm, ci->getLangOpts()));
                std::string text;
                while (it != end) {
                    text.push_back(*sm.getCharacterData(it));
                    it = it.getLocWithOffset(1);
                }
                text += ";";
        
                TypeDBEntry ti = TypeDBEntry::mkType(td->getNameAsString(),
                                               text, reqs);
                seen[t] = ti.hash();
                return ti.hash();     
            }
        }
    }

    else if (t->isPointerType()) {
        // Just ensure that the pointed-to type is defined
        size_t hash = define_type(t->getPointeeType().getTypePtr(), seen, ci);
        seen[t] = hash;
        return hash;
    }

    else if (t->isArrayType()) {
        // Just ensure that the array's element type is defined.
        size_t hash = define_type(t->getAsArrayTypeUnsafe()
                                   ->getElementType().getTypePtr(),
                                  seen, ci);
        seen[t] = hash;
        return hash;
    }
    
    else if (t->isStructureType()) {
        const RecordType * rt = t->getAsStructureType();
        RecordDecl * rd = rt->getDecl()->getDefinition();

        std::set<size_t> reqs;

        // Forward-declare the struct/class if it is not anonymous
        
        if (rd->getNameAsString() != "") {
            TypeDBEntry fdecl = TypeDBEntry::mkFwdDecl(rd->getNameAsString(),
                                                 "struct");
            // Temporarily make this Type* reference the hash of the
            // forward declaration.
            seen[t] = fdecl.hash();
            reqs.insert(fdecl.hash());
        }
        
        // Define the member types
        for (RecordDecl::field_iterator it = rd->field_begin();
             it != rd->field_end();
             ++it)
        {
            size_t h = define_type(it->getType().getTypePtr(), seen, ci);
            reqs.insert(h);
        }

        // Emit the struct/class/union definition
        SourceRange r = rd->getSourceRange();
        SourceManager & sm = ci->getSourceManager();
        SourceLocation end = r.getEnd();
        end = end.getLocWithOffset(
            Lexer::MeasureTokenLength(end, sm, ci->getLangOpts()));

        std::string text;
        for (SourceLocation it = r.getBegin();
             it != end;
             it = it.getLocWithOffset(1))
        {
            text.push_back(*sm.getCharacterData(it));
        }
        text += ";";
        
        TypeDBEntry ti = TypeDBEntry::mkType(rd->getNameAsString(),
                                       text, reqs);
        seen[t] = ti.hash();
        return ti.hash();
    }

    return 0;
}

size_t clang_mutate::hash_type(const Type * t, CompilerInstance * ci)
{
    std::map<const Type*, size_t> seen;
    return define_type(t, seen, ci);
}
