
#include "clang-mutate.h"
#include <unistd.h>
#include <iomanip>
#include <sstream>

namespace clang_mutate {
using namespace clang;
using namespace parser_templates;

template <typename T> using Optional = parser_templates::Optional<T>;

extern const char get_[]  = "get";
extern const char as_[] = "as";
struct get_op
{
    typedef sequence_<str_<get_>, optional<chr<'\''>>> command;

    typedef tokens< command, p_ast,
                    optional< tokens<str_<as_>, variable > > > parser;

    static RewritingOpPtr make(
        Optional<char> const& quote,
        AstRef const& ast,
        Optional<std::string> const& var)
    {
        char _;
        bool normalizing = quote.get(_);
        std::string v = "$$";
        (void) var.get(v);
        return getTextAs(ast->counter(), v, normalizing);
    }

    static std::vector<std::string> purpose()
    {
        return { "Get the text of an AST, with free variables indicated."
               , "Alternately, use get' to extract the normalized AST text."
               , "The optional 'as' clause binds the result to a variable."
               };
    }
};

extern const char set_[] = "set";
struct set_op
{
    typedef sequence_< str_<set_>, optional<chr<'\''>>> command;

    typedef tokens< command, many1<tokens<p_ast, p_text>> >
        parser;

    static RewritingOpPtr make(
        Optional<char> const& quote,
        std::vector<std::pair<AstRef, std::string>> const& args)
    {
        char _;
        bool normalizing = !quote.get(_);
        RewritingOps ops;
        TURef tu;
        for (auto & p : args) {
            ops.push_back(setText(p.first->counter(),
                                  p.second,
                                  normalizing));
            tu = p.first.tuid();
        }
        return chain(ops);
    }

    static std::vector<std::string> purpose()
    { return { "Set the text of an AST to the given value."
             , "Use set' to set the un-normalized text." }; }
};

extern const char print_[] = "print";
struct print_op
{
    typedef alt<str_<print_>, ignored<chr<'p'>>> command;

    typedef tokens< command, p_tu > parser;

    static RewritingOpPtr make(TURef const& tu)
    { return printOriginal(tu); }

    static std::vector<std::string> purpose()
    { return { "Print the original source for a translation unit." }; }
};

extern const char preview_[] = "preview";
struct preview_op
{
    typedef str_<preview_> command;
    typedef tokens< command, p_tu > parser;

    static RewritingOpPtr make(TURef const& tu)
    { return printModified(tu); }

    static std::vector<std::string> purpose()
    { return { "Print the modified source for a translation unit." }; }
};

extern const char info_[] = "info";
struct info_op
{
    typedef str_<info_> command;
    typedef tokens< command > parser;

    static RewritingOpPtr make()
    {
        std::ostringstream oss;
        // Figure out how far to the right the table should go.
        size_t maxFilenameLength = 12;
        std::string prefix("");
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)))
            prefix = std::string(cwd);

        // Gather the filenames, stripping off the current
        // working directory.
        std::vector<std::string> filenames;
        for (auto & tu : TUs) {
            std::string filename = tu.second->filename;
            if (filename.find(prefix) == 0)
                filename = filename.substr(prefix.size());
            if (filename.size() + 2 > maxFilenameLength)
                maxFilenameLength = filename.size() + 2;
            filenames.push_back(filename);
        }
#define HRULE                                                      \
        oss << "+------+--------+";                                \
        for (size_t i = 0; i < maxFilenameLength; ++i) oss << "-"; \
        oss << "+" << std::endl;

#define PAD(k)                                                     \
        for (size_t i = k; i < maxFilenameLength; ++i) oss << " "; \
        oss << "|" << std::endl;

        // Print the table
        HRULE;
        oss << "|  TU  |  ASTs  |  Filename"; PAD(10);
        HRULE;
        size_t n = 0;
        for (auto & tu : TUs) {
            std::string name = filenames[n];
            oss << "| " << std::setw(4) << tu.first << " | "
                << std::setw(6) << tu.second->asts.size() << " | "
                << name;
            PAD(name.size() + 1);
            ++n;
        }
        HRULE;
        return echo(oss.str());
    }

    static std::vector<std::string> purpose()
    { return { "Print information about the loaded translation units." }; }
};

extern const char help_fields_[] = "?fields";
extern const char help_fields_md_[] = "md";
struct help_fields_op
{
    typedef str_<help_fields_> command;
    typedef tokens< command, optional<str<help_fields_md_>>> parser;

    static RewritingOpPtr make(Optional<std::string> const& format)
    {
        std::string how;
        std::ostringstream oss;
        if (!format.get(how)) {
            oss << "JSON representation of ASTs" << std::endl
                << "---------------------------" << std::endl;
            oss << "Each AST node is represented by a single JSON object, "
                << "with the following fields:" << std::endl << std::endl;
            #define FIELD_DEF(Name, T, Descr, Predicate, Body)           \
            oss << "  key   = \"" #Name "\"" << std::endl                \
                << "  value = " << describe_json<T>::str() << std::endl; \
            for (auto & line : Utils::split(Descr, '\n'))                \
                oss << "    " << line << std::endl;                      \
            oss << std::endl;
            #include "FieldDefs.cxx"
            return echo(oss.str());
        }
        else if (how == "md") {
            // Format the field descriptions for inclusion in a markdown file.
            #define FIELD_DEF(Name, T, Descr, Predicate, Body)                \
            {                                                                 \
                bool first_descr = true;                                      \
                oss << #Name << ": " << describe_json<T>::str() << std::endl; \
                for (auto & line : Utils::split(Descr, '\n')) {               \
                    oss << (first_descr ? ":   " : "    ");                   \
                    oss << line << std::endl;                                 \
                    first_descr = false;                                      \
                }                                                             \
                oss << std::endl;                                             \
            }
            #include "FieldDefs.cxx"
            std::string s = oss.str();
            return echo(Utils::intercalate(Utils::split(s, '_'), "\\_"));
        }
        else {
            oss << "unknown formatting option '" << how << "'" << std::endl;
            return echo(oss.str());
        }
    }

    static std::vector<std::string> purpose()
    { return { "Print usage information and definitions"
               " for AST JSON representation." }; }
};

extern const char types_[] = "types";
struct types_op
{
    typedef str_<types_> command;
    typedef tokens< command > parser;

    static RewritingOpPtr make()
    {
        std::ostringstream oss;
        oss << to_json(TypeDBEntry::databaseToJSON());
        return echo(oss.str());
    }

    static std::vector<std::string> purpose()
    { return { "Print a JSON representation of the type database." }; }
};

extern const char echo_[] = "echo";
struct echo_op
{
    typedef str_<echo_> command;
    typedef tokens< command, p_text > parser;

    static RewritingOpPtr make(std::string const& text)
    { return echo(text); }

    static std::vector<std::string> purpose()
    { return
        { "Echo the given token, JSON-escaped quoted string, or variable." }; }
};

extern const char setfunc_[] = "set-func";
struct setfunc_op
{
    typedef str_<setfunc_> command;
    typedef tokens< command, p_ast, p_text > parser;

    static RewritingOpPtr make(
        AstRef const& body,
        std::string const& text)
    {
        TU & tu = body.tu();
        auto fsearch = tu.function_starts.find(body);
        if (fsearch == tu.function_starts.end()) {
            std::ostringstream oss;
            oss << body << " is not the body AST of a function.";
            return note(oss.str());
        }
        int adjust = fsearch->second - body->initial_offset();
        return setRangeText(body, body, text, adjust, 0);
    }

    static std::vector<std::string> purpose()
    {
        return { "Replace the full text of a function."
               , "The function is identified by providing its body AST."
               };
    }
};

extern const char clear_[] = "clear";
struct clear_op
{
    typedef str_<clear_> command;
    typedef tokens< command, optional<p_text> > parser;

    static RewritingOpPtr make(Optional<std::string> const& var)
    {
        std::string v;
        return var.get(v) ? clear_var(v) : clear_vars();
    }

    static std::vector<std::string> purpose()
    {
        return { "Clear any bindings for the given variable or, if no"
               , "variable was provided, clear all variables."
               };
    }
};

extern const char reset_[] = "reset";
struct reset_op
{
    typedef str_<reset_> command;
    typedef tokens< command, optional<p_tu> > parser;

    static RewritingOpPtr make(Optional<TURef> const& tu)
    {
        TURef tuid;
        return tu.get(tuid) ? reset_buffer(tuid) : reset_buffers();
    }

    static std::vector<std::string> purpose()
    {
        return { "Reset a translation unit's rewrite buffer or, if no"
               , "translation unit was provided, reset all rewrite buffers."
               };
    }
};

extern const char binary_[] = "binary";
struct binary_op
{
    typedef str_<binary_> command;
    typedef tokens< command, p_tu, p_text, optional<p_text> > parser;

    static RewritingOpPtr make(
        TURef const& tuid,
        std::string const& binaryPath,
        Optional<std::string> const& dwarfFilepathMapping)
    {
        std::string pathmap = "";
        (void) dwarfFilepathMapping.get(pathmap);
        TUs[tuid]->addrMap = BinaryAddressMap(binaryPath, pathmap);
        std::ostringstream oss;
        oss << "set TU " << tuid << "'s binary path to " << binaryPath;
        if (pathmap != "")
            oss << ", with path map " << pathmap;
        return note(oss.str());
    }

    static std::vector<std::string> purpose()
    {
        return { "Provide a binary for a given translation unit."
               , "The optional argument is a path to the DWARF filepath map."
                };
    }
};

extern const char ids_[] = "ids";
struct ids_op
{
    typedef str_<ids_> command;
    typedef tokens< command, p_tu > parser;

    static RewritingOpPtr make(TURef const& tuid)
    {
        std::ostringstream oss;
        oss << TUs[tuid]->asts.size();
        return echo(oss.str());
    }

    static std::vector<std::string> purpose()
    { return { "Print the number of ASTs in a translation unit." }; }
};

class ClassAnnotator : public Annotator
{
public:
    virtual ~ClassAnnotator() {}

    std::string before(const Ast * ast) {
        std::ostringstream oss;
        oss << "/* " << ast->counter()
            << ":" << ast->className() << "[ */";
        return oss.str();
    }

    std::string after(const Ast * ast) {
        std::ostringstream oss;
        oss << "/* ]" << ast->counter() << " */";
        return oss.str();
    }

    std::string describe() { return "class-and-counter"; }
};

ClassAnnotator classAnnotator;

extern const char annotate_[] = "annotate";
struct annotate_op
{
    typedef str_<annotate_> command;
    typedef tokens< command, p_tu > parser;

    static RewritingOpPtr make(TURef const& tuid)
    { return annotateWith(tuid, &classAnnotator); }

    static std::vector<std::string> purpose()
    { return { "Annotate each AST with its counter and class." }; }
};

class CounterAnnotator : public Annotator
{
public:
    virtual ~CounterAnnotator() {}

    std::string before(const Ast * ast) {
        std::ostringstream oss;
        oss << "/* " << ast->counter() << "[ */";
        return oss.str();
    }

    std::string after(const Ast * ast) {
        std::ostringstream oss;
        oss << "/* ]" << ast->counter() << " */";
        return oss.str();
    }

    std::string describe() { return "counter"; }
};

CounterAnnotator counterAnnotator;

extern const char number_[] = "number";
struct number_op
{
    typedef str_<number_> command;
    typedef tokens< command, p_tu > parser;

    static RewritingOpPtr make(TURef const& tuid)
    { return annotateWith(tuid, &counterAnnotator); }

    static std::vector<std::string> purpose()
    { return { "Annotate each AST with its counter." }; }
};

class FullCounterAnnotator : public Annotator
{
public:
    virtual ~FullCounterAnnotator() {}

    std::string before(const Ast * ast)
    {
        std::ostringstream oss;
        if (ast->isFullStmt())
            oss << "/*[" << ast->counter() << ":]*/ ";
        return oss.str();
    }

    std::string after(const Ast*) { return ""; }

    std::string describe() { return "full-counter"; }
};

FullCounterAnnotator fullCounterAnnotator;

extern const char number_full_[] = "number-full";
struct number_full_op
{
    typedef str_<number_full_> command;
    typedef tokens< command, p_tu > parser;

    static RewritingOpPtr make(TURef const& tuid)
    { return annotateWith(tuid, &fullCounterAnnotator); }

    static std::vector<std::string> purpose()
    { return { "Annotate each full AST with its counter." }; }
};

extern const char list_[] = "list";
struct list_op
{
    typedef str_<list_> command;
    typedef tokens< command, p_tu > parser;

    static RewritingOpPtr make(TURef const & tuid)
    {
        std::ostringstream oss;
        TU * tu = TUs[tuid];

        std::vector<picojson::value> ans;
        std::set<std::string> all_keys;
        for (auto & ast : tu->asts) {
            char msg[256];
            sprintf(msg, "%8d %6d:%-3d %6d:%-3d %-25s",
                    (unsigned int) ast->counter().counter(),
                    ast->begin_src_pos().getLine(),
                    ast->begin_src_pos().getColumn(),
                    ast->end_src_pos().getLine(),
                    ast->end_src_pos().getColumn(),
                    ast->className().c_str());
            BinaryAddressMap::BeginEndAddressPair addrRange;
            if (ast->binaryAddressRange(addrRange)) {
                sprintf(msg + strlen(msg), " %#016lx %#016lx",
                        addrRange.first,
                        addrRange.second);
            }
            oss << msg << std::endl;
        }
        return echo(oss.str());
    }

    static std::vector<std::string> purpose()
    { return { "List the ASTs, showing source ranges and classes." }; }
};

extern const char cut_[] = "cut";
struct cut_op
{
    typedef str_<cut_> command;
    typedef tokens< command, many1<tokens<p_ast>> > parser;

    static RewritingOpPtr make(std::vector<AstRef> const& targets)
    {
        RewritingOps ops;
        for (auto & ast : targets)
            ops.push_back(setText(ast, ""));
        return chain(ops);
    }

    static std::vector<std::string> purpose()
    { return { "Remove the source text of the given ASTs." }; }
};

extern const char setrange_[] = "set-range";
struct setrange_op
{
    typedef str_<setrange_> command;
    typedef tokens< command, many1<tokens<p_ast, p_ast, p_text>> > parser;

    static RewritingOpPtr make(
        std::vector<std::tuple<AstRef, AstRef, std::string>> const& ranges)
    {
        RewritingOps ops;
        for (auto & range : ranges) {
            ops.push_back(setRangeText(std::get<0>(range),
                                       std::get<1>(range),
                                       std::get<2>(range)));
        }
        return chain(ops);
    }

    static std::vector<std::string> purpose()
    {
        return { "Replace the source range from the start of the first AST"
               , "to the end of the second AST with the given text."
               };
    }
};

extern const char insert_[] = "insert";
extern const char before_[] = "before";
struct insert_op
{
    typedef sequence_ < alt< str_<insert_>, str_<before_> >
                      , optional<chr<'\''>>
                      > command;
    typedef tokens< command, many1<tokens<p_ast, p_text>> > parser;

    static RewritingOpPtr make(
        Optional<char> const& quote,
        std::vector<std::pair<AstRef, std::string>> const& insertions)
    {
        char _;
        bool normalizing = !quote.get(_);
        RewritingOps ops;
        for (auto & insertion : insertions) {
            ops.push_back(insertBefore(insertion.first,
                                       insertion.second,
                                       normalizing));
        }
        return chain(ops);
    }

    static std::vector<std::string> purpose()
    { return { "Insert text before the given AST."
             , "Use the ' variant to perform non-normalizing insertions." }; }
};

extern const char after_[] = "after";
struct after_op
{
    typedef sequence_< str_<after_>, optional<chr<'\''>>> command;
    typedef tokens< command, many1<tokens<p_ast, p_text>> > parser;

    static RewritingOpPtr make(
        Optional<char> const& quote,
        std::vector<std::pair<AstRef, std::string>> const& insertions)
    {
        char _;
        bool normalizing = !quote.get(_);
        RewritingOps ops;
        for (auto & insertion : insertions) {
            ops.push_back(insertAfter(insertion.first,
                                      insertion.second,
                                      normalizing));
        }
        return chain(ops);
    }

    static std::vector<std::string> purpose()
    { return { "Insert text after the given AST."
             , "Use the ' variant to perform non-normalizing insertions." }; }
};

extern const char comment_[] = "comment";
struct comment_op
{
    typedef str_<comment_> command;
    typedef tokens< command, many1<tokens<p_ast>> > parser;

    static RewritingOpPtr make(
        std::vector<AstRef> const& stmts)
    {
        RewritingOps ops;
        for (auto & stmt : stmts) {
            ops.push_back(insertBefore(stmt, "/*", false));
            ops.push_back(insertAfter (stmt, "*/", false));
        }
        return chain(ops);
    }

    static std::vector<std::string> purpose()
    { return { "Comment out the given AST." }; }
};

extern const char swap_[] = "swap";
struct swap_op
{
    typedef str_<swap_> command;
    typedef tokens< command, p_ast, p_ast > parser;

    static RewritingOpPtr make(AstRef const& ast1, AstRef const& ast2)
    {
        return chain( { getTextAs (ast1, "$swap1")
                      , getTextAs (ast2, "$swap2")
                      , setText   (ast1, "$swap2")
                      , setText   (ast2, "$swap1") });
    }

    static std::vector<std::string> purpose()
    { return { "Swap the text of two ASTs." }; }
};

extern const char aux_[] = "aux";
struct aux_op
{
    typedef str_<aux_> command;
    typedef tokens< command, p_tu > parser;

    static RewritingOpPtr make(TURef const& tuid)
    {
        auto & aux = TUs[tuid]->aux;
        std::map<std::string, picojson::value> ans;
        std::ostringstream oss;
        for (auto & entry : aux)
            ans[entry.first] = to_json(entry.second);
        oss << to_json(ans);
        return echo(oss.str());
    }

    static std::vector<std::string> purpose()
    { return { "Display auxiliary database entries for a translation unit." }; }
};

template <char const * keyword> using
kwarg = optional<sequence_<str_<keyword>, sepBy<word, chr<','>>>>;

extern const char ast_[] = "ast";
extern const char jfields_[] = "fields=";
struct ast_op
{
    typedef str_<ast_> command;
    typedef tokens< command, p_ast, kwarg<jfields_> > parser;

    static RewritingOpPtr make(
        AstRef const& ast,
        Optional<std::vector<std::string>> const& fields)
    {
        std::set<std::string> ast_fields;
        std::vector<std::string> parsed_fields;
        (void) fields.get(parsed_fields);
        for (auto & field : parsed_fields)
            ast_fields.insert(field);
        std::ostringstream oss;
        oss << ast->toJSON(ast_fields, true);
        return echo(oss.str());
    }

    static std::vector<std::string> purpose()
    { return { "Dump the representation of an AST as JSON."
             , "Use '?fields' to get a description of each JSON field." }; }
};

extern const char json_[] = "json";
extern const char jaux_[] = "aux=";
struct json_op
{
    typedef str_<json_> command;
    typedef tokens< command, p_tu, kwarg<jaux_>, kwarg<jfields_> > parser;

    static RewritingOpPtr make(
        TURef const& tuid,
        Optional<std::vector<std::string>> const& aux_fields,
        Optional<std::vector<std::string>> const& ast_fields)
    {
        TU & tu = *TUs[tuid];
        std::string sep = "";
        std::ostringstream oss;
        oss << "[";

        bool include_aux = false;
        std::set<std::string> aux_keys;
        std::vector<std::string> auxf = { "types", "decls", "asts" };
        (void) aux_fields.get(auxf);
        for (auto & a : auxf)
            aux_keys.insert(a);
        for (auto & aux_key : aux_keys) {
            if (aux_key == "types") {
                for (auto & entry : TypeDBEntry::databaseToJSON()) {
                    oss << sep << to_json(entry);
                    sep = ",";
                }
            }
            else if (aux_key == "asts") {
                include_aux = true;
            }
            else {
                for (auto & entry : tu.aux[aux_key]) {
                    oss << sep << to_json(entry);
                    sep = ",";
                }
            }
        }

        std::set<std::string> ast_keys;
        std::vector<std::string> astf;
        (void) ast_fields.get(astf);
        for (auto & a : astf)
            ast_keys.insert(a);
        for (auto & ast : tu.asts) {
            oss << sep << ast->toJSON(ast_keys, include_aux);
            sep = ",";
        }
        oss << "]";
        return echo(oss.str());
    }

    static std::vector<std::string> purpose()
    { return { "Dump the representation of a translation unit as JSON."
             , "Use '?fields' to get a description of each JSON field." }; }
};

// Note: this is a copy of json_op with only minor changes to the
// serialization code. Could probably be refactored.
extern const char sexp_[] = "sexp";
struct sexp_op
{
    typedef str_<sexp_> command;
    typedef tokens< command, p_tu, kwarg<jaux_>, kwarg<jfields_> > parser;

    static RewritingOpPtr make(
        TURef const& tuid,
        Optional<std::vector<std::string>> const& aux_fields,
        Optional<std::vector<std::string>> const& ast_fields)
    {
        TU & tu = *TUs[tuid];
        std::string sep = "";
        std::ostringstream oss;
        oss << "(";

        bool include_aux = false;
        std::set<std::string> aux_keys;
        std::vector<std::string> auxf = { "types", "decls", "asts" };
        (void) aux_fields.get(auxf);
        for (auto & a : auxf)
            aux_keys.insert(a);
        for (auto & aux_key : aux_keys) {
            if (aux_key == "types") {
                for (auto & entry : TypeDBEntry::databaseToJSON()) {
                    oss << sep;
                    serialize_as_sexpr(entry, oss);
                    sep = "\n";
                }
            }
            else if (aux_key == "asts") {
                include_aux = true;
            }
            else {
                for (auto & entry : tu.aux[aux_key]) {
                    oss << sep;
                    serialize_as_sexpr(entry, oss);
                    sep = "\n";
                }
            }
        }

        std::set<std::string> ast_keys;
        std::vector<std::string> astf;
        (void) ast_fields.get(astf);
        for (auto & a : astf)
            ast_keys.insert(a);
        for (auto & ast : tu.asts) {
            oss << sep;
            serialize_as_sexpr(ast->toJSON(ast_keys, include_aux), oss);
            sep = "\n";
        }
        oss << ")";
        return echo(oss.str());
    }

    static std::vector<std::string> purpose()
    { return { "Dump the representation of a translation unit as JSON."
             , "Use '?fields' to get a description of each JSON field." }; }
};

extern const char load_[] = "load";
struct load_op
{
    typedef str_<load_> command;
    typedef tokens< command
                  , p_text
                  , fmap<FromOptional<std::vector<std::string>>,
                         optional<sepBy<p_text, spaces>>>
                  > parser;

    static RewritingOpPtr make(
        std::string const& path,
        std::vector<std::string> const& options)
    {
        int argc = 3 + options.size();
        const char** argv = new const char*[argc + 1];
        argv[0] = "clang-mutate";
        argv[1] = path.c_str();
        argv[2] = "--";
        for (size_t i = 0; i < options.size(); ++i)
            argv[3 + i] = options[i].c_str();
        argv[argc] = NULL;

        int result = process_command_line(argc, argv);
        std::ostringstream oss;
        oss << "loaded " << path
            << ": tu = " << tu_in_progress->tuid
            << ", result = " << result;
        delete [] argv;
        return echo(oss.str());
    }

    static std::vector<std::string> purpose()
    { return { "Load a file as a new translation unit. Optionally provide"
             , "compilation flags after the filename." }; }
};

extern const char unload_[] = "unload";
struct unload_op
{
    typedef str_<unload_> command;
    typedef tokens< command, p_tu > parser;

    static RewritingOpPtr make(TURef const& tu)
    {
        auto it = TUs.find(tu);
        std::ostringstream oss;
        oss << "unloaded translation unit " << it->first;
        auto op = reset_buffer(it->first);
        delete it->second;
        TUs.erase(it);
        return op->then(echo(oss.str()));
    }

    static std::vector<std::string> purpose()
    { return { "Unload a translation unit." }; }
};

////////////////////////////////////////////////////////////////////////////////
//
//  Make, Make_impl: call X::make() with arguments unpacked from a tuple or a
//                   pair.
//
// This is a specialized version of http://stackoverflow.com/questions/10766112/
// c11-i-can-go-from-multiple-args-to-tuple-but-can-i-go-from-tuple-to-multiple
//
template <typename X, typename Args, bool done, int size, int... N>
struct Make_impl
{
    static RewritingOpPtr go(Args&& args)
    {
        return Make_impl
            <X, Args, size == 1 + sizeof...(N), size, N..., sizeof...(N)>
            ::go(std::forward<Args>(args));
    }
};

template <typename X, typename Args, int size, int...N>
struct Make_impl<X, Args, true, size, N...>
{
    static RewritingOpPtr go(Args&& args)
    { return X::make(std::get<N>(std::forward<Args>(args))...); }
};

template <typename X>
struct Make
{
    typedef typename X::parser::type dom;
    typedef RewritingOpPtr cod;

    static cod apply(void) { return X::make(); }

    template <typename T>
    static cod apply(T & x) { return X::make(x); }

    template <typename T1, typename T2>
    static cod apply(std::pair<T1,T2> & x)
    { return X::make(x.first, x.second); }

    template <typename ...Ts>
    static cod apply(std::tuple<Ts...> & x)
    {
        typedef std::tuple<Ts...> Tuple;
        return Make_impl<
            X, dom, 0 == std::tuple_size<Tuple>::value,
            std::tuple_size<Tuple>::value>::go(std::forward<Tuple>(x));
    }

    template <typename P> static std::string describe()
    { return P::describe(); }
};

} // end namespace clang_mutate
