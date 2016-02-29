#include "Interactive.h"

#include "AST.h"
#include "BinaryAddressMap.h"
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
#define EXPECT(p, msg) \
    if (!(p)) { std::cout << "** " << msg << CANCEL; continue; }

std::vector<std::pair<CompilerInstance*, AstTable> > TUs;

// Interactive actions
void printSource(CompilerInstance * ci);

void get(CompilerInstance * ci,
         AstTable & asts,
         AstRef ast);

void set(CompilerInstance * ci,
         AstTable & asts,
         std::set<std::pair<AstRef, std::string> > & replacements);

std::string getAstText(CompilerInstance * ci,
                       AstTable & asts,
                       AstRef ast);


void runInteractiveSession(std::istream & input)
{
    interactive_flags["ctrl"] = false;
    
    std::cout << "***** begin clang-mutate interactive session *****" << std::endl;
    std::cout << "processed " << TUs.size() << " translation units" << std::endl;

    while (true) {

        std::string cmdline;
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
            std::map<std::string, bool>::iterator search =
                interactive_flags.find(cmd[1]);
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
            continue;
        }
        
        if (cmd[0] == "print") {
            unsigned int n;
            EXPECT(cmd.size() == 2, "expected one argument");
            EXPECT(Utils::read_uint(cmd[1], n), "expected a translation unit id");
            EXPECT(n < TUs.size(),
                   "out of range, only " << TUs.size()
                                         << " translation units loaded.");
            printSource(TUs[n].first);
            continue;
        }

        if (cmd[0] == "get") {
            unsigned int n;
            EXPECT(cmd.size() == 3, "expected two arguments");
            EXPECT(Utils::read_uint(cmd[1], n), "argument must be a translation unit id.");
            EXPECT(n < TUs.size(),
                   "out of range, only " << TUs.size()
                                         << " translation units loaded.");
            unsigned int ast;
            EXPECT(Utils::read_uint(cmd[2], ast), "expected an AST id.");
            get(TUs[n].first, TUs[n].second, ast);
            continue;
        }

        if (cmd[0] == "cut") {
            unsigned int n;
            EXPECT(cmd.size() > 2, "expected at least three arguments");
            EXPECT(Utils::read_uint(cmd[1], n), "argument must be an translation unit id.");
            CompilerInstance * ci = TUs[n].first;
            AstTable & asts = TUs[n].second;
            std::set<std::pair<AstRef, std::string> > targets;
            for (size_t i = 2; i < cmd.size(); ++i) {
                EXPECT(Utils::read_uint(cmd[i], n),
                       "argument " << i << " must be an AST id.");
                EXPECT(n > 0, "AST numbering starts at 1.");
                EXPECT(n <= asts.count(),
                       "argument " << i << " out of range, only "
                       << asts.count() << " ASTs.");
                targets.insert(std::make_pair(n, ""));
            }
            set(ci, asts, targets);
            continue;
        }

        if (cmd[0] == "set") {
            unsigned int tuid;
            std::set<std::pair<AstRef, std::string> > replacements;
            EXPECT(cmd.size() >= 4, "expected at least three arguments");
            EXPECT(Utils::read_uint(cmd[1], tuid), "first argument must be a translation unit id.");
            size_t i;
            for (i = 2; i < cmd.size() - 1; i += 2) {
                unsigned int ast;
                EXPECT(Utils::read_uint(cmd[i], ast), "expected an AST id.");
                replacements.insert(std::make_pair(ast, cmd[i + 1]));
            }
            EXPECT(i == cmd.size(), "incomplete final replacement pair.");
            set(TUs[tuid].first, TUs[tuid].second, replacements);
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
            std::string text1 = getAstText(tu.first, tu.second, ast1);
            std::string text2 = getAstText(tu.first, tu.second, ast2);
            std::set<std::pair<AstRef, std::string> > replacements;
            replacements.insert(std::make_pair(ast1, text2));
            replacements.insert(std::make_pair(ast2, text1));
            set(tu.first, tu.second, replacements);
            continue;
        }

        EXPECT(false, "unknown command \"" << cmd[0] << "\"");
    }
}

void printSource(CompilerInstance * ci)
{
    SourceManager & sm = ci->getSourceManager();
    std::cout << sm.getBufferData(sm.getMainFileID()).str() << DONE;
}

std::string getAstText(
    CompilerInstance * ci,
    AstTable & asts,
    AstRef ast)
{
    SourceManager & sm = ci->getSourceManager();
    SourceRange r = //normalizedSourceRange(asts[*it]); <- see Renaming.cpp:42
        Utils::getImmediateMacroArgCallerRange(
            sm, asts[ast].sourceRange());
    SourceLocation begin = r.getBegin();
    SourceLocation end = Lexer::getLocForEndOfToken(r.getEnd(),
                                                    0,
                                                    sm,
                                                    ci->getLangOpts());
    return Lexer::getSourceText(
        CharSourceRange::getCharRange(begin, end),
        sm,
        ci->getLangOpts());
}

void get(CompilerInstance * ci,
         AstTable & asts,
         AstRef ast)
{
    std::cout << getAstText(ci, asts, ast) << DONE;
}

void set(CompilerInstance * ci,
         AstTable & asts,
         std::set<std::pair<AstRef, std::string> > & targets)
{
    SourceManager & sm = ci->getSourceManager();
    Rewriter rewriter;
    rewriter.setSourceMgr(sm, ci->getLangOpts());

    for (auto it = targets.rbegin(); it != targets.rend(); ++it) {
        SourceRange r = //normalizedSourceRange(asts[*it]);
          Utils::getImmediateMacroArgCallerRange(
              sm, asts[it->first].sourceRange());
        rewriter.ReplaceText(r, StringRef(it->second));
    }

    FileID file = sm.getMainFileID();
    const RewriteBuffer * buf = rewriter.getRewriteBufferFor(file);
    std::cout << std::string(buf->begin(), buf->end()) << DONE;
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
