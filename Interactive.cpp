#include "Interactive.h"

#include "TU.h"
#include "AST.h"
#include "BinaryAddressMap.h"
#include "Rewrite.h"
#include "TypeDBEntry.h"
#include "AuxDB.h"
#include "Utils.h"

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


//////////////////////////////////////////////////////////
//
//  AST annotators
//

class ClassAnnotator : public Annotator
{
public:
    std::string before(const Ast & ast) {
        std::ostringstream oss;
        oss << "/* " << ast.counter()
            << ":" << ast.className() << "[ */";
        return oss.str();
    }

    std::string after(const Ast & ast) {
        std::ostringstream oss;
        oss << "/* ]" << ast.counter() << " */";
        return oss.str();
    }

    std::string describe() { return "class-and-counter"; }
};

ClassAnnotator classAnnotator;

class CounterAnnotator : public Annotator
{
public:
    std::string before(const Ast & ast) {
        std::ostringstream oss;
        oss << "/* " << ast.counter() << "[ */";
        return oss.str();
    }

    std::string after(const Ast & ast) {
        std::ostringstream oss;
        oss << "/* ]" << ast.counter() << " */";
        return oss.str();
    }

    std::string describe() { return "counter"; }
};

CounterAnnotator counterAnnotator;

//////////////////////////////////////////////////////////
//
//  The main interactive loop
//

void runInteractiveSession(std::istream & input)
{
    NamedText vars;
    
    if (interactive_flags["prompt"]) {
        std::cout << "***** begin clang-mutate interactive session *****"
                  << std::endl
                  << "processed " << TUs.size() << " translation units"
                  << std::endl;
    }

    while (true) {

        std::string cmdline;
        std::string err;
        if (interactive_flags["prompt"])
            std::cout << "clang-mutate> " << std::flush;
        if (!std::getline(input, cmdline))
            break;
        std::vector<std::string> cmd = Utils::tokenize(cmdline);
        
        if (cmd.size() == 0)
            continue;

        if (cmd[0] == "quit" || cmd[0] == "q")
            break;

        if (cmd[0] == "clear") {
            vars.clear();
            continue;
        }
            
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

        if (cmd[0] == "types") {
            EXPECT (cmd.size() == 1, "expected no arguments");
            std::cout << to_json(TypeDBEntry::databaseToJSON()) << DONE;
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

        if (cmd[0] == "ids") {
            unsigned int tuid;
            EXPECT (cmd.size() == 2, "expected one argument");
            EXPECT (Utils::read_uint(cmd[1], tuid),
                    "argument must be a translation unit id");
            EXPECT (tuid < TUs.size(),
                    "out of range, only " << TUs.size()
                                          << " translation units loaded.");
            std::cout << TUs[tuid].astTable.count() << DONE;
            continue;
        }

        if (cmd[0] == "annotate") {
            unsigned int tuid;
            EXPECT (cmd.size() == 2, "expected one argument");
            EXPECT (Utils::read_uint(cmd[1], tuid),
                    "argument must be a translation unit id");
            EXPECT (tuid < TUs.size(),
                    "out of range, only " << TUs.size()
                                          << " translation units loaded.");
            ChainedOp op = {
                annotateWith (&classAnnotator),
                printModifiedTo (std::cout)
            };
            std::string err;
            if (op.run(TUs[tuid], err))
                std::cout << DONE;
            else
                std::cout << err << CANCEL;
            continue;
        }
        
        if (cmd[0] == "number") {
            unsigned int tuid;
            EXPECT (cmd.size() == 2, "expected one argument");
            EXPECT (Utils::read_uint(cmd[1], tuid),
                    "argument must be a translation unit id");
            EXPECT (tuid < TUs.size(),
                    "out of range, only " << TUs.size()
                                          << " translation units loaded.");
            ChainedOp op = {
                annotateWith (&counterAnnotator),
                printModifiedTo (std::cout)
            };
            std::string err;
            if (op.run(TUs[tuid], err))
                std::cout << DONE;
            else
                std::cout << err << CANCEL;
            continue;
        }

        if (cmd[0] == "list") {
            unsigned int tuid;
            EXPECT (cmd.size() == 2, "expected one argument");
            EXPECT (Utils::read_uint(cmd[1], tuid),
                    "argument must be a translation unit id");
            EXPECT (tuid < TUs.size(),
                    "out of range, only " << TUs.size()
                                          << " translation units loaded.");
            TU & tu = TUs[tuid];
            SourceManager & sm = tu.ci->getSourceManager();
            
            std::vector<picojson::value> ans;
            std::set<std::string> all_keys;
            for (auto & ast : tu.astTable) {
                char msg[256];
                SourceRange r = ast.sourceRange();
                sprintf(msg, "%8d %6d:%-3d %6d:%-3d %-25s",
                        (unsigned int) ast.counter(),
                        sm.getSpellingLineNumber   (r.getBegin()),
                        sm.getSpellingColumnNumber (r.getBegin()),
                        sm.getSpellingLineNumber   (r.getEnd()),
                        sm.getSpellingColumnNumber (r.getEnd()),
                        ast.className().c_str());
                BinaryAddressMap::BeginEndAddressPair addrRange;
                if (ast.binaryAddressRange(tu, addrRange)) {
                    sprintf(msg + strlen(msg), " %#016lx %#016lx",
                            addrRange.first,
                            addrRange.second);
                }
                std::cout << msg << std::endl;
            }
            std::cout << DONE;
            continue;
        }
        
        if (cmd[0] == "aux") {
            unsigned int tuid;
            EXPECT (cmd.size() == 2, "expected one argument");
            EXPECT (Utils::read_uint(cmd[1], tuid),
                    "argument must be a translation unit id");
            EXPECT (tuid < TUs.size(),
                    "out of range, only " << TUs.size()
                                          << " translation units loaded.");
            TU & tu = TUs[tuid];
            std::map<std::string, picojson::value> ans;
            for (auto it = tu.aux.begin(); it != tu.aux.end(); ++it)
                ans[it->first] = to_json(it->second);
            std::cout << to_json(ans) << DONE;
            continue;
        }

        if (cmd[0] == "json") {
            unsigned int tuid;
            EXPECT (cmd.size() >= 2, "expected at least one argument");
            EXPECT (Utils::read_uint(cmd[1], tuid),
                    "argument must be a translation unit id");
            EXPECT (tuid < TUs.size(),
                    "out of range, only " << TUs.size()
                                          << " translation units loaded.");
            TU & tu = TUs[tuid];
            std::string sep = "";

            std::cout << "[";
            std::set<std::string> aux_keys;
            if (cmd.size() == 2) {
                aux_keys.insert("types");
                aux_keys.insert("decls");
                aux_keys.insert("protos");
            }
            if (cmd.size() > 2) {
                for (size_t i = 2; i < cmd.size(); ++i) {
                    aux_keys.insert(cmd[i]);
                }
            }
            for (auto & aux_key : aux_keys) {
                if (aux_key == "types") {
                    for (auto & entry : TypeDBEntry::databaseToJSON()) {
                        std::cout << sep << to_json(entry);
                        sep = ",";
                    }
                }
                else {
                    for (auto & entry : tu.aux[aux_key]) {
                        std::cout << sep << to_json(entry);
                        sep = ",";
                    }
                }
            }
            
            std::set<std::string> all_ast_keys;
            for (auto & ast : tu.astTable) {
                std::cout << sep << ast.toJSON(all_ast_keys, tu);
                sep = ",";
            }
            std::cout << "]" << DONE;
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

            std::set<std::string> all_keys;
            std::cout << asts[ast].toJSON(all_keys, tu) << DONE;
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
            if (chain(ops)->then(printModifiedTo(std::cout))->run(tu, err))
                std::cout << DONE;
            else
                std::cout << err << CANCEL;
            continue;
        }

        if (cmd[0] == "set" || cmd[0] == "s") {
            unsigned int tuid;
            RewritingOps ops;
            EXPECT(cmd.size() >= 4, "expected at least three arguments");
            EXPECT(Utils::read_uint(cmd[1], tuid), "first argument must be a translation unit id.");
            size_t i;
            for (i = 2; i < cmd.size() - 1; i += 2) {
                unsigned int ast;
                EXPECT(Utils::read_uint(cmd[i], ast),
                       "expected an AST id, got \"" << cmd[i] << "\"");
                std::string text = cmd[i + 1];
                if (text[0] == '$') {
                    auto search = vars.find(text);
                    EXPECT(search != vars.end(), "variable " << text << " is unbound.");
                    text = search->second;
                }
                std::cerr << "   " << ast << " : \"" << text << "\"" << std::endl;
                ops.push_back(setText(ast, text));
            }
            EXPECT(i == cmd.size(), "incomplete final replacement pair.");
            if (chain(ops)->then(printModifiedTo(std::cout))->run(TUs[tuid], err))
                std::cout << DONE;
            else
                std::cout << err << CANCEL;
            continue;
        }

        if (cmd[0] == "setrange" || cmd[0] == "sr") {
            unsigned int tuid;
            RewritingOps ops;
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
            if (chain(ops)->then(printModifiedTo(std::cout))->run(TUs[tuid], err))
                std::cout << DONE;
            else
                std::cout << err << CANCEL;
            continue;
        }

        if (cmd[0] == "insert" || cmd[0] == "i") {
            unsigned int tuid;
            RewritingOps ops;
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
            if (chain(ops)->then(printModifiedTo(std::cout))->run(TUs[tuid], err))
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
            RewritingOpPtr op = chain({
                getTextAs (ast1, "$x"),
                getTextAs (ast2, "$y"),
                setText   (ast1, "$y"),
                setText   (ast2, "$x"),
                printModifiedTo (std::cout)
                    });
            if (op->run(tu, err))
                std::cout << DONE;
            else
                std::cout << err << CANCEL;
           continue;
        }

        EXPECT(false, "unknown command \"" << cmd[0] << "\"");
    }
}

} // namespace clang_mutate
