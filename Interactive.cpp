#include "Interactive.h"

#include "AST.h"
#include "BinaryAddressMap.h"
#include "Rewrite.h"
#include "Utils.h"

#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Core/Rewriter.h"

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

std::vector<std::pair<CompilerInstance*, AstTable> > TUs;

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

        if (cmd[0] == "quit")
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
            for (auto it = TUs.begin(); it != TUs.end(); ++it) {
                SourceManager & sm = it->first->getSourceManager();
                AstTable & asts = it->second;
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
            for (auto it = TUs.begin(); it != TUs.end(); ++n, ++it) {
                AstTable & asts = it->second;
                std::string name = filenames[n];
                std::cout << "| " << std::setw(4) << n << " | "
                          << std::setw(6) << asts.count() << " | "
                          << name;
                PAD(name.size() + 1);
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
            std::pair<CompilerInstance*, AstTable> & tu = TUs[tuid];
            for (size_t i = 2; i < cmd.size(); ++i) {
                unsigned int n;
                EXPECT(Utils::read_uint(cmd[i], n),
                       "argument " << i << " must be an AST id.");
                EXPECT(n > 0, "AST numbering starts at 1.");
                EXPECT(n <= tu.second.count(),
                       "argument " << i << " out of range, only "
                       << tu.second.count() << " ASTs.");
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
            for (i = 2; i < cmd.size() - 1; i += 2) {
                unsigned int ast1;
                EXPECT(Utils::read_uint(cmd[i], ast1), "expected an AST id.");
                unsigned int ast2;
                EXPECT(Utils::read_uint(cmd[i], ast2), "expected an AST id.");
                std::string text = cmd[i + 1];
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
            std::pair<CompilerInstance*,AstTable> & tu = TUs[tuid];
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

class PrepareInteractiveSession
    : public ASTConsumer
    , public RecursiveASTVisitor<PrepareInteractiveSession>
{
    typedef RecursiveASTVisitor<PrepareInteractiveSession> base;

  public:
    PrepareInteractiveSession(StringRef Binary,
                              StringRef DwarfFilepathMap,
                              CompilerInstance * _ci)
        : Binary(Binary)
        , BinaryAddresses(Binary, DwarfFilepathMap)
        , ci(_ci)
        , sm(_ci->getSourceManager())
        , TU(TUs.back().second)
    {}

    ~PrepareInteractiveSession() {}

    virtual void HandleTranslationUnit(ASTContext &Context)
    {
        spine.clear();
        spine.push_back(NoAst);
        TraverseDecl(Context.getTranslationUnitDecl());
    }

    bool VisitStmt(Stmt * s)
    {
        if (Utils::ShouldVisitStmt(sm, ci->getLangOpts(),
                                   sm.getMainFileID(), s))
        {
            AstRef parent = spine.back();
            AstRef ast = TU.create(s, parent);
            if (parent != NoAst)
                TU[parent].add_child(ast);
            spine.push_back(ast);
        }
        return true;
    }

    bool TraverseStmt(Stmt * s)
    {
        size_t original_spine_size = spine.size();
        bool keep_going = base::TraverseStmt(s);
        // If we created a new node, remove it from the spine
        // now that the traversal of this Stmt is complete.
        if (spine.size() > original_spine_size)
            spine.pop_back();
        assert (spine.size() == original_spine_size);
        return keep_going;
    }

  private:
    StringRef Binary;
    BinaryAddressMap BinaryAddresses;
    CompilerInstance * ci;
    SourceManager & sm;
    AstTable & TU;

    std::vector<AstRef> spine;
};

} // namespace clang_mutate

std::unique_ptr<clang::ASTConsumer>
clang_mutate::CreateInteractive(clang::StringRef Binary,
                                clang::StringRef DwarfFilepathMap,
                                clang::CompilerInstance * CI)
{
    return std::unique_ptr<clang::ASTConsumer>(
               new PrepareInteractiveSession(Binary,
                                             DwarfFilepathMap,
                                             CI));
}
