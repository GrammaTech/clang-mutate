/*
 Class to hold the parsed results of llvm-dwarfdump -debug-dump=line
 This class will store all compilation units in a binary, the files
 in those compilation units, and the mapping file line number -> address
 in binary.

 @FUTURE: Use libdwarf on the raw binary to get this data.
*/

#ifndef BINARY_ADDRESS_MAP_HPP
#define BINARY_ADDRESS_MAP_HPP

#include <map>
#include <set>
#include <vector>

#include "third-party/elfio-3.2/elfio/elfio.hpp"

#include "CompilationDataMap.h"

namespace clang_mutate{
  typedef std::pair<unsigned long, unsigned long> AddressRange;
  typedef unsigned char Byte;
  typedef std::vector<Byte> Bytes;
  typedef std::pair<AddressRange, Bytes> BinaryData;

  class BinaryAddressMap : public CompilationDataMap<BinaryData> {
  public:
    typedef std::pair<unsigned int, AddressRange> LineNumAddressPair;
    typedef std::pair<std::string, LineNumAddressPair> FilenameLineNumAddressPair;
    typedef std::map<unsigned int, AddressRange> LineNumsToAddressesMap;
    typedef std::map<std::string, LineNumsToAddressesMap> FilesMap;
    typedef std::map<unsigned int, FilesMap> CompilationUnitMap;

    typedef std::map<std::string, std::string> DwarfFilepathMap;

    // Construct an empty BinaryAddressMap
    BinaryAddressMap();

    // Initialize a BinaryAddressMap from an ELF executable.
    BinaryAddressMap(const std::string &binary,
                     const std::string &dwarfFilepathMapping);

    // Copy Constructor
    BinaryAddressMap(const BinaryAddressMap& other);

    // Overloaded assignment operator
    BinaryAddressMap& operator=(const BinaryAddressMap& other);

    // Destructor
    ~BinaryAddressMap();

    // Return true if the BinaryAddressMap is empty
    virtual bool isEmpty() const override;

    // Return the path to the executable utilized to populate the map.
    virtual std::string getPath() const override;

    virtual Utils::Optional<BinaryData>
    getCompilationData( const std::string & filePath,
                        const LineRange & lineRange )
                        const override;
  private:
    ELFIO::elfio m_elf;
    CompilationUnitMap m_compilationUnitMap;
    DwarfFilepathMap m_dwarfFilepathMap;
    std::string m_binaryPath;

    // Parse two contiguous lines in the form "%0x     %d     %d     %d  %d  %s"
    // from the output of llvm-dwarfdump
    //  %0x#1: Address in binary
    //  %d#1: Line number in source
    //  %d#3: File index
    FilenameLineNumAddressPair parseAddressLine( const std::string &line,
                                                 const std::string &nextLine,
                                                 const std::vector<std::string> &files );

    // Parse a single line in the form "include_directories[ %d] = '%s'"
    // from the output of llvm-dwarfdump
    //  %d#1: Directory index (1..n)
    //  %s#1: File name
    // This will return the directory name (%s)
    // expanded to the full absolute path.
    std::string parseDirectoryLine( const std::string &line,
                                    const std::set<std::string>& sourcePaths );

    // Return the absolute path to filename by testing for the
    // existance of directory/fileName and then each sourcepath/directory/fileName
    // in turn.  This operates equivalently to GDB when attempting to load
    // source files.
    // See sourceware.org/gdb/onlinedocs/gdb/Source-Path.html for more
    // information.
    std::string findOnSourcePath( const std::set<std::string>& sourcePaths,
                                  const std::string& directory,
                                  const std::string& fileName = "");

    // Parse a single line in the form "file_names[ %d]   %d %0x %0x %s"
    // from the output of llvm-dwarfdump
    //  %d#1: File name index (1..n)
    //  %d#2: Directory index (1..n)
    //  %s#1: File name
    // This will return the file name appended to the directory associated with this file
    std::string parseFileLine( const std::string &line,
                               const std::set<std::string>& sourcePaths,
                               const std::vector<std::string> &directories );

    // Parse the contents of a single .debug_line contents section representing a single
    // compilation unit from the output of llvm-drawfdump.
    FilesMap parseCompilationUnit( const std::vector<std::string>& drawfDumpLines,
                                   const std::set<std::string>& sourcePaths,
                                   unsigned long long &currentline );

    // Return the set of paths to search when finding the absolute location
    // of a source file.
    //
    // It is equivalent to the default GDB source path of $cdir:$cwd
    // where $cdir is the compilation directory and $cwd is the
    // current working directory.
    // See sourceware.org/gdb/onlinedocs/gdb/Source-Path.html for more
    // information.
    std::set< std::string > getSourcePaths( const std::vector<std::string>& drawfDumpDebugInfo );

    // Parse mappings of the form <Dwarf filepath1>=<NewFilePath1>,...
    // into the m_dwarfFilepathMap
    void parseDwarfFilepathMapping(const std::string &dwarfFilepathMapping);

    // Return the binary address range for the given file:start,end
    Utils::Optional<AddressRange>
    getAddressRangeForLines( const std::string& filePath,
                             const LineRange & lineRange) const;

    // Get the raw contents of a binary from [startAddress, endAddress)
    // as a sequence of hex digits.
    Bytes getBinaryContents( unsigned long startAddress,
                             unsigned long endAddress ) const;

    // Deep copy other's members
    void copy(const BinaryAddressMap& other);

    // Initialize from the llvm-drawfdump .debug-dump=line output lines.
    // Source paths is a set of paths to search when locating files.
    void init(const std::vector<std::string>& drawfDumpDebugLine,
              const std::set< std::string >& searchPaths);
  };
}

#endif
