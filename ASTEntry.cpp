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

#include <algorithm>
#include <string>

namespace clang_mutate
{

ASTEntryField ASTEntryField::NULL_FIELD       = ASTEntryField("");
ASTEntryField ASTEntryField::COUNTER          = ASTEntryField("counter");
ASTEntryField ASTEntryField::PARENT_COUNTER   = ASTEntryField("parent_counter");
ASTEntryField ASTEntryField::AST_CLASS        = ASTEntryField("ast_class");
ASTEntryField ASTEntryField::SRC_FILE_NAME    = ASTEntryField("src_file_name");
ASTEntryField ASTEntryField::BEGIN_SRC_LINE   = ASTEntryField("begin_src_line");
ASTEntryField ASTEntryField::BEGIN_SRC_COL    = ASTEntryField("begin_src_col");
ASTEntryField ASTEntryField::END_SRC_LINE     = ASTEntryField("end_src_line");
ASTEntryField ASTEntryField::END_SRC_COL      = ASTEntryField("end_src_col");
ASTEntryField ASTEntryField::SRC_TEXT         = ASTEntryField("src_text");
ASTEntryField ASTEntryField::GUARD_STMT       = ASTEntryField("guard_stmt");
ASTEntryField ASTEntryField::FULL_STMT        = ASTEntryField("full_stmt");
ASTEntryField ASTEntryField::UNBOUND_VALS     = ASTEntryField("unbound_vals");
ASTEntryField ASTEntryField::UNBOUND_FUNS     = ASTEntryField("unbound_funs");
ASTEntryField ASTEntryField::MACROS           = ASTEntryField("macros");
ASTEntryField ASTEntryField::TYPES            = ASTEntryField("types");
ASTEntryField ASTEntryField::STMT_LIST        = ASTEntryField("stmt_list");
ASTEntryField ASTEntryField::OPCODE           = ASTEntryField("opcode");
ASTEntryField ASTEntryField::SCOPES           = ASTEntryField("scopes");
ASTEntryField ASTEntryField::BINARY_FILE_PATH = ASTEntryField("binary_file_path");
ASTEntryField ASTEntryField::BEGIN_ADDR       = ASTEntryField("begin_addr");
ASTEntryField ASTEntryField::END_ADDR         = ASTEntryField("end_addr");
ASTEntryField ASTEntryField::BINARY_CONTENTS  = ASTEntryField("binary_contents");

ASTEntryField::ASTEntryField(const std::string &jsonName) :
    m_jsonName(jsonName)
{}

std::string ASTEntryField::getJSONName() const {
  return m_jsonName;
}

bool ASTEntryField::operator==(const ASTEntryField &other) const {
  return getJSONName() == other.getJSONName();
}

bool ASTEntryField::operator<(const ASTEntryField &other) const {
  return getJSONName() < other.getJSONName();
}

ASTEntryField ASTEntryField::fromJSONName(const std::string &jsonName) {
  std::string lowerJSONName;

  lowerJSONName.resize(jsonName.size());
  std::transform(jsonName.begin(),
                 jsonName.end(),
                 lowerJSONName.begin(),
                 ::tolower);

  if (lowerJSONName == COUNTER.getJSONName())
    return COUNTER;
  else if (lowerJSONName == PARENT_COUNTER.getJSONName())
    return PARENT_COUNTER;
  else if (lowerJSONName == AST_CLASS.getJSONName())
    return AST_CLASS;
  else if (lowerJSONName == SRC_FILE_NAME.getJSONName())
    return SRC_FILE_NAME;
  else if (lowerJSONName == BEGIN_SRC_LINE.getJSONName())
    return BEGIN_SRC_LINE;
  else if (lowerJSONName == BEGIN_SRC_COL.getJSONName())
    return BEGIN_SRC_COL;
  else if (lowerJSONName == END_SRC_LINE.getJSONName())
    return END_SRC_LINE;
  else if (lowerJSONName == END_SRC_COL.getJSONName())
    return END_SRC_COL;
  else if (lowerJSONName == SRC_TEXT.getJSONName())
    return SRC_TEXT;
  else if (lowerJSONName == GUARD_STMT.getJSONName())
    return GUARD_STMT;
  else if (lowerJSONName == FULL_STMT.getJSONName())
    return FULL_STMT;
  else if (lowerJSONName == UNBOUND_VALS.getJSONName())
    return UNBOUND_VALS;
  else if (lowerJSONName == UNBOUND_FUNS.getJSONName())
    return UNBOUND_FUNS;
  else if (lowerJSONName == MACROS.getJSONName())
    return MACROS;
  else if (lowerJSONName == TYPES.getJSONName())
    return TYPES;
  else if (lowerJSONName == STMT_LIST.getJSONName())
    return STMT_LIST;
  else if (lowerJSONName == SCOPES.getJSONName())
    return SCOPES;
  else if (lowerJSONName == BINARY_FILE_PATH.getJSONName())
    return BINARY_FILE_PATH;
  else if (lowerJSONName == BEGIN_ADDR.getJSONName())
    return BEGIN_ADDR;
  else if (lowerJSONName == END_ADDR.getJSONName())
    return END_ADDR;
  else if (lowerJSONName == BINARY_CONTENTS.getJSONName())
    return BINARY_CONTENTS;
  else
    return NULL_FIELD;
}

std::set<ASTEntryField>
ASTEntryField::fromJSONNames(const std::vector<std::string> &jsonNames) {
  std::set<ASTEntryField> fields;

  for (std::vector<std::string>::const_iterator iter = jsonNames.begin();
       iter != jsonNames.end();
       iter++) {
    fields.insert(ASTEntryField::fromJSONName(*iter));
  }

  return fields;
}

picojson::value renames_to_json(const Renames & renames, RenameKind k)
{
  std::vector<std::pair<std::string, unsigned int> > ans;
  for (Renames::const_iterator it = renames.begin();
       it != renames.end();
       ++it)
  {
      if (it->kind == k) {
          ans.push_back(std::make_pair(it->name,
                                       it->index));
      }
  }
  return to_json(ans);
}

  std::set<ASTEntryField> ASTEntryField::getDefaultFields()
  {
    std::set<ASTEntryField> defaultFields;

    defaultFields.insert(ASTEntryField::COUNTER);
    defaultFields.insert(ASTEntryField::PARENT_COUNTER);
    defaultFields.insert(ASTEntryField::AST_CLASS);
    defaultFields.insert(ASTEntryField::SRC_FILE_NAME);
    defaultFields.insert(ASTEntryField::BEGIN_SRC_LINE);
    defaultFields.insert(ASTEntryField::BEGIN_SRC_COL);
    defaultFields.insert(ASTEntryField::END_SRC_LINE);
    defaultFields.insert(ASTEntryField::END_SRC_COL);
    defaultFields.insert(ASTEntryField::SRC_TEXT);
    defaultFields.insert(ASTEntryField::GUARD_STMT);
    defaultFields.insert(ASTEntryField::FULL_STMT);
    defaultFields.insert(ASTEntryField::COUNTER);
    defaultFields.insert(ASTEntryField::UNBOUND_VALS);
    defaultFields.insert(ASTEntryField::UNBOUND_FUNS);
    defaultFields.insert(ASTEntryField::MACROS);
    defaultFields.insert(ASTEntryField::TYPES);
    defaultFields.insert(ASTEntryField::STMT_LIST);
    defaultFields.insert(ASTEntryField::OPCODE);
    defaultFields.insert(ASTEntryField::BINARY_FILE_PATH);
    defaultFields.insert(ASTEntryField::BEGIN_ADDR);
    defaultFields.insert(ASTEntryField::END_ADDR);
    defaultFields.insert(ASTEntryField::BINARY_CONTENTS);

    return defaultFields;
  }

  ASTEntry* ASTEntryFactory::make(
      clang::Stmt *s,
      clang::Stmt *p,
      const std::map<clang::Stmt*, unsigned int> &spine,
      clang::Rewriter& rewrite,
      BinaryAddressMap &binaryAddressMap,
      const Renames & renames,
      const Macros & macros,
      const std::set<Hash> & types,
      const ScopedNames & scoped_names )
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
                                 types,
                                 scoped_names);
    }
    else
    {
      return new ASTNonBinaryEntry( s,
                                    p,
                                    spine,
                                    rewrite,
                                    renames,
                                    macros,
                                    types,
                                    scoped_names);
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
    m_types(),
    m_stmt_list(),
    m_opcode(""),
    m_scoped_names()
  {}

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
    const std::set<Hash> & types,
    const std::vector<unsigned int> & stmt_list,
    const std::string & opcode,
    const ScopedNames & scoped_names) :
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
    m_types(types),
    m_stmt_list(stmt_list),
    m_opcode(opcode),
    m_scoped_names(scoped_names)
    {}

  ASTNonBinaryEntry::ASTNonBinaryEntry(
    clang::Stmt * s,
    clang::Stmt * p,
    const std::map<clang::Stmt*, unsigned int> & spine,
    clang::Rewriter& rewrite,
    const Renames & renames,
    const Macros & macros,
    const std::set<Hash> & types,
    const ScopedNames & scoped_names )
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
    m_scoped_names = scoped_names;
    m_opcode = clang::isa<clang::BinaryOperator>(s) ?
               static_cast<clang::BinaryOperator*>(s)->getOpcodeStr() :
               "";

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
                                  m_types,
                                  m_stmt_list,
                                  m_opcode,
                                  m_scoped_names);
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

  std::set<Hash> ASTNonBinaryEntry::getTypes() const
  {
      return m_types;
  }

  std::vector<unsigned int> ASTNonBinaryEntry::getStmtList() const
  {
    return m_stmt_list;
  }

  std::string ASTNonBinaryEntry::getOpcode() const
  {
    return m_opcode;
  }

  ScopedNames ASTNonBinaryEntry::getScopedNames() const
  {
      return m_scoped_names;
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

  picojson::value
  ASTNonBinaryEntry::toJSON(const std::set<ASTEntryField> &fields) const
  {
    picojson::object jsonObj;

    if (fields.find(ASTEntryField::COUNTER) != fields.end())
        jsonObj[ASTEntryField::COUNTER.getJSONName()] =
            to_json(m_counter);

    if (fields.find(ASTEntryField::PARENT_COUNTER) != fields.end())
        jsonObj[ASTEntryField::PARENT_COUNTER.getJSONName()] =
            to_json(m_parentCounter);

    if (fields.find(ASTEntryField::AST_CLASS) != fields.end())
        jsonObj[ASTEntryField::AST_CLASS.getJSONName()] =
          to_json(m_astClass);

    if (fields.find(ASTEntryField::SRC_FILE_NAME) != fields.end())
        jsonObj[ASTEntryField::SRC_FILE_NAME.getJSONName()] =
            to_json(m_srcFileName);

    if (fields.find(ASTEntryField::BEGIN_SRC_LINE) != fields.end())
        jsonObj[ASTEntryField::BEGIN_SRC_LINE.getJSONName()] =
            to_json(m_beginSrcLine);

    if (fields.find(ASTEntryField::BEGIN_SRC_COL) != fields.end())
        jsonObj[ASTEntryField::BEGIN_SRC_COL.getJSONName()] =
            to_json(m_beginSrcCol);

    if (fields.find(ASTEntryField::END_SRC_LINE) != fields.end())
        jsonObj[ASTEntryField::END_SRC_LINE.getJSONName()] =
            to_json(m_endSrcLine);

    if (fields.find(ASTEntryField::END_SRC_COL) != fields.end())
        jsonObj[ASTEntryField::END_SRC_COL.getJSONName()] =
            to_json(m_endSrcCol);

    if (fields.find(ASTEntryField::SRC_TEXT) != fields.end())
        jsonObj[ASTEntryField::SRC_TEXT.getJSONName()] =
            to_json(m_srcText);

    if (fields.find(ASTEntryField::GUARD_STMT) != fields.end())
        jsonObj[ASTEntryField::GUARD_STMT.getJSONName()] =
            to_json(m_guardStmt);

    if (fields.find(ASTEntryField::FULL_STMT) != fields.end())
        jsonObj[ASTEntryField::ASTEntryField::FULL_STMT.getJSONName()] =
            to_json(m_fullStmt);

    if (fields.find(ASTEntryField::UNBOUND_VALS) != fields.end())
        jsonObj[ASTEntryField::UNBOUND_VALS.getJSONName()] =
            renames_to_json(m_renames, VariableRename);

    if (fields.find(ASTEntryField::UNBOUND_FUNS) != fields.end())
        jsonObj[ASTEntryField::UNBOUND_FUNS.getJSONName()] =
            renames_to_json(m_renames, FunctionRename);

    if (fields.find(ASTEntryField::MACROS) != fields.end())
        jsonObj[ASTEntryField::MACROS.getJSONName()] =
            to_json(m_macros);

    if (fields.find(ASTEntryField::TYPES) != fields.end())
        jsonObj[ASTEntryField::TYPES.getJSONName()] =
            to_json(m_types);

    if (fields.find(ASTEntryField::OPCODE) != fields.end() &&
        !m_opcode.empty())
    {
        jsonObj[ASTEntryField::OPCODE.getJSONName()] =
            to_json(m_opcode);
    }

    if (fields.find(ASTEntryField::STMT_LIST) != fields.end() &&
        m_astClass == "CompoundStmt")
    {
        jsonObj[ASTEntryField::STMT_LIST.getJSONName()] =
            to_json(m_stmt_list);
    }

    if (fields.find(ASTEntryField::SCOPES) != fields.end()) {
        jsonObj[ASTEntryField::SCOPES.getJSONName()] =
            to_json(m_scoped_names);
    }

    return to_json(jsonObj);
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
    const std::set<Hash> & types,
    const std::vector<unsigned int> & stmt_list,
    const std::string & opcode,
    const ScopedNames & scoped_names,
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
                       types,
                       stmt_list,
                       opcode,
                       scoped_names),
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
    const std::set<Hash> & types,
    const ScopedNames & scoped_names) :

    ASTNonBinaryEntry( s,
                       p,
                       spine,
                       rewrite,
                       renames,
                       macros,
                       types,
                       scoped_names)
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
                               getStmtList(),
                               getOpcode(),
                               getScopedNames(),
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

  picojson::value
  ASTBinaryEntry::toJSON(const std::set<ASTEntryField> &fields) const
  {
    picojson::object jsonObj = ASTNonBinaryEntry::toJSON().get<picojson::object>();

    if (fields.find(ASTEntryField::BINARY_FILE_PATH) != fields.end())
      jsonObj[ASTEntryField::BINARY_FILE_PATH.getJSONName()] =
        to_json(m_binaryFilePath);

    if (fields.find(ASTEntryField::BEGIN_ADDR) != fields.end())
      jsonObj[ASTEntryField::BEGIN_ADDR.getJSONName()] =
        to_json(m_beginAddress);

    if (fields.find(ASTEntryField::END_ADDR) != fields.end())
      jsonObj[ASTEntryField::END_ADDR.getJSONName()] =
        to_json(m_endAddress);

    if (fields.find(ASTEntryField::BINARY_CONTENTS) != fields.end())
      jsonObj[ASTEntryField::BINARY_CONTENTS.getJSONName()] =
        to_json(m_binaryContents);

    return to_json(jsonObj);
  }

}
