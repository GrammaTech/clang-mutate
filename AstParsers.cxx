
namespace clang_mutate {
using namespace parser_templates;

// Parse a translation unit.
struct p_tu;

// Parse an AST identifier.
struct p_ast;

// Parse a text snippet, e.g. one of
//   - a quoted, escaped string.
//   - a single, unquoted word.
struct p_text;

// Parse a variable identifier.
typedef fmap<Cons<char>, sequence_<chr<'$'>, word>> variable;

struct p_tu
{
    constexpr static bool is_productive = true;
    typedef TURef type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        parsed<type> ans;
        auto snapshot = ctx.save();
        auto n = parse<number>(ctx);
        if (!n.ok) {
            ctx.restore(snapshot);
            ctx.fail("expected a translation unit id.");
            ans.ok = false;
            return ans;
        }
        if (n.result >= TUs.size()) {
            std::ostringstream oss;
            oss << "only " << TUs.size() << " translation units loaded, id "
                << n.result << " is too large.";
            ctx.restore(snapshot);
            ctx.fail(oss.str());
            ans.ok = false;
            return ans;
        }
        ans.ok = true;
        ans.result = n.result;
        return ans;
    }
    static std::string describe() { return "<tu>"; }
};

struct p_ast
{
    constexpr static bool is_productive = true;
    typedef AstRef type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        parsed<type> ans;
        auto snapshot = ctx.save();
        auto n = parse<sequence_<p_tu, ignored<chr<'.'>>, number>>(ctx);
        if (!n.ok) {
            ctx.restore(snapshot);
            ctx.fail("expected an AST identifier of the form 'tu.ast'.");
            ans.ok = false;
            return ans;
        }
        TURef tu = std::get<0>(n.result);
        AstCounter counter = std::get<1>(n.result);
        if (counter == 0) {
            ctx.restore(snapshot);
            ctx.fail("AST numbering is 1-based.");
            ans.ok = false;
            return ans;
        }
        if (counter > TUs[tu]->asts.size()) {
            std::ostringstream oss;
            oss << "only " << TUs[tu]->asts.size()
                << " ASTs in translation unit " << tu;
            ctx.restore(snapshot);
            ctx.fail(oss.str());
            ans.ok = false;
            return ans;
        }
        ans.result = AstRef(tu, counter);
        ans.ok = true;
        return ans;
    }
    static std::string describe() { return "<ast>"; }
};

struct p_text
{
    constexpr static bool is_productive = true;
    typedef std::string type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        auto snapshot = ctx.save();
        auto quoted = parse<chr<'\"'>>(ctx);
        if (quoted.ok) {
            parsed<type> ans;
            ans.result = "\"";
            bool escaped = false;
            while (ctx.ok()) {
                char c;
                if (!ctx.get(c)) {
                    ctx.fail("unexpected end of input while parsing "
                             "quoted string.");
                    ans.ok = false;
                    return ans;
                }
                ans.result.push_back(c);
                if (!escaped && c == '\"')
                    break;
                if (escaped)
                    escaped = false;
                else if (c == '\\')
                    escaped = true;
            }
            ans.ok = true;
            ans.result = Utils::unescape(ans.result);
            return ans;
        }
        else {
            // Not quoted, parse one token.
            ctx.restore(snapshot);
            return parse<word>(ctx);
        }
    }
    static std::string describe() { return "<text>"; }
};

} // end namespace clang_mutate
