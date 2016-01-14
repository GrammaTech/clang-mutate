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
    ASTEntry* getEntry(unsigned int counter) const;

    bool isEmpty() const;

    std::string toString(unsigned int counter = -1) const;
    picojson::value toJSON(unsigned int counter = -1,
                           bool include_types = true,
                           const std::set<ASTEntryField> &fields = 
                           ASTEntryField::getDefaultFields()) const;

    std::ostream& toStream(std::ostream& out, 
                           unsigned int counter = -1) const;
    llvm::raw_ostream& toStream(llvm::raw_ostream& out, 
                                unsigned int counter = -1) const;

    std::ostream& toStreamJSON(std::ostream& out, 
                               unsigned int counter = -1,
                               bool include_types = true,
                               const std::set<ASTEntryField> &fields = 
                               ASTEntryField::getDefaultFields()) const;
    llvm::raw_ostream& toStreamJSON(llvm::raw_ostream& out, 
                                    unsigned int counter = -1,
                                    bool include_types = true,
                                    const std::set<ASTEntryField> &fields = 
                                    ASTEntryField::getDefaultFields()) const;

    bool toFile(const std::string& outfilePath, 
                unsigned int counter = -1) const;
    bool toFileJSON(const std::string& outfilePath, 
                    unsigned int counter = -1,
                    bool include_types = true,
                    const std::set<ASTEntryField> &fields = 
                    ASTEntryField::getDefaultFields()) const;

  private:
    std::vector<ASTEntry*> m_astEntries;
  };
}

#endif
