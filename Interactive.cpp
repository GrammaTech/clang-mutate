#include "Interactive.h"

#include "TU.h"
#include "AST.h"
#include "BinaryAddressMap.h"
#include "Rewrite.h"
#include "TypeDBEntry.h"
#include "AuxDB.h"
#include "Utils.h"
#include "Parser.h"

#include <iostream>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <set>

namespace clang_mutate {
using namespace clang;
using namespace parser_templates;

std::map<std::string, bool> interactive_flags;

#define DONE   \
    (interactive_flags["ctrl"] ? std::string("\x17") : "") \
    << (interactive_flags["prompt"] ? "\n" : "") << std::flush
#define CANCEL \
    (interactive_flags["ctrl"] ? std::string("\x18") : "") \
    << (interactive_flags["prompt"] ? "\n" : "") << std::flush


//////////////////////////////////////////////////////////
//
//  The main interactive loop
//

extern const char quit_s[] = "quit";
extern const char q_s   [] = "q";

void runInteractiveSession(std::istream & input)
{
    RewriterState state;

    std::string prompt = interactive_flags["prompt"]
        ? "clang-mutate> "
        : "";

    while(true) {
        std::string cmdline;
        std::cout << prompt << std::flush;
        if (!std::getline(input, cmdline))
            break;

        parser_context ctx(cmdline);

        if (matches<many<whitespace>, eof>(ctx))
            continue;

        if (matches<tokens<alt<str<quit_s>,str<q_s>>>, eof>(ctx))
            break;

        parsed<RewritingOpPtr> parsed_op =
            parse<sequence_<interactive_op, eof>>(ctx);

        if (!ctx.ok()) {
            for (size_t i = 0; i < prompt.size(); ++i)
                std::cerr << " ";
            ctx.indent(std::cerr);
            std::cerr << std::endl;
            std::cerr << "** parse error: " << ctx.error() << std::endl;
            std::cout << CANCEL;
            continue;
        }

        // In REPL-mode, if the last operation was a mutation, first print
        // the resulting edit buffer to $$.
        if (interactive_flags["prompt"]) {
            TURef tu;
            if (parsed_op.result->edits_tu(tu))
                parsed_op.result = parsed_op.result->then(printModified(tu));
            parsed_op.result = parsed_op.result->then(echo("$$"));
        }

        if (parsed_op.result->run(state)) {
            std::cout << DONE;
        }
        else {
            std::cerr << "** rewriting error: " << state.message << std::endl;
            std::cout << CANCEL;
        }
    }

    // In non-REPL-mode, print the final value of $$.
    if (!interactive_flags["prompt"]) {
        if (echo("$$")->run(state))
            std::cout << DONE;
        else
            std::cout << CANCEL;
    }
}

/*
void runInteractiveSession_(std::istream & input)
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

        if (cmd[0] == "setfunc") {
            unsigned int tuid;
            RewritingOps ops;
            EXPECT(cmd.size() == 4, "expected three arguments");
            EXPECT(Utils::read_uint(cmd[1], tuid), "first argument must be a translation unit id.");
            TU & tu = TUs[tuid];
            unsigned int ast;
            EXPECT(Utils::read_uint(cmd[2], ast), "expected an AST id.");
            std::string text = cmd[3];
            if (text[0] == '$') {
                auto search = vars.find(text);
                EXPECT(search != vars.end(), "variable " << text << " is unbound.");
                text = search->second;
            }

            // Find a function decl with body = ast, or fail.
            auto fsearch = tu.function_ranges.find(ast);
            EXPECT(fsearch != tu.function_ranges.end(),
                   "AST " << ast << " is not a function body.");
            RewritingOpPtr op = chain({
                    setRangeText(fsearch->second, text),
                    printModifiedTo(std::cout)
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
*/

} // namespace clang_mutate
