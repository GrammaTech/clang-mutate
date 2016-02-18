
#include "TypeDBEntry.h"
#include "Utils.h"

#include "clang/Lex/Lexer.h"
#include "clang/Lex/Preprocessor.h"

using namespace clang_mutate;
using namespace clang;

std::map<Hash, TypeDBEntry> TypeDBEntry::type_db;

TypeDBEntry TypeDBEntry::mkType(const std::string & _name,
                                const std::string & _text,
                                const std::set<Hash> & _reqs)
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
    size_t h = hasher(m_name + m_text);
    for (std::set<Hash>::iterator it = m_reqs.begin();
         it != m_reqs.end();
         ++it)
    {
        // C++11 decided to add a std::hash, but not a
        // std::hash_combine?! This is the 64-bit combiner
        // from Google's CityHash, as in
        // http://stackoverflow.com/questions/8513911/how-to-create-a-good-hash-combine-with-64-bit-output-inspired-by-boosthash-co
        const size_t kMul = 0x9ddfea08eb382d69ULL;
        size_t a = (it->hash() ^ h) * kMul;
        a ^= (a >> 47);
        size_t b = (h ^ a) * kMul;
        b ^= (b >> 47);
        h = b * kMul;
    }

    m_hash = h;
    if (type_db.find(m_hash) != type_db.end())
        return;
    type_db[m_hash] = *this;
}

picojson::value TypeDBEntry::toJSON() const
{
    picojson::object jsonObj;

    std::set<Hash> j_reqs = m_reqs;
    j_reqs.erase(Hash(0));
    
    jsonObj["hash"] = to_json(m_hash);
    jsonObj["type"] = to_json(m_name);
    if (m_is_include) {
        jsonObj["include"] = to_json(m_text);
    }
    else {
        jsonObj["decl"] = to_json(m_text);
    }
    jsonObj["reqs"] = to_json(j_reqs);
    
    return to_json(jsonObj);
}

picojson::array TypeDBEntry::databaseToJSON()
{
    picojson::array array;
    for (std::map<Hash, TypeDBEntry>::iterator it = type_db.begin();
         it != type_db.end();
         ++it)
    {
        array.push_back(to_json(it->second));
    }
    return array;
}

static Hash define_type(
    const Type * t,
    std::map<const Type*, Hash> & seen,
    CompilerInstance * ci)
{
    if (t == NULL)
        return 0;
    
    std::map<const Type*, Hash>::iterator search = seen.find(t);
    if (search != seen.end())
        return search->second;

    if (t->getAs<TypedefType>()) {
        TypedefNameDecl * td = t->getAs<TypedefType>()->getDecl();
        
        std::string header;
        SourceManager & sm = ci->getSourceManager();
        SourceLocation loc = sm.getSpellingLoc(td->getSourceRange().getBegin());
        if (Utils::in_system_header(loc, sm, header)) {
            Hash hash = TypeDBEntry::mkInclude(td->getNameAsString(), header).hash();
            seen[t] = hash;
            return hash;
        }
        else {
            // User-defined typedef
            const Type * u_tt = td->getUnderlyingType().getTypePtr();

            // Emit underlying type
            std::set<Hash> reqs;
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
                Hash hash = TypeDBEntry::mkType(name, one_line_name, reqs).hash();
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
        Hash hash = define_type(t->getPointeeType().getTypePtr(), seen, ci);
        seen[t] = hash;
        return hash;
    }

    else if (t->isArrayType()) {
        // Just ensure that the array's element type is defined.
        Hash hash = define_type(t->getAsArrayTypeUnsafe()
                                ->getElementType().getTypePtr(),
                                seen, ci);
        seen[t] = hash;
        return hash;
    }
    
    else if (t->isStructureType()) {
        const RecordType * rt = t->getAsStructureType();
        RecordDecl * decl = rt->getDecl();
        RecordDecl * rd = decl->getDefinition();

        // If there's no definition (e.g. from typedef struct Foo * FooPtr),
        // just use the declaration. That at least gives us the name.
        if (rd == NULL)
            rd = decl;

        std::set<Hash> reqs;

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
            Hash h = define_type(it->getType().getTypePtr(), seen, ci);
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

Hash clang_mutate::hash_type(const Type * t, CompilerInstance * ci)
{
    std::map<const Type*, Hash> seen;
    return define_type(t, seen, ci);
}
