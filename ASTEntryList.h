/*
 The ASTEntryList class is utilized for storing information about multiple
 entries in an AST.  It allows for loading from/storing to a JSON representation.
*/

#ifndef AST_ENTRY_LIST_HPP
#define AST_ENTRY_LIST_HPP

#include "ASTEntry.h"

#include "third-party/picojson-1.3.0/picojson.h"

#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <string>

namespace clang_mutate
{
  class ASTEntryList
  {
  public:
    ASTEntryList();
    ASTEntryList( const std::vector<ASTEntry*> &astEntries );

    ~ASTEntryList();

    void addEntry(ASTEntry *astEntry);
    ASTEntry* getEntry(unsigned int counter);

    bool isEmpty() const;

    std::string toString() const;
    picojson::value toJSON() const;

    std::ostream& toStream(std::ostream& out );
    llvm::raw_ostream& toStream(llvm::raw_ostream& out);

    std::ostream& toStreamJSON(std::ostream& out );
    llvm::raw_ostream& toStreamJSON(llvm::raw_ostream& out);

    bool toFile(const std::string& outfilePath );
    bool toFileJSON(const std::string& outfilePath );

  private:
    std::vector<ASTEntry*> m_astEntries;
  };
}

#endif
