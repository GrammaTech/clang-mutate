#include "EditBuffer.h"

#include "TU.h"

#include <map>
#include <sstream>

using namespace clang_mutate;

struct LinearEdit
{
    LinearEdit() : text(""), advance_by(0) {}
    std::string text;
    size_t advance_by;
};


std::string EditBuffer::preview(const std::string & source) const
{
    std::ostringstream oss;

    std::map<SourceOffset, LinearEdit> linear_edits;

    AstRef skipTo = NoAst;
    
    // Gather the linearized edits to perform on the buffer.
    for (auto & e : edits) {
        AstRef ast  = e.first;
        Edit const& edit = e.second;

        if (skipTo != NoAst && (skipTo->is_ancestor_of(ast) || ast < skipTo))
            continue;
        skipTo = NoAst;

        AstRef endAst = (edit.skipTo == NoAst) ? ast : edit.skipTo;
        SourceOffset start = ast->initial_normalized_offset() + edit.preAdjust;
        SourceOffset end = 1 + edit.postAdjust
            + endAst->final_normalized_offset();
        LinearEdit & ledit = linear_edits[start];
        LinearEdit & redit = linear_edits[end];
        ledit.text = ledit.text + edit.prefix;
        redit.text = edit.suffix + redit.text;
        if (edit.skipTo != NoAst) {
            skipTo = edit.skipTo;
            ledit.text += edit.text;
            ledit.advance_by = end - start;
            if (start > end) {
                std::cerr << "** unexpected condition in EditBuffer **"
                          << std::endl;
                ledit.advance_by = 1;
            }
        }
    }
 
    // Apply the edits in order, emitting the modified text.
    SourceOffset idx = 0;
    for (auto & e : linear_edits) {
        if (idx > e.first)
            continue;
        // Emit any unmodified text before this edit.
        while (idx < e.first)
            oss << source[idx++];
        // Perform the edit itself
        oss << e.second.text;
        idx += e.second.advance_by;
    }
    // Emit any unmodified text at the end of the buffer.
    while (idx < (int) source.size())
        oss << source[idx++];
    
    return oss.str();
}

void EditBuffer::insertBefore(AstRef ast, const std::string & text)
{
    Edit & edit = edits[ast];
    edit.prefix += text;
}

void EditBuffer::insertAfter(AstRef ast, const std::string & text)
{
    Edit & edit = edits[ast];
    edit.suffix = text + edit.suffix;
}

void EditBuffer::replaceText(AstRef ast, const std::string & text)
{
    Edit & edit = edits[ast];
    edit.skipTo = ast;
    edit.text = text;
}

void EditBuffer::replaceTextRange(AstRef stmt1, AstRef stmt2,
                                  const std::string & text,
                                  int preAdjust, int postAdjust)
{
    Edit & edit = edits[stmt1];
    edit.text = text;
    edit.skipTo = stmt2;
    edit.preAdjust = preAdjust;
    edit.postAdjust = postAdjust;
}
