
#include "TypeDBEntry.h"
#include "Utils.h"

#include "clang/Lex/Lexer.h"
#include "clang/Lex/Preprocessor.h"

using namespace clang_mutate;
using namespace clang;

struct QualTypeComparator{
    bool operator()(const QualType &lhs, const QualType &rhs) {
        uintptr_t lx = (uintptr_t) lhs.getAsOpaquePtr();
        unsigned  ly = lhs.getLocalFastQualifiers();
        uintptr_t rx = (uintptr_t) rhs.getAsOpaquePtr();
        unsigned  ry = rhs.getLocalFastQualifiers();

        return ((((lx + ly) * (lx + ly + 1)) / 2) + ly) <
               ((((rx + ry) * (rx + ry + 1)) / 2) + ry);
    }
};
std::map<Hash, TypeDBEntry> TypeDBEntry::type_db;

TypeDBEntry TypeDBEntry::mkType(const std::string & _name,
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
                                const std::set<Hash> & _reqs)
{
    TypeDBEntry ti;
    ti.m_name = _name;
    ti.m_size = _size;
    ti.m_pointer = _pointer;
    ti.m_const = _const;
    ti.m_volatile = _volatile;
    ti.m_restrict = _restrict;
    ti.m_array_size = _array_size;
    ti.m_text = _text;
    ti.m_file = _file;
    ti.m_line = _line;
    ti.m_col = _col;
    ti.m_reqs = _reqs;
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
    ti.m_const = false;
    ti.m_volatile = false;
    ti.m_restrict = false;
    ti.m_array_size = _array_size;
    ti.m_text = _kind;
    ti.m_text += " ";
    ti.m_text += _name;
    ti.m_text += ";";
    ti.m_file = _file;
    ti.m_line = _line;
    ti.m_col = _col;
    ti.m_size = 0;
    ti.compute_hash();
    return ti;
}

TypeDBEntry TypeDBEntry::mkInclude(const std::string & _name,
                                   const uint64_t & _size,
                                   const bool & _pointer,
                                   const bool & _const,
                                   const bool & _volatile,
                                   const bool & _restrict,
                                   const std::string & _array_size,
                                   const std::string & _file,
                                   const unsigned int & _line,
                                   const unsigned int & _col,
                                   const std::string & _ifile)

{
    TypeDBEntry ti;
    ti.m_name = _name;
    ti.m_size = _size;
    ti.m_pointer = _pointer;
    ti.m_const = _const;
    ti.m_volatile = _volatile;
    ti.m_restrict = _restrict;
    ti.m_array_size = _array_size;
    ti.m_file = _file;
    ti.m_line = _line;
    ti.m_col = _col;
    ti.m_ifile = _ifile;
    ti.compute_hash();
    return ti;
}

TypeDBEntry TypeDBEntry::find_type(Hash hash)
{
    return type_db[hash];
}

void TypeDBEntry::compute_hash()
{
    std::hash<std::string> hasher;
    size_t h = hasher(m_name +
                      m_text +
                      m_array_size +
                      (m_pointer ? "pointer" : "") +
                      (m_const ? "const" : "") +
                      (m_volatile ? "volatile" : "") +
                      (m_restrict ? "restrict" : ""));

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
    jsonObj["const"] = to_json(m_const);
    jsonObj["volatile"] = to_json(m_volatile);
    jsonObj["restrict"] = to_json(m_restrict);
    jsonObj["array"] = to_json(m_array_size);
    jsonObj["file"] = to_json(m_file);
    jsonObj["line"] = to_json(m_line);
    jsonObj["col"] = to_json(m_col);
    if (!m_ifile.empty()) {
        jsonObj["i-file"] = to_json(m_ifile);
    }
    else {
        jsonObj["decl"] = to_json(m_text);
    }
    jsonObj["reqs"] = to_json(j_reqs);
    if (m_size != 0)
        jsonObj["size"] = to_json(m_size / 8);

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

static std::string safe_get_filename(PresumedLoc loc)
{
    if (loc.isValid())
        return loc.getFilename();
    else
        return "";
}

uint64_t get_type_size(ASTContext * context, const Type * t)
{
    // Can't get size of these kinds of types
    if (t->isIncompleteType() || t->isDependentType()
        || t->isUndeducedType())
        return 0;
    // Some builtin types will crash in getTypeSize().
    // For example '<bound member function type>', which can occur in C++
    // code. I have not found a better way to detect these, so instead
    // whitelist builtin types which we know are safe.
    const BuiltinType *bt = t->getAs<BuiltinType>();
    if (bt && !(bt->isInteger() || bt->isFloatingPoint()))
        return 0;
    return context->getTypeSize(t);
}

static Hash define_type(
    QualType t,
    CompilerInstance * ci,
    ASTContext *context)
{
    static std::map<QualType, Hash, QualTypeComparator> seen;

    if (t.getTypePtrOrNull() == NULL)
        return 0;

    bool is_pointer = false;
    bool is_const = t.isConstQualified();
    bool is_volatile = t.isVolatileQualified();
    bool is_restrict = t.isRestrictQualified();
    std::string size_mod = "";
    uint64_t size = get_type_size(context, t.getTypePtr());

    // Check for pointer or array types, and if so we'll update the
    // name later.
    while (t.getTypePtr()->isArrayType() || t.getTypePtr()->isPointerType()) {
        if (t.getTypePtr()->isArrayType()) {
            std::string len_str = "";
            if (isa<ConstantArrayType>(t.getTypePtr())) {
                uint64_t len =
                    static_cast<const ConstantArrayType *>(t.getTypePtr())
                                                          ->getSize()
                                                          .getLimitedValue();
                len_str = std::to_string(len);
            }

            switch(t.getTypePtr()->getAsArrayTypeUnsafe()->getSizeModifier()) {
            case ArrayType::Normal:
              size_mod += "[" + len_str + "]";
              break;
            case ArrayType::Static:
              size_mod += "[static " + len_str + "]";
              break;
            case ArrayType::Star:
              size_mod += "[*]";
              break;
            }
            t = t.getTypePtr()->getAsArrayTypeUnsafe()->getElementType();

            is_const = t.isConstQualified() || is_const;
            is_volatile = t.isVolatileQualified() || is_volatile;
            is_restrict = t.isRestrictQualified() || is_restrict;
        }
        if (t.getTypePtr()->isPointerType()) {
            t = t.getTypePtr()->getPointeeType();

            is_pointer = true;
            is_const = t.isConstQualified() || is_const;
            is_volatile = t.isVolatileQualified() || is_volatile;
            is_restrict = t.isRestrictQualified() || is_restrict;

            // Pointer to pointer
            if (t.getTypePtr()->isPointerType()) {
                Hash pointee_hash = define_type(t, ci, context);
                std::set<Hash> reqs;
                reqs.insert(pointee_hash);

                TypeDBEntry pointee = TypeDBEntry::find_type(pointee_hash);

                Hash hash = TypeDBEntry::mkType(
                        // name of the pointee type
                        pointee.name() + "*",
                        size,
                        true,
                        pointee.is_const() || is_const,
                        pointee.is_volatile() || is_volatile,
                        pointee.is_restrict() || is_restrict,
                        size_mod,
                        "",
                        "",
                        0,
                        0,
                        reqs).hash();
                seen[t] = hash;
                return hash;
            }
        }
    }

    // Attempt to find the base type in the cache
    std::map<QualType, Hash, QualTypeComparator>::iterator search;
    search = seen.find(t);
    if (search != seen.end()) {
        TypeDBEntry base = TypeDBEntry::find_type(search->second);

        if (base.in_include_file())
            return TypeDBEntry::mkInclude(
                       base.name(),
                       size,
                       is_pointer,
                       is_const,
                       is_volatile,
                       is_restrict,
                       size_mod,
                       base.file(),
                       base.line(),
                       base.col(),
                       base.include_file()).hash();
        else
            return TypeDBEntry::mkType(
                       base.name(),
                       size,
                       is_pointer,
                       is_const,
                       is_volatile,
                       is_restrict,
                       size_mod,
                       base.text(),
                       base.file(),
                       base.line(),
                       base.col(),
                       base.reqs()).hash();
    }

    // Now handle the normal type.
    if (t.getTypePtr()->getAs<TypedefType>()) {
        TypedefNameDecl * td = t.getTypePtr()->getAs<TypedefType>()->getDecl();

        std::string header;
        SourceManager & sm = ci->getSourceManager();
        SourceLocation loc = td->getSourceRange().getBegin();
        if (Utils::in_header(td->getSourceRange().getBegin(), ci, header) &&
            (sm.isInSystemHeader(loc) || sm.isInExternCSystemHeader(loc))) {
            loc = sm.getSpellingLoc(loc);
            PresumedLoc beginLoc = sm.getPresumedLoc(loc);

            Hash hash = TypeDBEntry::mkInclude(
                td->getNameAsString(),
                size,
                is_pointer,
                is_const,
                is_volatile,
                is_restrict,
                size_mod,
                safe_get_filename(beginLoc),
                beginLoc.getLine(),
                beginLoc.getColumn(),
                header).hash();
            seen[t] = hash;
            return hash;
        }
        else {
            // User-defined typedef
            const QualType u_tt = td->getUnderlyingType();

            // Emit underlying type
            std::set<Hash> reqs;
            reqs.insert(define_type(u_tt, ci, context));

            // Emit the typedef itself
            // Exception: in the case of an underlying composite type,
            //  just emit a one-line typedef so that we don't repeat
            //  the definition of the struct/class/union itself.
            //  Exception to the exception: If the underlying struct
            //   is anonymous, we must emit the type here after all!
            bool one_line = false;
            std::string one_line_name, name;
            if (u_tt.getTypePtr()->isRecordType()) {
                one_line = true;
                const RecordDecl * rd = NULL;
                if (u_tt.getTypePtr()->isStructureType()) {
                    rd = u_tt.getTypePtr()->getAsStructureType()
                                          ->getDecl()
                                          ->getDefinition();
                    one_line_name = "typedef struct ";
                }
                else if (u_tt.getTypePtr()->isUnionType()) {
                    rd = u_tt.getTypePtr()->getAsUnionType()
                                          ->getDecl()
                                          ->getDefinition();
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
                    size,
                    is_pointer,
                    is_const,
                    is_volatile,
                    is_restrict,
                    size_mod,
                    one_line_name,
                    safe_get_filename(beginLoc),
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
                if (!text.empty())
                    text += ";";

                TypeDBEntry ti = TypeDBEntry::mkType(td->getNameAsString(),
                                                     size,
                                                     is_pointer,
                                                     is_const,
                                                     is_volatile,
                                                     is_restrict,
                                                     size_mod,
                                                     text,
                                                     safe_get_filename(beginLoc),
                                                     beginLoc.getLine(),
                                                     beginLoc.getColumn(),
                                                     reqs);
                seen[t] = ti.hash();
                return ti.hash();
            }
        }
    }

    else if (t.getTypePtr()->getAs<BuiltinType>()) {
        std::string name =
            t.getTypePtr()->getAs<BuiltinType>()
                          ->getName(PrintingPolicy(ci->getLangOpts()));
        Hash hash = TypeDBEntry::mkInclude(
            name,
            size,
            is_pointer,
            is_const,
            is_volatile,
            is_restrict,
            size_mod,
            "",
            0,
            0,
            "").hash();
        seen[t] = hash;
        return hash;
    }

    else if (t.getTypePtr()->getAs<ParenType>()) {
        return define_type(t.getTypePtr()->getAs<ParenType>()->getInnerType(),
                           ci, context);
    }

    else if (t.getTypePtr()->getAs<FunctionProtoType>()) {
        const FunctionProtoType * ft = t->getAs<FunctionProtoType>();
        std::set<Hash> reqs;

        reqs.insert(define_type(ft->getReturnType(), ci, context));
        for (FunctionProtoType::param_type_iterator it = ft->param_type_begin();
             it != ft->param_type_end();
             ++it)
        {
            reqs.insert(define_type(*it, ci, context));
        }

        Hash hash = TypeDBEntry::mkType(
            t.getAsString(),
            size,
            is_pointer,
            is_const,
            is_volatile,
            is_restrict,
            size_mod,
            "",
            "",
            0,
            0,
            reqs).hash();
        seen[t] = hash;
        return hash;
    }

    else if (t.getTypePtr()->getAs<FunctionNoProtoType>()) {
        const FunctionNoProtoType * ft = t->getAs<FunctionNoProtoType>();
        std::set<Hash> reqs;

        reqs.insert(define_type(ft->getReturnType(),
                                StorageClass::SC_None,
                                ci,
                                context));
        Hash hash = TypeDBEntry::mkType(
            t.getAsString(),
            size,
            is_pointer,
            is_const,
            is_volatile,
            is_restrict,
            get_storage_class_string(sc),
            size_mod,
            "",
            "",
            0,
            0,
            reqs).hash();
        seen[t] = hash;
        return hash;
    }

    else if (t.getTypePtr()->isStructureType() || t.getTypePtr()->isUnionType()) {
        const RecordType * rt = t.getTypePtr()->isStructureType() ?
                                t.getTypePtr()->getAsStructureType() :
                                t.getTypePtr()->getAsUnionType();
        RecordDecl * decl = rt->getDecl();
        RecordDecl * rd = decl->getDefinition();

        // If there's no definition (e.g. from typedef struct Foo * FooPtr),
        // just use the declaration. That at least gives us the name.
        if (rd == NULL)
            rd = decl;

        std::string header;
        SourceManager & sm = ci->getSourceManager();
        SourceLocation loc = rd->getSourceRange().getBegin();
        if (Utils::in_header(rd->getSourceRange().getBegin(), ci, header) &&
            (sm.isInSystemHeader(loc) || sm.isInExternCSystemHeader(loc))) {
            loc = sm.getSpellingLoc(loc);
            PresumedLoc beginLoc = sm.getPresumedLoc(loc);

            Hash hash = TypeDBEntry::mkInclude(
                rd->getNameAsString(),
                size,
                is_pointer,
                is_const,
                is_volatile,
                is_restrict,
                size_mod,
                safe_get_filename(beginLoc),
                beginLoc.getLine(),
                beginLoc.getColumn(),
                header).hash();
            seen[t] = hash;
            return hash;
        }
        else {
            std::set<Hash> reqs;

            // Forward-declare the struct/class if it is not anonymous

            if (rd->getNameAsString() != "") {
                SourceManager & sm = ci->getSourceManager();
                PresumedLoc beginLoc =
                    sm.getPresumedLoc(rd->getSourceRange().getBegin());

                TypeDBEntry fdecl = TypeDBEntry::mkFwdDecl(
                    rd->getNameAsString(),
                    is_pointer,
                    size_mod,
                    (t->isStructureType() ? "struct" : "union"),
                    safe_get_filename(beginLoc),
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
                Hash h = define_type(it->getType(), ci, context);
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
            if (!text.empty())
                text += ";";

            TypeDBEntry ti = TypeDBEntry::mkType(rd->getNameAsString(),
                                                 size,
                                                 is_pointer,
                                                 is_const,
                                                 is_volatile,
                                                 is_restrict,
                                                 size_mod,
                                                 text,
                                                 safe_get_filename(beginLoc),
                                                 beginLoc.getLine(),
                                                 beginLoc.getColumn(),
                                                 reqs);
            seen[t] = ti.hash();
            return ti.hash();
        }
    }

    return 0;
}

Hash clang_mutate::hash_type(const QualType & t, CompilerInstance * ci,
                             ASTContext *context)
{
    return define_type(t, ci, context);
}
