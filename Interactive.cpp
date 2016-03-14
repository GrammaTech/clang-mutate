#include "Interactive.h"

#include "TU.h"
#include "BinaryAddressMap.h"
#include "Bindings.h"
#include "Rewrite.h"
#include "Macros.h"
#include "Renaming.h"
#include "Scopes.h"
#include "Requirements.h"
#include "Utils.h"

#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/SaveAndRestore.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <set>
#include <unistd.h>

namespace clang_mutate{
using namespace clang;

std::map<std::string, bool> interactive_flags;

#define DONE   \
    (interactive_flags["ctrl"] ? std::string("\x17") : "") << std::endl
#define CANCEL \
    (interactive_flags["ctrl"] ? std::string("\x18") : "") << std::endl

#define EXPECT(p, msg)                                                  \
    if (!(p)) { std::cout << "** " << msg << CANCEL; continue; }

std::vector<TU> TUs;

void runInteractiveSession(std::istream & input)
{
    NamedText vars;
    interactive_flags["ctrl"] = false;

    std::cout << "***** begin clang-mutate interactive session *****" << std::endl;
    std::cout << "processed " << TUs.size() << " translation units" << std::endl;

    while (true) {

        std::string cmdline;
        std::string err;
        std::cout << "clang-mutate> " << std::flush;
        std::getline(input, cmdline);
        std::vector<std::string> cmd = Utils::split(cmdline);

        if (cmd.size() == 0)
            continue;

        if (cmd[0] == "quit" || cmd[0] == "q")
            break;

        if (cmd[0] == "configure") {
            EXPECT (cmd.size() == 3, "expected two arguments");
            bool yn;
            EXPECT (Utils::read_yesno(cmd[2], yn), "expected 'yes' or 'no'");
            auto search = interactive_flags.find(cmd[1]);
            EXPECT (search != interactive_flags.end(),
                    "unknown configuration flag " << cmd[1]);
            interactive_flags[cmd[1]] = yn;
            continue;
        }

        if (cmd[0] == "info") {
            // Figure out how far to the right the table should go.
            size_t maxFilenameLength = 12;
            std::string prefix("");
            char cwd[128];
            if (getcwd(cwd, 128))
                prefix = std::string(cwd);

            // Gather the filenames, stripping off the current
            // working directory.
            std::vector<std::string> filenames;
            for (auto & tu : TUs) {
                SourceManager & sm = tu.ci->getSourceManager();
                std::string filename = sm.getFilename(
                    sm.getLocForStartOfFile(sm.getMainFileID())).str();
                if (filename.find(prefix) == 0)
                    filename = filename.substr(prefix.size());
                if (filename.size() + 2 > maxFilenameLength)
                    maxFilenameLength = filename.size() + 2;
                filenames.push_back(filename);
            }
#define HRULE \
  std::cout << "+------+--------+"; \
  for (size_t i = 0; i < maxFilenameLength; ++i) std::cout << "-";      \
  std::cout << "+" << std::endl;

#define PAD(k) \
  for (size_t i = k; i < maxFilenameLength; ++i) std::cout << " "; \
  std::cout << "|" << std::endl;

            // Print the table
            HRULE;
            std::cout << "|  TU  |  ASTs  |  Filename"; PAD(10);
            HRULE;
            size_t n = 0;
            for (auto & tu : TUs) {
                std::string name = filenames[n];
                std::cout << "| " << std::setw(4) << n << " | "
                          << std::setw(6) << tu.astTable.count() << " | "
                          << name;
                PAD(name.size() + 1);
                ++n;
            }
            HRULE;
            std::cout << DONE;
            continue;
        }

        if (cmd[0] == "print" || cmd[0] == "p") {
            unsigned int n;
            EXPECT(cmd.size() == 2, "expected one argument");
            EXPECT(Utils::read_uint(cmd[1], n), "expected a translation unit id");
            EXPECT(n < TUs.size(),
                   "out of range, only " << TUs.size()
                                         << " translation units loaded.");
            printOriginalTo(std::cout)->run(TUs[n], err);
            std::cout << DONE;
            continue;
        }

        if (cmd[0] == "echo") {
            EXPECT(cmd.size() == 2, "expected one argument");
            std::string text = cmd[1];
            if (text[0] == '$') {
                auto search = vars.find(text);
                EXPECT(search != vars.end(), "variable " << text << " is unbound.");
                text = search->second;
            }
            std::cout << text << DONE;
            continue;
        }

        if (cmd[0] == "binary") {
            unsigned int tuid;
            EXPECT (cmd.size() == 3 || cmd.size() == 4,
                    "expected two or three arguments");
            EXPECT (Utils::read_uint(cmd[1], tuid),
                    "argument must be a translation unit id");
            EXPECT (tuid < TUs.size(),
                    "out of range, only " << TUs.size()
                                          << " translation units loaded.");
            std::string binaryPath = cmd[2];
            std::string dwarfFilepathMapping = cmd.size() == 4
                ? cmd[3]
                : std::string("");
            TUs[tuid].addrMap = BinaryAddressMap(binaryPath,
                                                 dwarfFilepathMapping);
            TUs[tuid].addrMap.dump(std::cout);
            continue;
        }
        
        if (cmd[0] == "ast" || cmd[0] == "a") {
            unsigned int tuid;
            unsigned int ast;
            EXPECT (cmd.size() == 3, "expected two arguments");
            EXPECT (Utils::read_uint(cmd[1], tuid),
                    "argument must be a translation unit id");
            EXPECT (tuid < TUs.size(),
                    "out of range, only " << TUs.size()
                                          << " translation units loaded.");
            TU & tu = TUs[tuid];
            AstTable & asts = tu.astTable;

            EXPECT (Utils::read_uint(cmd[2], ast), "expected an AST id.");
            EXPECT (ast > 0, "AST numbering begins at 1.");
            EXPECT(ast <= asts.count(),
                   "out of range, only " << asts.count() << " ASTs.");

            Ast & theAst = asts[ast];
            SourceManager & sm = tu.ci->getSourceManager();
            SourceRange r = theAst.sourceRange();
            unsigned int parent = theAst.parent();
            
            std::cout << "         counter: " << ast << std::endl;
            std::cout << "  parent_counter: " << parent << std::endl;
            std::cout << "       ast_class: " << theAst.className() << std::endl;

            std::string src_file_name = realpath(
                sm.getFileEntryForID( sm.getMainFileID() )->getName(),
                NULL);

            std::cout << "   src_file_name: " << src_file_name << std::endl;

            std::cout << "         is_decl: " << std::boolalpha
                      << theAst.isDecl() << std::endl;
            
            std::cout << "      guard_stmt: " << std::boolalpha
                      << theAst.isGuard() << std::endl;
            std::cout << "       full_stmt: "
                      << std::boolalpha
                      << ( (parent == NoAst && theAst.isStmt()) ||
                           (parent != NoAst &&
                            asts[parent].isDecl() &&
                            asts[parent].className() == "CompoundStmt") )
                      << std::endl;
            if (theAst.className() == "CompoundStmt") {
                std::cout << "       stmt_list: [";
                std::string sep = "";
                for (auto child : theAst.children()) {
                    std::cout << sep << child;
                    sep = ", ";
                }
                std::cout << "]" << std::endl;
            }
            std::cout << " *      children: [";
            {
                std::string sep = "";
                for (auto child : theAst.children()) {
                    std::cout << sep << child;
                    sep = ", ";
                }
                std::cout << "]" << std::endl;
            }

            if (theAst.isStmt() &&
                clang::isa<clang::BinaryOperator>(theAst.asStmt()))
            {
                std::cout << "          opcode: "
                          << static_cast<clang::BinaryOperator*>(theAst.asStmt())
                               ->getOpcodeStr().str()
                          << std::endl;
            }

            // Find all macros used, yielding either their definitions or the
            // files that should be #include'd to get the macro.
            {
                std::cout << "          macros: [";
                std::string sep = "";
                for (auto & macro : theAst.macros()) {
                    std::cout << sep
                              << "[" << to_json(macro.name())
                              << "," << to_json(macro.body())
                              << "]";
                    sep = ", ";
                }
                std::cout << "]" << std::endl;

                sep = "";
                std::cout << "        includes: [";
                for (auto & inc : theAst.includes()) {
                    std::cout << sep << to_json(inc);
                    sep = ", ";
                }
                std::cout << "]" << std::endl;
                    
                sep = "";
                std::cout << "           types: [";
                for (auto & t : theAst.types()) {
                    std::cout << sep << to_json(t);
                    sep = ", ";
                }
                std::cout << "]" << std::endl;

                sep = "";
                std::cout << "    unbound_vals: [";
                for (auto & val : theAst.freeVariables()) {
                    std::cout << sep << val.toJSON();
                    sep = ", ";
                }
                std::cout << "]" << std::endl;
                
                std::cout << "    unbound_funs: [";
                sep = "";
                for (auto & fun : theAst.freeFunctions()) {
                    std::cout << sep << fun.toJSON();
                    sep = ", ";
                }
                std::cout << "]" << std::endl;
            }

            // Print variables in scope, at each depth
            {
                std::cout << "          scopes: [";
                auto scopes = tu.scopes.get_names_in_scope_from(
                    theAst.scopePosition(), 1000);
                std::string sep1 = "";
                for (auto & scope : scopes) {
                    std::cout << sep1 << "[";
                    std::string sep2 = "";
                    for (auto & var : scope) {
                        std::cout << sep2 << var;
                        sep2 = ",";
                    }
                    std::cout << "]";
                    sep1 = ", ";
                }
                std::cout << "]" << std::endl;
            }
            
            std::cout << "  begin_src_line: "
                      << sm.getSpellingLineNumber(r.getBegin())
                      << std::endl;
            std::cout << "   begin_src_col: "
                      << sm.getSpellingColumnNumber(r.getBegin())
                      << std::endl;
            std::cout << "    end_src_line: "
                      << sm.getSpellingLineNumber(r.getEnd())
                      << std::endl;
            std::cout << "     end_src_col: "
                      << sm.getSpellingColumnNumber(r.getEnd())
                      << std::endl;
            std::ostringstream oss;
            ChainedOp op = {
                getTextAs (ast, "$result"),
                echoTo    (oss, "$result")
            };
            op.run(tu, err);
            std::cout << "       orig_text: " << to_json(oss.str()) << std::endl;
            std::cout << "        src_text: " << to_json(theAst.text()) << std::endl;
            
            if (!tu.addrMap.isEmpty()) {
                clang::PresumedLoc beginLoc = sm.getPresumedLoc(r.getBegin());
                clang::PresumedLoc   endLoc = sm.getPresumedLoc(r.getEnd());
                unsigned int beginSrcLine = beginLoc.getLine();
                unsigned int   endSrcLine = endLoc.getLine();
                
                BinaryAddressMap::BeginEndAddressPair addrRange;
                if (tu.addrMap.lineRangeToAddressRange(
                        src_file_name,
                        std::make_pair(beginSrcLine, endSrcLine),
                        addrRange))
                {
                    // Mapping data between the source and binary is available,
                    // emit JSON entries for it.
                    std::cout << "binary_file_path: "
                              << tu.addrMap.getBinaryPath() << std::endl;
                    std::cout << "      begin_addr: "
                              << std::hex << addrRange.first
                              << std::dec << std::endl;
                    std::cout << "        end_addr: "
                              << std::hex << addrRange.second
                              << std::dec << std::endl;
                    std::cout << " binary_contents: "
                              << tu.addrMap.getBinaryContentsAsStr(
                                  addrRange.first,
                                  addrRange.second)
                              << std::endl;
                }
            }
            continue;
        }
        
        if (cmd[0] == "get") {
            unsigned int tuid;
            EXPECT(cmd.size() == 3 || cmd.size() == 5,
                   "expected two arguments");
            EXPECT(Utils::read_uint(cmd[1], tuid),
                   "argument must be a translation unit id.");
            EXPECT(tuid < TUs.size(),
                   "out of range, only " << TUs.size()
                                         << " translation units loaded.");
            unsigned int ast;
            EXPECT(Utils::read_uint(cmd[2], ast), "expected an AST id.");
            EXPECT(ast > 0, "AST numbering starts at 1.");
            EXPECT(ast <= TUs[tuid].astTable.count(),
                   "out of range, only "
                   << TUs[tuid].astTable.count() << " ASTs.")
                
            std::ostringstream oss;
            ChainedOp op = {
                getTextAs (ast, "$result"),
                echoTo    (oss, "$result")
            };
            op.run(TUs[tuid], err);
            std::string result = oss.str();

            if (cmd.size() == 5) {
                // 'as' clause, bind the results to a named variable.
                EXPECT(cmd[3] == "as", "expected 'as'");
                EXPECT(cmd[4][0] == '$', "variable name must start with '$'");
                vars[cmd[4]] = result;
            }
            std::cout << result << DONE;
            continue;
        }

        if (cmd[0] == "cut" || cmd[0] == "c") {
            unsigned int tuid;
            EXPECT(cmd.size() > 2, "expected at least three arguments");
            EXPECT(Utils::read_uint(cmd[1], tuid), "argument must be an translation unit id.");
            RewritingOps ops;
            TU & tu = TUs[tuid];
            for (size_t i = 2; i < cmd.size(); ++i) {
                unsigned int n;
                EXPECT(Utils::read_uint(cmd[i], n),
                       "argument " << i << " must be an AST id.");
                EXPECT(n > 0, "AST numbering starts at 1.");
                EXPECT(n <= tu.astTable.count(),
                       "argument " << i << " out of range, only "
                       << tu.astTable.count() << " ASTs.");
                ops.push_back(setText(n, ""));
            }
            ChainedOp op(ops);
            if (op.then(printModifiedTo(std::cout))->run(tu, err))
                std::cout << DONE;
            else
                std::cout << err << CANCEL;
            continue;
        }

        if (cmd[0] == "set" || cmd[0] == "s") {
            unsigned int tuid;
            std::vector<const RewritingOp*> ops;
            EXPECT(cmd.size() >= 4, "expected at least three arguments");
            EXPECT(Utils::read_uint(cmd[1], tuid), "first argument must be a translation unit id.");
            size_t i;
            for (i = 2; i < cmd.size() - 1; i += 2) {
                unsigned int ast;
                EXPECT(Utils::read_uint(cmd[i], ast), "expected an AST id.");
                std::string text = cmd[i + 1];
                if (text[0] == '$') {
                    auto search = vars.find(text);
                    EXPECT(search != vars.end(), "variable " << text << " is unbound.");
                    text = search->second;
                }
                ops.push_back(setText(ast, text));
            }
            EXPECT(i == cmd.size(), "incomplete final replacement pair.");
            ChainedOp op(ops);
            if (op.then(printModifiedTo(std::cout))->run(TUs[tuid], err))
                std::cout << DONE;
            else
                std::cout << err << CANCEL;
            continue;
        }

        if (cmd[0] == "setrange" || cmd[0] == "sr") {
            unsigned int tuid;
            std::vector<const RewritingOp*> ops;
            EXPECT(cmd.size() >= 5, "expected at least four arguments");
            EXPECT(Utils::read_uint(cmd[1], tuid), "first argument must be a translation unit id.");
            size_t i;
            for (i = 2; i < cmd.size() - 1; i += 3) {
                unsigned int ast1;
                EXPECT(Utils::read_uint(cmd[i], ast1), "expected an AST id.");
                unsigned int ast2;
                EXPECT(Utils::read_uint(cmd[i + 1], ast2), "expected an AST id.");
                std::string text = cmd[i + 2];
                if (text[0] == '$') {
                    auto search = vars.find(text);
                    EXPECT(search != vars.end(), "variable " << text << " is unbound.");
                    text = search->second;
                }
                ops.push_back(setRangeText(ast1, ast2, text));
            }
            EXPECT(i == cmd.size(), "incomplete final replacement pair.");
            ChainedOp op(ops);
            if (op.then(printModifiedTo(std::cout))->run(TUs[tuid], err))
                std::cout << DONE;
            else
                std::cout << err << CANCEL;
            continue;
        }

        if (cmd[0] == "insert" || cmd[0] == "i") {
            unsigned int tuid;
            std::vector<const RewritingOp*> ops;
            EXPECT(cmd.size() >= 4, "expected at least three arguments");
            EXPECT(Utils::read_uint(cmd[1], tuid), "first argument must be a translation unit id.");
            size_t i;
            for (i = 2; i < cmd.size() - 1; i += 2) {
                unsigned int ast;
                EXPECT(Utils::read_uint(cmd[i], ast), "expected an AST id.");
                std::string text = cmd[i + 1];
                if (text[0] == '$') {
                    auto search = vars.find(text);
                    EXPECT(search != vars.end(), "variable " << text << " is unbound.");
                    text = search->second;
                }
                ops.push_back(insertBefore(ast, text));
            }
            EXPECT(i == cmd.size(), "incomplete final replacement pair.");
            ChainedOp op(ops);
            if (op.then(printModifiedTo(std::cout))->run(TUs[tuid], err))
                std::cout << DONE;
            else
                std::cout << err << CANCEL;
            continue;
        }

        if (cmd[0] == "swap") {
            unsigned int tuid;
            EXPECT(cmd.size() == 4, "expected exactly three arguments");
            EXPECT(Utils::read_uint(cmd[1], tuid), "first argument must be a translation unit id.");
            unsigned int ast1, ast2;
            EXPECT(Utils::read_uint(cmd[2], ast1), "expected an AST id.");
            EXPECT(Utils::read_uint(cmd[3], ast2), "expected an AST id.");
            TU & tu = TUs[tuid];
            ChainedOp op = {
                getTextAs (ast1, "$x"),
                getTextAs (ast2, "$y"),
                setText   (ast1, "$y"),
                setText   (ast2, "$x"),
                printModifiedTo (std::cout)
            };
            if (op.run(tu, err))
                std::cout << DONE;
            else
                std::cout << err << CANCEL;
           continue;
        }

        EXPECT(false, "unknown command \"" << cmd[0] << "\"");
    }
}

class BuildTU
    : public ASTConsumer
    , public RecursiveASTVisitor<BuildTU>
{
    typedef RecursiveASTVisitor<BuildTU> base;

    typedef std::set<clang::IdentifierInfo*> VarScope;
    
  public:
    BuildTU(StringRef Binary,
            StringRef DwarfFilepathMap,
            CompilerInstance * _ci)
        : Binary(Binary)
        , BinaryAddresses(Binary, DwarfFilepathMap)
        , ci(_ci)
        , sm(_ci->getSourceManager())
        , asts(TUs.back().astTable)
        , decl_scopes(TUs.back().scopes)
    {}

    ~BuildTU() {}

    virtual void HandleTranslationUnit(ASTContext &Context)
    {
        spine.clear();
        spine.push_back(NoAst);

        // Add an empty top-level variable scope
        var_scopes.clear();
        var_scopes.push_back(VarScope());
        
        // Run Recursive AST Visitor
        DeclDepth = 0;
        TraverseDecl(Context.getTranslationUnitDecl());
    }

    void RegisterFunctionDecl(const FunctionDecl * F)
    {
        // TODO, see ASTLister.cpp
    }

    template <typename T>
    AstRef makeAst(T * clang_obj, Requirements & required)
    {
        AstRef parent = spine.back();
        AstRef ast = asts.create(clang_obj, parent);
        spine.push_back(ast);
        Ast & newAst = asts[ast];

        newAst.setIncludes(required.includes());
        newAst.setMacros(required.macros());
        newAst.setTypes(required.types());
        newAst.setScopePosition(decl_scopes.current_scope_position());
        newAst.setFreeVariables(required.variables());
        newAst.setFreeFunctions(required.functions());
        
        Renames renames(required.variables(),
                        required.functions());
        RenameFreeVar renamer(clang_obj, sm, ci->getLangOpts(), renames);
        newAst.setText(renamer.getRewrittenString());
    
        if (parent != NoAst)
            asts[parent].add_child(ast);
        return ast;
    }
    
    bool VisitStmt(Stmt * s)
    {
        if (Utils::ShouldVisitStmt(sm, ci->getLangOpts(),
                                   sm.getMainFileID(), s))
        {
            Requirements reqs(ci, decl_scopes.get_names_in_scope());
            reqs.TraverseStmt(s);
            
            AstRef ast = makeAst(s, reqs);
            AstRef parent = asts[ast].parent();
            if (parent != NoAst && asts[parent].isStmt()) {
                asts[ast].setIsGuard(
                    Utils::IsGuardStmt(s, asts[parent].asStmt()));
            }
        }
        return true;
    }

    bool VisitDecl(Decl * d)
    {
        if (Utils::ShouldVisitDecl(sm, ci->getLangOpts(),
                                   sm.getMainFileID(), d))
        {
            if (isa<VarDecl>(d))
                decl_scopes.declare(static_cast<VarDecl*>(d)->getIdentifier());
            Requirements reqs(ci, decl_scopes.get_names_in_scope());
            reqs.TraverseDecl(d);

            makeAst(d, reqs);
        }
        return true;
    }
    
    bool TraverseStmt(Stmt * s)
    {
        if (Utils::ShouldVisitStmt(sm, ci->getLangOpts(),
                                   sm.getMainFileID(), s))
        {
            if (begins_scope(s)) {
                decl_scopes.enter_scope(NoAst);
                var_scopes.push_back(VarScope());
            }
            size_t original_spine_size = spine.size();
            bool keep_going = base::TraverseStmt(s);
            // If we created a new node, remove it from the spine
            // now that the traversal of this Stmt is complete.
            if (spine.size() > original_spine_size)
                spine.pop_back();
            assert (spine.size() == original_spine_size);

            if (begins_scope(s)) {
                var_scopes.pop_back();
                decl_scopes.exit_scope();
            }
            return keep_going;
        }
        else {
            return base::TraverseStmt(s);
        }
    }

    bool TraverseDecl(Decl * d)
    {
        if (Utils::ShouldVisitDecl(sm, ci->getLangOpts(),
                                   sm.getMainFileID(), d))
        {
            PTNode pos;
            if (begins_pseudoscope(d)) {
                pos = decl_scopes.current_scope_position();
            }
            size_t original_spine_size = spine.size();
            bool keep_going = base::TraverseDecl(d);
            // If we created a new node, remove it from the spine
            // now that the traversal of this Decl is complete.
            if (spine.size() > original_spine_size)
                spine.pop_back();
            assert (spine.size() == original_spine_size);
            if (begins_pseudoscope(d)) {
                decl_scopes.restore_scope_position(pos);
            }
            return keep_going;
        }
        else {
            return base::TraverseDecl(d);
        }
    }

  private:
    StringRef Binary;
    BinaryAddressMap BinaryAddresses;
    CompilerInstance * ci;
    SourceManager & sm;
    AstTable & asts;

    std::vector<AstRef> spine;

    unsigned int DeclDepth;
    std::vector<VarScope> var_scopes;
    Scope & decl_scopes;
};

} // namespace clang_mutate

std::unique_ptr<clang::ASTConsumer>
clang_mutate::CreateInteractive(clang::StringRef Binary,
                                clang::StringRef DwarfFilepathMap,
                                clang::CompilerInstance * CI)
{
    return std::unique_ptr<clang::ASTConsumer>(
               new BuildTU(Binary,
                           DwarfFilepathMap,
                           CI));
}
