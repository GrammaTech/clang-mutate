#include "ASTBinaryAddressLister.h"

#include "clang/Basic/FileManager.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/AST/ParentMap.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Timer.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>

#define VISIT(func) \
  bool func { VisitRange(element->getSourceRange()); return true; }

using namespace clang;

namespace {
  // Class to hold the parsed results of llvm-dwarfdump -debug-dump=line 
  // This class will store all compilation units in a binary, the files
  // in those compilation units, and the mapping file line number -> address
  // in binary.
  // @FUTURE: Use libdrawf on the raw binary to get this data.
  class BinaryAddressMap {
  private:
    typedef std::pair<long, unsigned long long> LineNumAddressPair;
    typedef std::pair<std::string, LineNumAddressPair> FilenameLineNumAddressPair;
    typedef std::map<long, unsigned long long> LineNumsToAddressesMap;
    typedef std::map<std::string, LineNumsToAddressesMap> FilesMap;
    typedef std::map<unsigned, FilesMap> CompilationUnitMap;

    CompilationUnitMap compilationUnitMap;

    // Parse a single line in the form "%0x     %d     %d     %d  %d  %s"
    //  %0x#1: Address in binary
    //  %d#1: Line number in source
    //  %d#3: File index
    FilenameLineNumAddressPair parseAddressLine( const std::string &line, 
                                                 const std::vector<std::string> &files ) {
      // Regular expressions would be cool...
      std::stringstream lineEntriesStream( line );
      FilenameLineNumAddressPair filenameLineNumAddressPair;
      unsigned long long address;
      long lineNum;
      unsigned tmp, fileIndex;
    
      // Special: If end_sequence is present, this is the end address of the current
      // line number. We will just store it as the next line number.      
      bool endSequence = line.find("end_sequence") != std::string::npos;

      lineEntriesStream >> std::hex >> address >> std::dec >> lineNum >> tmp >> fileIndex;

      filenameLineNumAddressPair.first = files[fileIndex-1];
      filenameLineNumAddressPair.second.first = (endSequence) ? lineNum+1 : lineNum;
      filenameLineNumAddressPair.second.second = address;

      return filenameLineNumAddressPair;
    }

    // Parse a single line in the form "include_directories[ %d] = '%s'"
    //  %d#1: Directory index (1..n)
    //  %s#1: File name
    // This will return the directory name (%s) normalized by realpath.
    std::string parseDirectoryLine( const std::string &line ) {
      // Regular expressions would be cool...
      size_t startquote = line.find_first_of('\'') + 1;
      size_t endquote = line.find_last_of('\'') - 1;
      std::string directory = line.substr( startquote, endquote - startquote + 1);
      char buffer[1024];
        
      return realpath(directory.c_str(), buffer);
    }

    // Parse a single line in the form "file_names[ %d]   %d %0x %0x %s"
    //  %d#1: File name index (1..n)
    //  %d#2: Directory index (1..n)
    //  %s#1: File name
    // This will return the file name appended to the directory associated with this file
    std::string parseFileLine( const std::string &line, const std::vector<std::string> &directories ) {
      // Regular expresssions would be cool...
      std::stringstream lineEntriesStream( line.substr( line.find_last_of(']')+1 ) );
      unsigned directoryIndex;
      std::string tmp1, tmp2, fileName;

      lineEntriesStream >> directoryIndex >> tmp1 >> tmp2 >> fileName;
      return directories[directoryIndex-1] + "/" + fileName;
    }

    // Parse the contents of a single .debug_line contents section representing a single
    // compilation unit.
    FilesMap parseCompilationUnit( const std::vector<std::string>& drawfDumpLines, 
                                   unsigned long long &currentline ) {
      FilesMap filesMap;
      std::vector<std::string> directories;
      std::vector<std::string> files;

      currentline++;

      while ( currentline < drawfDumpLines.size() &&
              drawfDumpLines[currentline].find(".debug_line contents") == std::string::npos ) {
        std::string line = drawfDumpLines[currentline];
        std::string directory;
        std::string file;

        if ( line.find("include_directories") == 0 ) {
          directory = parseDirectoryLine( line );
          directories.push_back( directory );
        }

        if ( line.find("file_names") == 0 ) {
          file = parseFileLine( line, directories );
          files.push_back( file );
        }

        if ( line.find("0x") == 0) {
          FilenameLineNumAddressPair fileNameLineNumAddressPair = parseAddressLine( line, files );
          filesMap[ fileNameLineNumAddressPair.first ].insert( fileNameLineNumAddressPair.second );
        }

        currentline++;
      }

      return filesMap;
    }

    // Parse the drawf-dump output lines.
    void init(const std::vector<std::string>& drawfDumpLines){
      std::string line;
      unsigned compilationUnit = -1;
     
      for ( unsigned long long currentline = 0;
            currentline < drawfDumpLines.size();
            currentline++ ) {
        if ( drawfDumpLines[currentline].find(".debug_line contents") != std::string::npos ) {
          compilationUnit++;
          compilationUnitMap[compilationUnit] = parseCompilationUnit( drawfDumpLines, currentline );
        }
      }
    }

    // Executes a command a returns the output as a vector of strings
    std::vector<std::string> exec(const char* cmd) {
      FILE* pipe = popen(cmd, "r");
      std::vector<std::string> lines;
      char buffer[256];

      if (pipe) {
        while (!feof(pipe)) {
          if (fgets(buffer, 256, pipe) != NULL)
            lines.push_back(buffer);
        }
      }
      pclose(pipe);
      
      return lines;
    }
    
    public:

    BinaryAddressMap() {
    }

    // Initialize a BinaryAddressMap from an ELF executable.
    BinaryAddressMap(const std::string &binary) {
      char realpath_buffer[1024];
      std::string binaryRealPath = 
        (realpath(binary.c_str(), realpath_buffer) == NULL) ? 
        "" : realpath_buffer;
   
      if ( !binaryRealPath.empty() ) {
        const std::string cmd("llvm-dwarfdump -debug-dump=line " + binaryRealPath);
        init( exec(cmd.c_str()) );
      }
    }

    bool isEmpty() { 
      return compilationUnitMap.empty();
    }

    // Retrieve the begin and end addresses in the binary for a given line in a file.
    typedef std::pair<unsigned long long, unsigned long long> BeginEndAddressPair;
    BeginEndAddressPair getBeginEndAddressesForLine( const std::string& filePath, 
                                                     long lineNum) {
      BeginEndAddressPair beginEndAddresses;
      beginEndAddresses.first  = (unsigned long long) -1;
      beginEndAddresses.second = (unsigned long long) -1;

      // Iterate over each compilation unit
      for ( CompilationUnitMap::iterator compilationUnitMapIter = compilationUnitMap.begin();
            compilationUnitMapIter != compilationUnitMap.end();
            compilationUnitMapIter++ ) {
        FilesMap filesMap = compilationUnitMapIter->second;

        // Check if file is in the files for this compilation unit
        if ( filesMap.find( filePath ) != filesMap.end() ) {
          LineNumsToAddressesMap lineNumsToAddressesMap = filesMap[filePath];
         
          // Check if line number is in file 
          if ( lineNumsToAddressesMap.find( lineNum ) != lineNumsToAddressesMap.end() ) {
            // We found a match.  Return the starting and ending address for this line.
            LineNumsToAddressesMap::iterator lineNumsToAddressesMapIter = lineNumsToAddressesMap.find(lineNum);

            beginEndAddresses.first = lineNumsToAddressesMapIter->second;
            lineNumsToAddressesMapIter++;
            beginEndAddresses.second = lineNumsToAddressesMapIter->second;
            break;
          }     
        }
      }

      return beginEndAddresses;
    }

    unsigned long long getBeginAddressForLine( const std::string& filePath,
                                               long lineNum ) {
      return getBeginEndAddressesForLine(filePath, lineNum ).first;
    }

    unsigned long long getEndAddressForLine( const std::string& filePath,
                                             long lineNum ) {
      return getBeginEndAddressesForLine(filePath, lineNum ).second;
    }

    std::ostream& dump(std::ostream& out) {
      for ( CompilationUnitMap::iterator compilationUnitMapIter = compilationUnitMap.begin();
            compilationUnitMapIter != compilationUnitMap.end();
            compilationUnitMapIter++ ) {
        FilesMap filesMap = compilationUnitMapIter->second;
        out << "CU: " << compilationUnitMapIter->first << std::endl;

        for ( FilesMap::iterator filesMapIter = filesMap.begin();
              filesMapIter != filesMap.end();
              filesMapIter++ ) {
          LineNumsToAddressesMap lineNumsToAddressesMap = filesMapIter->second;

          out << "\t" << filesMapIter->first << ": " << std::endl;
          for ( LineNumsToAddressesMap::iterator lineNumsToAddressesMapIter = lineNumsToAddressesMap.begin();
                lineNumsToAddressesMapIter != lineNumsToAddressesMap.end();
                lineNumsToAddressesMapIter++ ) {
            out << "\t\t" << std::hex << lineNumsToAddressesMapIter->first 
                << ", " << lineNumsToAddressesMapIter->second << std::endl;
          }
        }
      }
     
      return out;
    }
  };
  
  class ASTBinaryAddressLister : public ASTConsumer,
                                 public RecursiveASTVisitor<ASTBinaryAddressLister> {
    typedef RecursiveASTVisitor<ASTBinaryAddressLister> base;

  public:
    ASTBinaryAddressLister(raw_ostream *Out = NULL,
                           StringRef Binary = (StringRef) "")
      : Out(Out ? *Out : llvm::outs()),
        Binary(Binary),
        BinaryAddresses(Binary),
        PM(NULL) {}

    ~ASTBinaryAddressLister(){
      delete PM;
      PM = NULL;
    }

    virtual void HandleTranslationUnit(ASTContext &Context) {
      TranslationUnitDecl *D = Context.getTranslationUnitDecl();

      // Setup
      Counter=0;

      MainFileID=Context.getSourceManager().getMainFileID();
      MainFileName = Context.getSourceManager().getFileEntryForID(MainFileID)->getName();

      Rewrite.setSourceMgr(Context.getSourceManager(),
                           Context.getLangOpts());

      // Run Recursive AST Visitor
      TraverseDecl(D);
    };

    bool IsSourceRangeInMainFile(SourceRange r)
    {
      FullSourceLoc loc = FullSourceLoc(r.getEnd(), Rewrite.getSourceMgr());
      return (loc.getFileID() == MainFileID);
    }

    // Return true if the clang::Expr is a statement in the C/C++ grammar.
    // This is done by testing if the parent of the clang::Expr
    // is an aggregation type.  The immediate children of an aggregation
    // type are all valid statements in the C/C++ grammar.
    bool IsCompleteCStatement(Stmt *ExpressionStmt)
    {
      Stmt* parent = PM->getParent(ExpressionStmt);

      switch ( parent->getStmtClass() )
      {
      case Stmt::CapturedStmtClass:
      case Stmt::CompoundStmtClass:
      case Stmt::CXXCatchStmtClass:
      case Stmt::CXXForRangeStmtClass:
      case Stmt::CXXTryStmtClass:
      case Stmt::DoStmtClass:
      case Stmt::ForStmtClass:
      case Stmt::IfStmtClass:
      case Stmt::SwitchStmtClass:
      case Stmt::WhileStmtClass: 
        return true;
      
      default:
        return false;
      }
    }

    void ListStmt(Stmt *S)
    {
      char Msg[256];
      SourceManager &SM = Rewrite.getSourceMgr();
      PresumedLoc BeginPLoc = SM.getPresumedLoc(S->getSourceRange().getBegin());
      PresumedLoc EndPLoc = SM.getPresumedLoc(S->getSourceRange().getEnd());

      if ( BinaryAddresses.isEmpty() ) // no binary information given
      {
        sprintf(Msg, "%8d %6d:%-3d %6d:%-3d %s",
                     Counter,
                     BeginPLoc.getLine(),
                     BeginPLoc.getColumn(),
                     EndPLoc.getLine(),
                     EndPLoc.getColumn(),
                     S->getStmtClassName());
        Out << Msg << "\n";
      }
      else 
      {
        // binary information given
        unsigned long long BeginAddress = 
          BinaryAddresses.getBeginAddressForLine( MainFileName, BeginPLoc.getLine() );
        unsigned long long EndAddress = 
          BinaryAddresses.getEndAddressForLine( MainFileName, EndPLoc.getLine() );

        if ( BeginAddress != ((unsigned long long) -1) &&
             EndAddress != ((unsigned long long) -1))
        {
          // file, linenumber could be found in the binary's debug info
          // print the begin/end address in the text segment
          sprintf(Msg, "%8d %6d:%-3d %6d:%-3d %#016x %#016x %s", 
                        Counter,
                        BeginPLoc.getLine(),
                        BeginPLoc.getColumn(),
                        EndPLoc.getLine(),
                        EndPLoc.getColumn(),
                        BeginAddress,
                        EndAddress,
                        S->getStmtClassName());
          Out << Msg << "\n";
        }
      }
    }

    virtual bool VisitDecl(Decl *D){
      // Set a flag if we are in a new declaration
      // section.  There is a tight coupling between
      // this action and VisitStmt(Stmt* ).
      if (D->getKind() == Decl::Function) {
        IsNewFunctionDecl = true; 
      }

      return true;
    }

    virtual bool VisitStmt(Stmt *S){
      SourceRange R;

      // Test if we are in a new function
      // declaration.  If so, update the parent
      // map with the root of this function declaration.
      if ( IsNewFunctionDecl ) {
        delete PM;
        PM = new ParentMap(S);
        IsNewFunctionDecl = false;
      }
 
      switch (S->getStmtClass()){

      case Stmt::NoStmtClass:
        return true;

      // These classes of statements
      // correspond to exactly 1 or more
      // lines in a source file.
      case Stmt::BreakStmtClass:
      case Stmt::CapturedStmtClass:
      case Stmt::CompoundStmtClass:
      case Stmt::ContinueStmtClass:
      case Stmt::CXXCatchStmtClass:
      case Stmt::CXXForRangeStmtClass:
      case Stmt::CXXTryStmtClass:
      case Stmt::DeclStmtClass:
      case Stmt::DoStmtClass:
      case Stmt::ForStmtClass:
      case Stmt::GotoStmtClass:
      case Stmt::IfStmtClass:
      case Stmt::IndirectGotoStmtClass:
      case Stmt::ReturnStmtClass:
      case Stmt::SwitchStmtClass:
      case Stmt::DefaultStmtClass: 
      case Stmt::CaseStmtClass: 
      case Stmt::WhileStmtClass:
        R = S->getSourceRange();
        if(IsSourceRangeInMainFile(R)){
          ListStmt(S);
        }
        break;

      // These expression may correspond
      // to one or more lines in a source file.
      case Stmt::AtomicExprClass:
      case Stmt::CXXMemberCallExprClass:
      case Stmt::CXXOperatorCallExprClass:
      case Stmt::CallExprClass:
        R = S->getSourceRange();
        if(IsSourceRangeInMainFile(R) && 
           IsCompleteCStatement(S)){
          ListStmt(S);
        }
        break;
      default:
        break;
      }

      Counter++;
      return true;
    }

  private:
    raw_ostream &Out;
    StringRef Binary;

    BinaryAddressMap BinaryAddresses;

    Rewriter Rewrite;
    ParentMap* PM;
    bool IsNewFunctionDecl;
    unsigned int Counter;
    FileID MainFileID;
    std::string MainFileName;
  };
}

ASTConsumer *clang::CreateASTBinaryAddressLister(StringRef Binary){
  return new ASTBinaryAddressLister(0, Binary);
}
