#include "Interactive.h"

#include "TU.h"
#include "Ast.h"
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

} // namespace clang_mutate
