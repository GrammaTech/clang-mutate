/* 
 Class to hold the parsed results of llvm-dwarfdump -debug-dump=line 
 This class will store all compilation units in a binary, the files
 in those compilation units, and the mapping file line number -> address
 in binary.

 @FUTURE: Use libdrawf on the raw binary to get this data.
*/

#ifndef BINARY_ADDRESS_MAP_HPP
#define BINARY_ADDRESS_MAP_HPP

#include <cstdio>
#include <map>
#include <string>
#include <sstream>
#include <vector>

namespace clang_mutate{
  class BinaryAddressMap {
  private:
    typedef std::pair<unsigned int, unsigned long> LineNumAddressPair;
    typedef std::pair<std::string, LineNumAddressPair> FilenameLineNumAddressPair;
    typedef std::map<unsigned int, unsigned long> LineNumsToAddressesMap;
    typedef std::map<std::string, LineNumsToAddressesMap> FilesMap;
    typedef std::map<unsigned int, FilesMap> CompilationUnitMap;

    CompilationUnitMap m_compilationUnitMap;
    std::string m_binaryPath;

    // Parse a single line in the form "%0x     %d     %d     %d  %d  %s"
    //  %0x#1: Address in binary
    //  %d#1: Line number in source
    //  %d#3: File index
    FilenameLineNumAddressPair parseAddressLine( const std::string &line, 
                                                 const std::vector<std::string> &files ) {
      // Regular expressions would be cool...
      std::stringstream lineEntriesStream( line );
      FilenameLineNumAddressPair filenameLineNumAddressPair;
      unsigned long address;
      unsigned int lineNum;
      unsigned int tmp, fileIndex;
    
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
      unsigned int directoryIndex;
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
      unsigned int compilationUnit = -1;
     
      for ( unsigned long long currentline = 0;
            currentline < drawfDumpLines.size();
            currentline++ ) {
        if ( drawfDumpLines[currentline].find(".debug_line contents") != std::string::npos ) {
          compilationUnit++;
          m_compilationUnitMap[compilationUnit] = parseCompilationUnit( drawfDumpLines, currentline );
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
    BinaryAddressMap(const std::string &binary)
    {
      char realpath_buffer[1024];
      m_binaryPath = (realpath(binary.c_str(), realpath_buffer) == NULL) ? 
                     "" : realpath_buffer;
   
      if ( !m_binaryPath.empty() ) {
        const std::string cmd("llvm-dwarfdump -debug-dump=line " + m_binaryPath);
        init( exec(cmd.c_str()) );
      }
    }

    bool isEmpty() const { 
      return m_compilationUnitMap.empty();
    }

    std::string getBinaryPath() const {
      return m_binaryPath;
    }

    // Retrieve the begin and end addresses in the binary for a given line in a file.
    typedef std::pair<unsigned long, unsigned long> BeginEndAddressPair;
    BeginEndAddressPair getBeginEndAddressesForLine( const std::string& filePath, 
                                                     unsigned int lineNum) const {
      BeginEndAddressPair beginEndAddresses;
      beginEndAddresses.first  = (unsigned long) -1;
      beginEndAddresses.second = (unsigned long) -1;

      // Iterate over each compilation unit
      for ( CompilationUnitMap::const_iterator compilationUnitMapIter = m_compilationUnitMap.begin();
            compilationUnitMapIter != m_compilationUnitMap.end();
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

    unsigned long getBeginAddressForLine( const std::string& filePath,
                                          unsigned int lineNum ) const {
      return getBeginEndAddressesForLine(filePath, lineNum ).first;
    }

    unsigned long getEndAddressForLine( const std::string& filePath,
                                        unsigned int lineNum ) const {
      return getBeginEndAddressesForLine(filePath, lineNum ).second;
    }

    std::ostream& dump(std::ostream& out) const {
      for ( CompilationUnitMap::const_iterator compilationUnitMapIter = m_compilationUnitMap.begin();
            compilationUnitMapIter != m_compilationUnitMap.end();
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
}

#endif
