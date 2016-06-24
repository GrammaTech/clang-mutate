#include "Cfg.h"

#include <clang/Analysis/CFG.h>

namespace clang_mutate {
using namespace clang;

static void addASTSuccessors(std::map<const Stmt *, Ast*> &ast_map,
                             std::unique_ptr<CFG> const &cfg,
                             AstRef body_ast, Ast *ast,
                             CFGBlock *next_block)
{
    // Sometimes coercing AdjacentBlock to CFGBlock returns NULL. Just
    // ignore those.
    if (!next_block)
        return;

    // Denote function exit with the AST of the function body.
    // This is a little weird but it matches the way we
    // instrument, and that AST won't otherwise appear in the
    // CFG.
    if (next_block->getBlockID() == cfg->getExit().getBlockID())
        ast->add_successor(body_ast);

    // Traverse empty blocks until we find statements
    if (next_block->empty()) {
        for (auto s = next_block->succ_begin(); s != next_block->succ_end(); s++)
            addASTSuccessors(ast_map, cfg, body_ast, ast, *s);
        return;
    }

    // Non-empty block: add its first statement as a sucessor
    llvm::Optional<CFGStmt> first = next_block->front().getAs<CFGStmt>();
    if (first.hasValue()) {
        const Stmt *next_s = first->getStmt();
        if (ast_map[next_s])
            ast->add_successor(ast_map[next_s]->counter());
    }
}

void GenerateCFG(TU &TU, CompilerInstance const &CI, ASTContext &Context)
{
    std::map<const Stmt *, Ast*> ast_map;
    for (auto a : TU.asts) {
        if (a->isStmt()) {
            ast_map[a->asStmt(CI)] = a;
        }
    }

    for (auto f : TU.function_starts) {
        AstRef body_ast = f.first;
        AstRef decl = body_ast->parent();

        std::unique_ptr<CFG> cfg =
            CFG::buildCFG(decl->asDecl(CI),
                          body_ast->asStmt(CI),
                          &Context, CFG::
                          BuildOptions());

        for (auto block : *cfg) {
            // Find the terminator (if the block ends with a control flow
            // statement), or the final statement in the block.
            const Stmt *s = block->getTerminator();
            if (!s) {
                llvm::Optional<CFGStmt> last = block->back().getAs<CFGStmt>();
                if (last.hasValue())
                    s = last->getStmt();
            }
            if (!s)
                continue;
            Ast *end_ast = ast_map[s];
            if (!end_ast)
                continue;

            // All statements within the block are succeeded by the final
            // statement.
            for (auto elt : *block) {
                llvm::Optional<CFGStmt> stmt = elt.getAs<CFGStmt>();
                if (stmt.hasValue() && stmt->getStmt() != NULL) {
                    Ast *ast = ast_map[stmt->getStmt()];
                    if (ast && ast != end_ast)
                        ast->add_successor(end_ast->counter());
                }
            }

            // Add successors for the final statement
            for (auto s = block->succ_begin(); s != block->succ_end(); s++) {
                addASTSuccessors(ast_map, cfg, body_ast, end_ast, *s);
            }
        }
    }
}

} // namespace clang_mutate
