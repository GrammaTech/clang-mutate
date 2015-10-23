/* 
 Class to hold the parsed results of llvm-dwarfdump -debug-dump=line 
 This class will store all compilation units in a binary, the files
 in those compilation units, and the mapping file line number -> address
 in binary.

 @FUTURE: Use libdrawf on the raw binary to get this data.
*/

#ifndef BINARY_ADDRESS_MAP_HPP
#define BINARY_ADDRESS_MAP_HPP

#include <map>
#include <string>
#include <vector>

namespace clang_mutate{
  class BinaryAddressMap {
  public:
    typedef std::pair<unsigned int, unsigned long> LineNumAddressPair;
    typedef std::pair<std::string, LineNumAddressPair> FilenameLineNumAddressPair;
    typedef std::map<unsigned int, unsigned long> LineNumsToAddressesMap;
    typedef std::map<std::string, LineNumsToAddressesMap> FilesMap;
    typedef std::map<unsigned int, FilesMap> CompilationUnitMap;

    typedef unsigned char Byte;
    typedef std::vector<Byte> Bytes;
    typedef std::map<unsigned long, Bytes > BinaryContentsMap;

    // Construct an empty BinaryAddressMap
    BinaryAddressMap();

    // Initialize a BinaryAddressMap from an ELF executable.
    BinaryAddressMap(const std::string &binary);

    // Return true if the BinaryAddressMap is empty
    bool isEmpty() const;

    // Return the path to the executable utilized to populate the map.
    std::string getBinaryPath() const;

    // Return true if the begin address for a given line in a file can be found.
    bool canGetBeginAddressForLine( const std::string& filePath,
                                    unsigned int lineNum ) const;

    // Return true if the end address for a given line in a file can be found.
    bool canGetEndAddressForLine( const std::string& filePath,
                                  unsigned int lineNum ) const;

    // Retrieve the begin address for a given line in a file.
    unsigned long getBeginAddressForLine( const std::string& filePath,
                                          unsigned int lineNum ) const;

    // Retrieve the end address for a given line in a file.
    unsigned long getEndAddressForLine( const std::string& filePath,
                                        unsigned int lineNum ) const;

    // Retrieve the begin and end addresses in the binary for a given line in a file.
    typedef std::pair<unsigned long, unsigned long> BeginEndAddressPair;
    BeginEndAddressPair getBeginEndAddressesForLine( const std::string& filePath, 
                                                     unsigned int lineNum) const;

    // Get the raw contents of a binary from [startAddress, endAddress)
    // as a sequence of hex digits.
    Bytes getBinaryContents( unsigned long startAddress,
                             unsigned long endAddress );

    // Get the raw contents of a binary from [startAddress, endAddress)
    // as a sequence of hex digits.
    std::string getBinaryContentsAsStr( unsigned long startAddress,
                                        unsigned long endAddress ); 

    // Dump the BinaryAddressMap to stream.
    std::ostream& dump(std::ostream& out) const;

  private:
    BinaryContentsMap m_binaryContentsCache;
    CompilationUnitMap m_compilationUnitMap;
    std::string m_binaryPath;

    // Return the results of using objdump to disassemble
    // from [beginAddress, endAddress)
    std::vector<std::string> objdumpDisassemble( unsigned long beginAddress,
                                                 unsigned long endAddress);

    // Parse the results of using objdump to disassemble 
    // into the binary contents cache
    BinaryContentsMap parseObjdumpDisassembly( const std::vector<std::string> &objdumpLines );

    // Fill the binary contents cache with the results of 
    // using objdump to disassemble from [beginAddress, endAddress)
    void fillBinaryContentsCache( unsigned long beginAddress, unsigned long endAddress );

    // Parse a single line in the form "%0x     %d     %d     %d  %d  %s"
    // from the output of llvm-dwarfdump
    //  %0x#1: Address in binary
    //  %d#1: Line number in source
    //  %d#3: File index
    FilenameLineNumAddressPair parseAddressLine( const std::string &line, 
                                                 const std::vector<std::string> &files );

    // Parse a single line in the form "include_directories[ %d] = '%s'"
    // from the output of llvm-dwarfdump
    //  %d#1: Directory index (1..n)
    //  %s#1: File name
    // This will return the directory name (%s) normalized by realpath.
    std::string parseDirectoryLine( const std::string &line );

    // Parse a single line in the form "file_names[ %d]   %d %0x %0x %s"
    // from the output of llvm-dwarfdump
    //  %d#1: File name index (1..n)
    //  %d#2: Directory index (1..n)
    //  %s#1: File name
    // This will return the file name appended to the directory associated with this file
    std::string parseFileLine( const std::string &line, const std::vector<std::string> &directories );

    // Parse the contents of a single .debug_line contents section representing a single
    // compilation unit from the output of llvm-drawfdump.
    FilesMap parseCompilationUnit( const std::vector<std::string>& drawfDumpLines, 
                                   unsigned long long &currentline );

    // Initialize from the llvm-drawfdump output lines.
    void init(const std::vector<std::string>& drawfDumpLines);

  };
}

#endif
