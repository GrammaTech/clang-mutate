#ifndef EDIT_BUFFER_H
#define EDIT_BUFFER_H

#include "AstRef.h"

#include <string>

namespace clang_mutate {

struct Edit
{
    Edit() : prefix("")
           , text("")
           , suffix("")
           , skipTo(NoAst)
           , preAdjust(0)
           , postAdjust(0)
    {}
    std::string prefix;
    std::string text;
    std::string suffix;
    AstRef skipTo;
    int preAdjust, postAdjust;
};

class EditBuffer
{
public:
    EditBuffer() : edits() {}

    void insertBefore (AstRef ast, const std::string & text);
    void insertAfter  (AstRef ast, const std::string & text);
    void replaceText  (AstRef ast, const std::string & text);
    void replaceTextRange (AstRef start, AstRef end,
                           const std::string & text,
                           int preAdjust = 0, int postAdjust = 0);
    
    std::string preview(const std::string & source) const;
    
private:

    std::map<AstRef, Edit> edits;
};

} // end namespace clang_mutate

#endif
