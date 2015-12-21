/*
 See ASTEntry.h
*/

#include "ASTEntry.h"
#include "Utils.h"

#include "BinaryAddressMap.h"
#include "Renaming.h"

#include "Json.h"

#include "clang/AST/AST.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/SaveAndRestore.h"

#include <sstream>

namespace clang_mutate
{

picojson::value types_to_json(const std::set<size_t> & types)
{
    std::vector<picojson::value> ans;
    for (std::set<size_t>::const_iterator it = types.begin();
         it != types.end();
         ++it)
    {
        std::ostringstream ss;
        ss << std::hex << *it;
        ans.push_back(picojson::value(ss.str()));
    }
    return picojson::value(ans);
}

picojson::value renames_to_json(const Renames & renames, RenameKind k)
{
  std::vector<picojson::value> ans;
  for (Renames::const_iterator it = renames.begin();
       it != renames.end();
       ++it)
  {
      if (it->kind == k) {
          std::vector<picojson::value> item;
          item.push_back(picojson::value(it->name));
          item.push_back(picojson::value(static_cast<int64_t>(it->index)));
          ans.push_back(picojson::value(item));
      }
  }
  return picojson::value(ans);
}

picojson::value macros_to_json(const Macros & macros)
{
    std::vector<picojson::value> ans;
    for (Macros::const_iterator it = macros.begin(); it != macros.end(); ++it)
    {
        std::vector<picojson::value> item;
        item.push_back(picojson::value(it->name()));
        item.push_back(picojson::value(it->body()));
        ans.push_back(picojson::value(item));
    }
    return picojson::value(ans);
}

picojson::value stmt_list_to_json(const std::vector<unsigned int> & stmts)
{
    std::vector<picojson::value> ans;
    for (std::vector<unsigned int>::const_iterator it = stmts.begin();
         it != stmts.end();
         ++it)
    {
        ans.push_back(picojson::value(static_cast<int64_t>(*it)));
    }
    return picojson::value(ans);
}

  ASTEntry* ASTEntryFactory::make(
      clang::Stmt *s,
      clang::Stmt *p,
      const std::map<clang::Stmt*, unsigned int> &spine,
      clang::Rewriter& rewrite,
      BinaryAddressMap &binaryAddressMap,
      const Renames & renames,
      const Macros & macros,
      const std::set<size_t> & types )
  {
    clang::SourceManager &sm = rewrite.getSourceMgr();
    clang::PresumedLoc beginLoc = sm.getPresumedLoc(s->getSourceRange().getBegin());
    clang::PresumedLoc endLoc = sm.getPresumedLoc(s->getSourceRange().getEnd());

    std::string srcFileName = 
        realpath( 
            sm.getFileEntryForID( sm.getMainFileID() )->getName(),
            NULL);
    unsigned int beginSrcLine = beginLoc.getLine();
    unsigned int endSrcLine = endLoc.getLine();

    if ( binaryAddressMap.canGetBeginAddressForLine( srcFileName, beginSrcLine) &&
         binaryAddressMap.canGetEndAddressForLine( srcFileName, endSrcLine ) )
    {
      return new ASTBinaryEntry( s,
                                 p,
                                 spine,
                                 rewrite,
                                 binaryAddressMap,
                                 renames,
                                 macros,
                                 types);
    }
    else
    {
      return new ASTNonBinaryEntry( s,
                                    p,
                                    spine,
                                    rewrite,
                                    renames,
                                    macros,
                                    types);
    }
  }

  ASTNonBinaryEntry::ASTNonBinaryEntry() :
    m_counter(0),
    m_parentCounter(0),
    m_astClass(""),
    m_srcFileName(""),
    m_beginSrcLine(0),
    m_beginSrcCol(0),
    m_endSrcLine(0),
    m_endSrcCol(0),
    m_srcText(""),
    m_guardStmt(false),
    m_fullStmt(false),
    m_renames(),
    m_macros(),
    m_types()
  {
  }

  ASTNonBinaryEntry::ASTNonBinaryEntry( 
    const unsigned int counter,
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
    const std::set<size_t> & types) :

    m_counter(counter),
    m_parentCounter(parentCounter),
    m_astClass(astClass),
    m_srcFileName(srcFileName),
    m_beginSrcLine(beginSrcLine),
    m_beginSrcCol(beginSrcCol),
    m_endSrcLine(endSrcLine),
    m_endSrcCol(endSrcCol),
    m_srcText(srcText),
    m_guardStmt(guardStmt),
    m_fullStmt(fullStmt),
    m_renames(renames),
    m_macros(macros),
    m_types(types)
  {
  }

  ASTNonBinaryEntry::ASTNonBinaryEntry( 
    clang::Stmt * s,
    clang::Stmt * p,
    const std::map<clang::Stmt*, unsigned int> & spine,
    clang::Rewriter& rewrite,
    const Renames & renames,
    const Macros & macros,
    const std::set<size_t> & types )
  {
    clang::SourceManager &sm = rewrite.getSourceMgr();
    clang::SourceRange r = s->getSourceRange();
    clang::PresumedLoc beginLoc = sm.getPresumedLoc(r.getBegin());
    clang::PresumedLoc endLoc = sm.getPresumedLoc(r.getEnd());

    m_counter = spine.find(s)->second;
    m_parentCounter = ( p == NULL || spine.find(p) == spine.end() ) ?
                      0 : spine.find(p)->second;
    m_astClass = s->getStmtClassName();
    m_srcFileName = 
        realpath( 
            sm.getFileEntryForID( sm.getMainFileID() )->getName(),
            NULL);
    m_beginSrcLine = beginLoc.getLine();
    m_beginSrcCol = beginLoc.getColumn();
    m_endSrcLine = endLoc.getLine();
    m_endSrcCol = endLoc.getColumn();
    m_guardStmt = Utils::IsGuardStmt(s, p);
    m_fullStmt = p == NULL
        || p->getStmtClassName() == std::string("CompoundStmt");
    m_renames = renames;
    m_macros = macros;
    m_types = types;
        
    RenameFreeVar renamer(s, rewrite, renames);
    m_srcText = renamer.getRewrittenString();
  }

  ASTNonBinaryEntry::~ASTNonBinaryEntry() {}

  ASTNonBinaryEntry::ASTEntry* ASTNonBinaryEntry::clone() const 
  {
    return new ASTNonBinaryEntry( m_counter,
                                  m_parentCounter,
                                  m_astClass,
                                  m_srcFileName,
                                  m_beginSrcLine,
                                  m_beginSrcCol,
                                  m_endSrcLine,
                                  m_endSrcCol,
                                  m_srcText,
                                  m_guardStmt,
                                  m_fullStmt,
                                  m_renames,
                                  m_macros,
                                  m_types);
  }

  unsigned int ASTNonBinaryEntry::getCounter() const
  {
    return m_counter;
  }

  unsigned int ASTNonBinaryEntry::getParentCounter() const
  {
    return m_parentCounter;
  }

  std::string ASTNonBinaryEntry::getASTClass() const 
  {
    return m_astClass;
  }

  std::string ASTNonBinaryEntry::getSrcFileName() const
  {
    return m_srcFileName;
  }

  unsigned int ASTNonBinaryEntry::getBeginSrcLine() const
  {
    return m_beginSrcLine;
  }

  unsigned int ASTNonBinaryEntry::getBeginSrcCol() const
  {
    return m_beginSrcCol;
  }

  unsigned int ASTNonBinaryEntry::getEndSrcLine() const
  {
    return m_endSrcLine;
  }

  unsigned int ASTNonBinaryEntry::getEndSrcCol() const
  {
    return m_endSrcCol;
  }

  std::string ASTNonBinaryEntry::getSrcText() const
  {
    return m_srcText;
  }

  bool ASTNonBinaryEntry::getIsGuardStmt() const
  {
    return m_guardStmt;
  }

  bool ASTNonBinaryEntry::getIsFullStmt() const
  {
    return m_fullStmt;
  }

  Renames ASTNonBinaryEntry::getRenames() const
  {
      return m_renames;
  }

  Macros ASTNonBinaryEntry::getMacros() const
  {
      return m_macros;
  }

  std::set<size_t> ASTNonBinaryEntry::getTypes() const
  {
      return m_types;
  }

  std::string ASTNonBinaryEntry::toString() const
  {
    char msg[256];

    sprintf(msg, "%8d %6d:%-3d %6d:%-3d %s", 
                 m_counter,
                 m_beginSrcLine,
                 m_beginSrcCol,
                 m_endSrcLine,
                 m_endSrcCol,
                 m_astClass.c_str());

    return std::string(msg);
  }

  picojson::value ASTNonBinaryEntry::toJSON() const
  {
    picojson::object jsonObj;
    
    jsonObj["counter"] = picojson::value(static_cast<int64_t>(m_counter));
    jsonObj["parent_counter"] = picojson::value(static_cast<int64_t>(m_parentCounter));
    jsonObj["ast_class"] = picojson::value(m_astClass);
    jsonObj["src_file_name"] = picojson::value(m_srcFileName);
    jsonObj["begin_src_line"] = picojson::value(static_cast<int64_t>(m_beginSrcLine));
    jsonObj["begin_src_col"] = picojson::value(static_cast<int64_t>(m_beginSrcCol));
    jsonObj["end_src_line"] = picojson::value(static_cast<int64_t>(m_endSrcLine));
    jsonObj["end_src_col"] = picojson::value(static_cast<int64_t>(m_endSrcCol));
    jsonObj["src_text"] = picojson::value(escape_for_json(m_srcText));
    jsonObj["guard_stmt"] = picojson::value(m_guardStmt);
    jsonObj["full_stmt"] = picojson::value(m_fullStmt);
    jsonObj["unbound_vals"] = renames_to_json(m_renames, VariableRename);
    jsonObj["unbound_funs"] = renames_to_json(m_renames, FunctionRename);
    jsonObj["macros"] = macros_to_json(m_macros);
    jsonObj["types"] = types_to_json(m_types);
    
    if (m_astClass == "CompoundStmt") {
        jsonObj["stmt_list"] = stmt_list_to_json(m_stmt_list);
    }

    return picojson::value(jsonObj);
  }

  ASTBinaryEntry::ASTBinaryEntry() :
    ASTNonBinaryEntry(),
    m_binaryFilePath(""),
    m_beginAddress(0),
    m_endAddress(0)
  {
  }

  ASTBinaryEntry::ASTBinaryEntry( 
    const unsigned int counter, 
    const unsigned int parent_counter,
    const std::string &astClass,
    const std::string &srcFileName,
    const unsigned int beginSrcLine,
    const unsigned int beginSrcCol,
    const unsigned int endSrcLine,
    const unsigned int endSrcCol,
    const std::string &srcText,
    bool guardStmt,
    bool fullStmt,
    const Renames & renames,
    const Macros & macros,
    const std::set<size_t> & types,
    const std::string &binaryFileName,
    const unsigned long beginAddress,
    const unsigned long endAddress,
    const std::string &binaryContents ) :

    ASTNonBinaryEntry( counter, 
                       parent_counter,
                       astClass, 
                       srcFileName, 
                       beginSrcLine, 
                       beginSrcCol, 
                       endSrcLine, 
                       endSrcCol, 
                       srcText,
                       guardStmt,
                       fullStmt,
                       renames,
                       macros,
                       types),
    m_binaryFilePath(binaryFileName),
    m_beginAddress(beginAddress),
    m_endAddress(endAddress),
    m_binaryContents(binaryContents)
  {
  }

  ASTBinaryEntry::ASTBinaryEntry( 
    clang::Stmt * s,
    clang::Stmt * p,
    const std::map<clang::Stmt*, unsigned int> & spine,
    clang::Rewriter& rewrite,
    BinaryAddressMap& binaryAddressMap,
    const Renames & renames,
    const Macros & macros,
    const std::set<size_t> & types) :

    ASTNonBinaryEntry( s,
                       p,
                       spine,
                       rewrite,
                       renames,
                       macros,
                       types)
  {
    m_binaryFilePath = binaryAddressMap.getBinaryPath();
    m_beginAddress = 
        binaryAddressMap.getBeginAddressForLine(
            getSrcFileName(), 
            getBeginSrcLine());
    m_endAddress = 
        binaryAddressMap.getEndAddressForLine(
            getSrcFileName(), 
            getEndSrcLine());
    m_binaryContents = 
        binaryAddressMap.getBinaryContentsAsStr(
            m_beginAddress, 
            m_endAddress);
  }

  ASTBinaryEntry::~ASTBinaryEntry() {}

  ASTBinaryEntry::ASTEntry* ASTBinaryEntry::clone() const 
  {
    return new ASTBinaryEntry( getCounter(),
                               getParentCounter(),
                               getASTClass(),
                               getSrcFileName(),
                               getBeginSrcLine(),
                               getBeginSrcCol(),
                               getEndSrcLine(),
                               getEndSrcCol(),
                               getSrcText(),
                               getIsGuardStmt(),
                               getIsFullStmt(),
                               getRenames(),
                               getMacros(),
                               getTypes(),
                               m_binaryFilePath,
                               m_beginAddress,
                               m_endAddress,
                               m_binaryContents );
  }

  std::string ASTBinaryEntry::getBinaryFilePath() const
  {
    return m_binaryFilePath;
  }

  unsigned long ASTBinaryEntry::getBeginAddress() const
  {
    return m_beginAddress;
  }

  unsigned long ASTBinaryEntry::getEndAddress() const
  {
    return m_endAddress;
  }

  std::string ASTBinaryEntry::getBinaryContents() const
  {
    return m_binaryContents;
  }

  std::string ASTBinaryEntry::toString() const
  {
    char msg[256];

    sprintf(msg, "%8d %6d:%-3d %6d:%-3d %-25s %#016lx %#016lx", 
                 getCounter(),
                 getBeginSrcLine(),
                 getBeginSrcCol(),
                 getEndSrcLine(),
                 getEndSrcCol(),
                 getASTClass().c_str(),
                 m_beginAddress,
                 m_endAddress);

    return std::string(msg);
  }
  
  picojson::value ASTBinaryEntry::toJSON() const
  {
    picojson::object jsonObj = ASTNonBinaryEntry::toJSON().get<picojson::object>();
    
    jsonObj["binary_file_path"] = picojson::value(m_binaryFilePath);
    jsonObj["begin_addr"] = picojson::value(static_cast<int64_t>(m_beginAddress));
    jsonObj["end_addr"] = picojson::value(static_cast<int64_t>(m_endAddress));
    jsonObj["binary_contents"] = picojson::value(m_binaryContents);

    return picojson::value(jsonObj);
  }

}
