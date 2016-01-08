/*
 The classes within this file are utilized for storing information about a single entry in 
 an AST.  They allow for loading from/storing to a JSON representation.
*/

#ifndef AST_ENTRY_HPP
#define AST_ENTRY_HPP

#include "BinaryAddressMap.h"
#include "Renaming.h"
#include "Macros.h"
#include "Utils.h"

#include "clang/AST/AST.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "third-party/picojson-1.3.0/picojson.h"

#include <string>
#include <map>

namespace clang_mutate
{
  typedef std::vector<std::vector<std::string> > ScopedNames;

  class ASTEntryField 
  {
  public:
    static ASTEntryField NULL_FIELD;
    static ASTEntryField COUNTER;
    static ASTEntryField PARENT_COUNTER;
    static ASTEntryField AST_CLASS;
    static ASTEntryField SRC_FILE_NAME;
    static ASTEntryField BEGIN_SRC_LINE;
    static ASTEntryField BEGIN_SRC_COL;
    static ASTEntryField END_SRC_LINE;
    static ASTEntryField END_SRC_COL;
    static ASTEntryField SRC_TEXT;
    static ASTEntryField GUARD_STMT;
    static ASTEntryField FULL_STMT;
    static ASTEntryField UNBOUND_VALS;
    static ASTEntryField UNBOUND_FUNS;
    static ASTEntryField MACROS;
    static ASTEntryField TYPES;
    static ASTEntryField STMT_LIST;
    static ASTEntryField OPCODE;
    static ASTEntryField SCOPES;
    static ASTEntryField BINARY_FILE_PATH;
    static ASTEntryField BEGIN_ADDR;
    static ASTEntryField END_ADDR;
    static ASTEntryField BINARY_CONTENTS;

    static ASTEntryField fromJSONName(const std::string &jsonName);
    static std::set<ASTEntryField> fromJSONNames(const std::vector<std::string> 
                                                 &jsonNames);
    static std::set<ASTEntryField> getDefaultFields();

    std::string getJSONName() const;

    bool operator==(const ASTEntryField &other) const;
    bool operator<(const ASTEntryField &other) const;
  private:
    ASTEntryField(const std::string &jsonName);
    std::string m_jsonName;
  };

  // Abstract base class for all AST entries.
  // Includes methods to convert to a string or JSON representation.
  class ASTEntry
  {
  public:
    virtual ~ASTEntry() {}
    virtual ASTEntry* clone() const = 0;
    virtual unsigned int getCounter() const = 0;
    virtual std::string toString() const = 0;
    virtual picojson::value toJSON(const std::set<ASTEntryField> &fields =
                                   ASTEntryField::getDefaultFields()) const = 0;

    virtual void set_stmt_list(const std::vector<unsigned int> &) {}
    virtual std::vector<unsigned int> get_stmt_list() const
    { return std::vector<unsigned int>(); }

  };

  class ASTEntryFactory
  {
  public:
    static ASTEntry* make( clang::Stmt *s, 
                           clang::Stmt *p,
                           const std::map<clang::Stmt *, unsigned int> &spine,
                           clang::Rewriter& rewrite,
                           BinaryAddressMap &binaryAddressMap,
                           const Renames & renames,
                           const Macros & macros,
                           const std::set<Hash> & types,
                           const ScopedNames & scoped_names);
  private:
    ASTEntryFactory() {}
  };

  // AST entry without binary information
  class ASTNonBinaryEntry : public ASTEntry
  {
  public:
    ASTNonBinaryEntry();

    ASTNonBinaryEntry( const unsigned int counter,
                       const unsigned int parentCounter,
                       const std::string &astClass,
                       const std::string &srcFileName,
                       const unsigned int beginSrcLine,
                       const unsigned int beginSrcCol,
                       const unsigned int endSrcLine,
                       const unsigned int endSrcCol,
                       const std::string &srcText,
                       const bool guardStmt,
                       const bool fullStmt,
                       const Renames & renames,
                       const Macros & macros,
                       const std::set<Hash> & types,
                       const std::vector<unsigned int> & stmt_list,
                       const std::string & opcode,
                       const ScopedNames & scoped_names );

    ASTNonBinaryEntry( clang::Stmt * s,
                       clang::Stmt * p,
                       const std::map<clang::Stmt *, unsigned int> &spine,
                       clang::Rewriter& rewrite,
                       const Renames & renames,
                       const Macros & macros,
                       const std::set<Hash> & types,
                       const ScopedNames & scoped_names );

    virtual ~ASTNonBinaryEntry();

    virtual ASTEntry* clone() const;

    virtual unsigned int getCounter() const;
    virtual unsigned int getParentCounter() const;
    virtual std::string getASTClass() const;
    virtual std::string getSrcFileName() const;
    virtual unsigned int getBeginSrcLine() const;
    virtual unsigned int getBeginSrcCol() const;
    virtual unsigned int getEndSrcLine() const;
    virtual unsigned int getEndSrcCol() const;
    virtual std::string getSrcText() const;
    virtual bool getIsGuardStmt() const;
    virtual bool getIsFullStmt() const;
    virtual Renames getRenames() const;
    virtual Macros getMacros() const;
    virtual std::set<Hash> getTypes() const;
    virtual std::vector<unsigned int> getStmtList() const;
    virtual std::string getOpcode() const;
    virtual ScopedNames getScopedNames() const;
    
    virtual std::string toString() const;
    virtual picojson::value toJSON(const std::set<ASTEntryField> &fields =
                                   ASTEntryField::getDefaultFields()) const;

    virtual void set_stmt_list(const std::vector<unsigned int> & stmt_list)
    { m_stmt_list = stmt_list; }
    virtual std::vector<unsigned int> get_stmt_list() const { return m_stmt_list; }

  private:
    unsigned int m_counter;
    unsigned int m_parentCounter;
    std::string  m_astClass;
    std::string  m_srcFileName;
    unsigned int m_beginSrcLine;
    unsigned int m_beginSrcCol;
    unsigned int m_endSrcLine;
    unsigned int m_endSrcCol;
    std::string  m_srcText;
    bool m_guardStmt;
    bool m_fullStmt;
    Renames m_renames;
    Macros m_macros;
    std::set<Hash> m_types;
    std::vector<unsigned int> m_stmt_list;
    std::string m_opcode;
    ScopedNames m_scoped_names;
  };

  // AST entry with binary information
  class ASTBinaryEntry : public ASTNonBinaryEntry
  {
  public:
    ASTBinaryEntry();

    ASTBinaryEntry( const unsigned int counter, 
                    const unsigned int parentCounter,
                    const std::string &astClass,
                    const std::string &srcFileName,
                    const unsigned int beginSrcLine,
                    const unsigned int beginSrcCol,
                    const unsigned int endSrcLine,
                    const unsigned int endSrcCol,
                    const std::string &srcText,
                    const bool guardStmt,
                    const bool fullStmt,
                    const Renames & renames,
                    const Macros & macros,
                    const std::set<Hash> & types,
                    const std::vector<unsigned int> & stmt_list,
                    const std::string & opcode,
                    const ScopedNames & scoped_names,
                    const std::string &binaryFileName,
                    const unsigned long beginAddress,
                    const unsigned long endAddress,
                    const std::string &binaryContents );

    ASTBinaryEntry( clang::Stmt * s,
                    clang::Stmt * p,
                    const std::map<clang::Stmt*, unsigned int> &spine,
                    clang::Rewriter& rewrite,
                    BinaryAddressMap& binaryAddressMap,
                    const Renames & renames,
                    const Macros & macros,
                    const std::set<Hash> & types,
                    const ScopedNames & scoped_names );

    virtual ~ASTBinaryEntry();

    virtual ASTEntry* clone() const;

    virtual std::string getBinaryFilePath() const;
    virtual unsigned long getBeginAddress() const;
    virtual unsigned long getEndAddress() const;
    virtual std::string getBinaryContents() const;

    virtual std::string toString() const;
    virtual picojson::value toJSON(const std::set<ASTEntryField> &fields =
                                   ASTEntryField::getDefaultFields()) const;
    
  private:
    std::string       m_binaryFilePath;
    unsigned long     m_beginAddress;
    unsigned long     m_endAddress;
    std::string       m_binaryContents;
  };
}

#endif
