/*
 The classes within this file are utilized for storing information about a single entry in 
 an AST.  They allow for loading from/storing to a JSON representation.
*/

#ifndef AST_ENTRY_HPP
#define AST_ENTRY_HPP

#include "BinaryAddressMap.hpp"

#include "clang/AST/AST.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "third-party/picojson-1.3.0/picojson.h"

#include <string>

namespace clang_mutate
{
  // Abstract base class for all AST entries.
  // Includes methods to convert to a string or JSON representation.
  class ASTEntry
  {
  public:
    virtual ~ASTEntry() {}
    virtual ASTEntry* clone() const = 0;
    virtual unsigned int getCounter() const = 0;
    virtual std::string toString() const = 0;
    virtual picojson::value toJSON() const = 0;
  };

  // AST entry without binary information
  class ASTNonBinaryEntry : public ASTEntry
  {
  protected:
    unsigned int m_counter;
    std::string  m_astClass;
    std::string  m_srcFileName;
    unsigned int m_beginSrcLine;
    unsigned int m_beginSrcCol;
    unsigned int m_endSrcLine;
    unsigned int m_endSrcCol;
    std::string  m_srcText;
  public:
    ASTNonBinaryEntry() :
      m_counter(0),
      m_astClass(""),
      m_srcFileName(""),
      m_beginSrcLine(0),
      m_beginSrcCol(0),
      m_endSrcLine(0),
      m_endSrcCol(0),
      m_srcText("")
    {
    }

    ASTNonBinaryEntry( const unsigned int counter,
                       const std::string &astClass,
                       const std::string &srcFileName,
                       const unsigned int beginSrcLine,
                       const unsigned int beginSrcCol,
                       const unsigned int endSrcLine,
                       const unsigned int endSrcCol,
                       const std::string &srcText ) :
      m_counter(counter),
      m_astClass(astClass),
      m_srcFileName(srcFileName),
      m_beginSrcLine(beginSrcLine),
      m_beginSrcCol(beginSrcCol),
      m_endSrcLine(endSrcLine),
      m_endSrcCol(endSrcCol),
      m_srcText(srcText)
    {
    }

    ASTNonBinaryEntry( const int counter,
                       clang::Stmt * s,
                       clang::Rewriter& rewrite )
    {
      clang::SourceManager &sm = rewrite.getSourceMgr();
      clang::PresumedLoc beginLoc = sm.getPresumedLoc(s->getSourceRange().getBegin());
      clang::PresumedLoc endLoc = sm.getPresumedLoc(s->getSourceRange().getEnd());

      m_counter = counter;
      m_astClass = s->getStmtClassName();
      m_srcFileName = sm.getFileEntryForID( sm.getMainFileID() )->getName();
      m_beginSrcLine = beginLoc.getLine();
      m_beginSrcCol = beginLoc.getColumn();
      m_endSrcLine = endLoc.getLine();
      m_endSrcCol = endLoc.getColumn();
      m_srcText = rewrite.ConvertToString(s);
    }

    ASTNonBinaryEntry( const picojson::value &jsonValue )
    {
      if ( ASTNonBinaryEntry::jsonObjHasRequiredFields(jsonValue) )
      {
        m_counter      = jsonValue.get("counter").get<int64_t>();
        m_astClass     = jsonValue.get("ast_class").get<std::string>();
        m_srcFileName  = jsonValue.get("src_file_name").get<std::string>();
        m_beginSrcLine = jsonValue.get("begin_src_line").get<int64_t>();
        m_beginSrcCol  = jsonValue.get("begin_src_col").get<int64_t>();
        m_endSrcLine   = jsonValue.get("end_src_line").get<int64_t>();
        m_endSrcCol    = jsonValue.get("end_src_col").get<int64_t>();
        m_srcText      = jsonValue.get("src_text").get<std::string>();
      }
    }

    virtual ~ASTNonBinaryEntry() {}

    virtual ASTEntry* clone() const 
    {
      return new ASTNonBinaryEntry( m_counter,
                                    m_astClass,
                                    m_srcFileName,
                                    m_beginSrcLine,
                                    m_beginSrcCol,
                                    m_endSrcLine,
                                    m_endSrcCol,
                                    m_srcText );
    }

    virtual unsigned int getCounter() const
    {
      return m_counter;
    }

    std::string getASTClass() const 
    {
      return m_astClass;
    }

    std::string getSrcFileName() const
    {
      return m_srcFileName;
    }

    unsigned int getBeginSrcLine() const
    {
      return m_beginSrcLine;
    }

    unsigned int getBeginSrcCol() const
    {
      return m_beginSrcCol;
    }

    unsigned int getEndSrcLine() const
    {
      return m_endSrcLine;
    }

    unsigned int getEndSrcCol() const
    {
      return m_endSrcCol;
    }

    std::string getSrcText() const
    {
      return m_srcText;
    }

    virtual std::string toString() const
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

    virtual picojson::value toJSON() const
    {
      picojson::object jsonObj;
      
      jsonObj["counter"] = picojson::value(static_cast<int64_t>(m_counter));
      jsonObj["ast_class"] = picojson::value(m_astClass);
      jsonObj["src_file_name"] = picojson::value(m_srcFileName);
      jsonObj["begin_src_line"] = picojson::value(static_cast<int64_t>(m_beginSrcLine));
      jsonObj["begin_src_col"] = picojson::value(static_cast<int64_t>(m_beginSrcCol));
      jsonObj["end_src_line"] = picojson::value(static_cast<int64_t>(m_endSrcLine));
      jsonObj["end_src_col"] = picojson::value(static_cast<int64_t>(m_endSrcCol));
      jsonObj["src_text"] = picojson::value(m_srcText);
 
      return picojson::value(jsonObj);
    }

    static bool jsonObjHasRequiredFields( const picojson::value& jsonValue )
    {
      return jsonValue.is<picojson::object>() && 
             jsonValue.contains("counter") &&
             jsonValue.contains("ast_class") &&
             jsonValue.contains("src_file_name") &&
             jsonValue.contains("begin_src_line") &&
             jsonValue.contains("begin_src_col") &&
             jsonValue.contains("end_src_line") &&
             jsonValue.contains("end_src_col") &&
             jsonValue.contains("src_text");
    }
  };

  // AST entry with binary information
  class ASTBinaryEntry : public ASTEntry
  {
  protected:
    ASTNonBinaryEntry m_astNonBinaryEntry;
    std::string       m_binaryFilePath;
    unsigned long     m_beginAddress;
    unsigned long     m_endAddress;
  public:
    ASTBinaryEntry() :
      m_astNonBinaryEntry(),
      m_binaryFilePath(""),
      m_beginAddress(0),
      m_endAddress(0)
    {
    }

    ASTBinaryEntry( const unsigned int counter, 
                    const std::string &astClass,
                    const std::string &srcFileName,
                    const unsigned int beginSrcLine,
                    const unsigned int beginSrcCol,
                    const unsigned int endSrcLine,
                    const unsigned int endSrcCol,
                    const std::string &srcText,
                    const std::string &binaryFileName,
                    const unsigned long beginAddress,
                    const unsigned long endAddress ) :
      m_astNonBinaryEntry( counter, 
                           astClass, 
                           srcFileName, 
                           beginSrcLine, 
                           beginSrcCol, 
                           endSrcLine, 
                           endSrcCol, 
                           srcText ),
      m_binaryFilePath(binaryFileName),
      m_beginAddress(beginAddress),
      m_endAddress(endAddress)
    {
    }

    ASTBinaryEntry( const unsigned int counter,
                    clang::Stmt * s,
                    clang::Rewriter& rewrite,
                    const BinaryAddressMap& binaryAddressMap ) :
      m_astNonBinaryEntry( counter, s, rewrite )
    {
      m_binaryFilePath = binaryAddressMap.getBinaryPath();
      m_beginAddress = binaryAddressMap.getBeginAddressForLine(
                           m_astNonBinaryEntry.getSrcFileName(), 
                           m_astNonBinaryEntry.getBeginSrcLine());
      m_endAddress = binaryAddressMap.getEndAddressForLine(
                           m_astNonBinaryEntry.getSrcFileName(), 
                           m_astNonBinaryEntry.getEndSrcLine());
    }

    ASTBinaryEntry( const picojson::value& jsonValue ) :
      m_astNonBinaryEntry( jsonValue )
    {
      if ( ASTBinaryEntry::jsonObjHasRequiredFields(jsonValue) )
      {
        m_binaryFilePath = jsonValue.get("binary_file_path").get<std::string>();
        m_beginAddress   = jsonValue.get("begin_addr").get<int64_t>();
        m_endAddress     = jsonValue.get("end_addr").get<int64_t>();
      }
    }
    
    virtual ~ASTBinaryEntry() {}

    virtual ASTEntry* clone() const 
    {
      return new ASTBinaryEntry( m_astNonBinaryEntry.getCounter(),
                                 m_astNonBinaryEntry.getASTClass(),
                                 m_astNonBinaryEntry.getSrcFileName(),
                                 m_astNonBinaryEntry.getBeginSrcLine(),
                                 m_astNonBinaryEntry.getBeginSrcCol(),
                                 m_astNonBinaryEntry.getEndSrcLine(),
                                 m_astNonBinaryEntry.getEndSrcCol(),
                                 m_astNonBinaryEntry.getSrcText(),
                                 m_binaryFilePath,
                                 m_beginAddress,
                                 m_endAddress );
    }

    virtual unsigned int getCounter() const
    {
      return m_astNonBinaryEntry.getCounter();
    }

    virtual std::string toString() const
    {
      char msg[256];

      sprintf(msg, "%8d %6d:%-3d %6d:%-3d %25s %#016x %#016x", 
                   m_astNonBinaryEntry.getCounter(),
                   m_astNonBinaryEntry.getBeginSrcLine(),
                   m_astNonBinaryEntry.getBeginSrcCol(),
                   m_astNonBinaryEntry.getEndSrcLine(),
                   m_astNonBinaryEntry.getEndSrcCol(),
                   m_astNonBinaryEntry.getASTClass().c_str(),
                   m_beginAddress,
                   m_endAddress);

      return std::string(msg);
    }

    virtual picojson::value toJSON() const
    {
      picojson::object jsonObj = m_astNonBinaryEntry.toJSON().get<picojson::object>();
      
      jsonObj["binary_file_path"] = picojson::value(m_binaryFilePath);
      jsonObj["begin_addr"] = picojson::value(static_cast<int64_t>(m_beginAddress));
      jsonObj["end_addr"] = picojson::value(static_cast<int64_t>(m_endAddress));
 
      return picojson::value(jsonObj);
    }

    static bool jsonObjHasRequiredFields( const picojson::value &jsonValue )
    {
      return ASTNonBinaryEntry::jsonObjHasRequiredFields( jsonValue ) &&
             jsonValue.contains("binary_file_path") &&
             jsonValue.contains("begin_addr") &&
             jsonValue.contains("end_addr");
    }
  };
}

#endif
