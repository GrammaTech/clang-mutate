
#include "TypeDBEntry.h"
#include "Utils.h"

#include "clang/Lex/Lexer.h"
#include "clang/Lex/Preprocessor.h"

using namespace clang_mutate;
using namespace clang;

std::map<Hash, TypeDBEntry> TypeDBEntry::type_db;

TypeDBEntry TypeDBEntry::mkType(const std::string & _name,
                                const bool & _pointer,
                                const std::string & _array_size,
                                const std::string & _text,
                                const std::string & _file,
                                const unsigned int & _line,
                                const unsigned int & _col,
                                const std::set<Hash> & _reqs)
{
    TypeDBEntry ti;
    ti.m_name = _name;
    ti.m_pointer = _pointer;
    ti.m_array_size = _array_size;
    ti.m_text = _text;
    ti.m_file = _file;
    ti.m_line = _line;
    ti.m_col = _col;
    ti.m_reqs = _reqs;
    ti.m_is_include = false;
    ti.compute_hash();
    return ti;
}

TypeDBEntry TypeDBEntry::mkFwdDecl(const std::string & _name,
                                   const bool & _pointer,
                                   const std::string & _array_size,
                                   const std::string & _kind,
                                   const std::string & _file,
                                   const unsigned int & _line,
                                   const unsigned int & _col)
{
    TypeDBEntry ti;
    ti.m_name = _name;
    ti.m_pointer = _pointer;
    ti.m_array_size = _array_size;
    ti.m_text = _kind;
    ti.m_text += " ";
    ti.m_text += _name;
    ti.m_text += ";";
    ti.m_file = _file;
    ti.m_line = _line;
    ti.m_col = _col;
    ti.m_is_include = false;
    ti.compute_hash();
    return ti;
}

TypeDBEntry TypeDBEntry::mkInclude(const std::string & _name,
                                   const bool & _pointer,
                                   const std::string & _array_size,
                                   const std::string & _header,
                                   const std::string & _file,
                                   const unsigned int & _line,
                                   const unsigned int & _col,
                                   const std::vector<std::string> & _ifile,
                                   const std::vector<unsigned int> & _iline,
                                   const std::vector<unsigned int> & _icol)
    
{
    TypeDBEntry ti;
    ti.m_is_include = true;
    ti.m_name = _name;
    ti.m_pointer = _pointer;
    ti.m_array_size = _array_size;
    ti.m_text = _header;
    ti.m_file = _file;
    ti.m_line = _line;
    ti.m_col = _col;
    ti.m_ifile = _ifile;
    ti.m_iline = _iline;
    ti.m_icol = _icol;
    ti.compute_hash();
    return ti;
}

void TypeDBEntry::compute_hash()
{
    std::hash<std::string> hasher;
    size_t h = hasher(m_name + m_text + m_array_size);
    if (m_pointer) h += 1;
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
    jsonObj["pointer"] = to_json(m_pointer);
    jsonObj["array"] = to_json(m_array_size);
    jsonObj["file"] = to_json(m_file);
    jsonObj["line"] = to_json(m_line);
    jsonObj["col"] = to_json(m_col);
    if (m_is_include) {
        jsonObj["i_file"] = to_json(m_ifile);
        jsonObj["i_line"] = to_json(m_iline);
        jsonObj["i_col"] = to_json(m_icol);
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

    // Check for pointer or array types, and if so we'll update the
    // name later.
    bool pointer = false;
    std::string size_mod = "";
    if (t->isPointerType()) {
        pointer = true;
        t = t->getPointeeType().getTypePtr();
    }
    if (t->isArrayType()) {
        switch(t->getAsArrayTypeUnsafe()->getSizeModifier()){
        case ArrayType::Normal : size_mod = "[]";
        case ArrayType::Static : size_mod = "[static]";
        case ArrayType::Star : size_mod = "[*]";
        }
        t = t->getAsArrayTypeUnsafe()->getElementType().getTypePtr();
    }

    std::map<const Type*, Hash>::iterator search = seen.find(t);
    if (search != seen.end())
        return search->second;

    // Now handle the normal type.
    if (t->getAs<TypedefType>()) {
        TypedefNameDecl * td = t->getAs<TypedefType>()->getDecl();
        
        std::string header;
        SourceManager & sm = ci->getSourceManager();
        SourceLocation loc = sm.getSpellingLoc(td->getSourceRange().getBegin());
        PresumedLoc beginLoc = sm.getPresumedLoc(loc);
        if (Utils::in_header(loc, ci, header)) {
            SourceLocation inc_loc = beginLoc.getIncludeLoc();
            PresumedLoc incLoc = sm.getPresumedLoc(inc_loc);
            // Build up lists.
            std::vector<std::string> ifiles;
            std::vector<unsigned int> ilines;
            std::vector<unsigned int> icols;
            while(incLoc.isValid()){
                ifiles.push_back(incLoc.getFilename());
                ilines.push_back(incLoc.getLine());
                icols.push_back(incLoc.getColumn());
                inc_loc = incLoc.getIncludeLoc();
                incLoc = sm.getPresumedLoc(inc_loc);
            }
            Hash hash = TypeDBEntry::mkInclude(
                td->getNameAsString(),
                pointer,
                size_mod,
                header,
                beginLoc.getFilename(),
                beginLoc.getLine(),
                beginLoc.getColumn(),
                ifiles,
                ilines,
                icols).hash();
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
                SourceRange r = td->getSourceRange();
                SourceManager & sm = ci->getSourceManager();
                SourceLocation it = r.getBegin();
                PresumedLoc beginLoc = sm.getPresumedLoc(sm.getSpellingLoc(it));
                Hash hash = TypeDBEntry::mkType(
                    name,
                    pointer,
                    size_mod,
                    one_line_name,
                    beginLoc.getFilename(),
                    beginLoc.getLine(),
                    beginLoc.getColumn(),
                    reqs).hash();
                seen[t] = hash;
                return hash;
            }
            else {
                SourceRange r = td->getSourceRange();
                SourceManager & sm = ci->getSourceManager();
                SourceLocation it = r.getBegin();
                SourceLocation end = r.getEnd();
                PresumedLoc beginLoc = sm.getPresumedLoc(sm.getSpellingLoc(it));
                end = end.getLocWithOffset(
                    Lexer::MeasureTokenLength(end, sm, ci->getLangOpts()));
                std::string text;
                while (it != end) {
                    text.push_back(*sm.getCharacterData(it));
                    it = it.getLocWithOffset(1);
                }
                text += ";";
        
                TypeDBEntry ti = TypeDBEntry::mkType(td->getNameAsString(),
                                                     pointer,
                                                     size_mod,
                                                     text,
                                                     beginLoc.getFilename(),
                                                     beginLoc.getLine(),
                                                     beginLoc.getColumn(),
                                                     reqs);
                seen[t] = ti.hash();
                return ti.hash();     
            }
        }
    }

    else if (t->getAs<BuiltinType>()) {
        std::vector<std::string> ifiles;
        std::vector<unsigned int> ilines;
        std::vector<unsigned int> icols;
        std::string name =
            t->getAs<BuiltinType>()->getName(PrintingPolicy(ci->getLangOpts()));
        Hash hash = TypeDBEntry::mkInclude(
            name,
            pointer,
            size_mod,
            "",
            "",
            0,
            0,
            ifiles,
            ilines,
            icols).hash();
        seen[t] = hash;
        return hash;
    }
    
    else if (t->isStructureType() || t->isUnionType()) {
        const RecordType * rt = t->isStructureType() ? 
                                t->getAsStructureType() :
                                t->getAsUnionType();
        RecordDecl * decl = rt->getDecl();
        RecordDecl * rd = decl->getDefinition();

        // If there's no definition (e.g. from typedef struct Foo * FooPtr),
        // just use the declaration. That at least gives us the name.
        if (rd == NULL)
            rd = decl;

        std::set<Hash> reqs;

        // Forward-declare the struct/class if it is not anonymous
        
        if (rd->getNameAsString() != "") {
            SourceManager & sm = ci->getSourceManager();
            PresumedLoc beginLoc =
                sm.getPresumedLoc(rd->getSourceRange().getBegin());

            TypeDBEntry fdecl = TypeDBEntry::mkFwdDecl(
                rd->getNameAsString(),
                pointer,
                size_mod,
                (t->isStructureType() ? "struct" : "union"),
                beginLoc.getFilename(),
                beginLoc.getLine(),
                beginLoc.getColumn());
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
        PresumedLoc beginLoc = sm.getPresumedLoc(r.getBegin());
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
                                             pointer,
                                             size_mod,
                                             text,
                                             beginLoc.getFilename(),
                                             beginLoc.getLine(),
                                             beginLoc.getColumn(),
                                             reqs);
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
