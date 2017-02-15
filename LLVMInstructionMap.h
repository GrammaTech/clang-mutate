#ifndef LLVM_INSTRUCTION_MAP_HPP
#define LLVM_INSTRUCTION_MAP_HPP

#include <map>
#include <vector>

#include "CompilationDataMap.h"

namespace clang_mutate{

typedef std::vector<std::string> Instructions;

class LLVMInstructionMap : public CompilationDataMap<Instructions> {
public:
    typedef std::map<unsigned int, Instructions>
            LineNumsToInstructionsMap;
    typedef std::map<std::string, LineNumsToInstructionsMap> FilesMap;

    // Construct an empty LLVMInstructionMap
    LLVMInstructionMap();

    // Initialize a LLVMInstructionMap from a LLVM file
    LLVMInstructionMap(const std::string &llvmPath);

    // Copy Constructor
    LLVMInstructionMap(const LLVMInstructionMap& other);

    // Overloaded assignment operator
    LLVMInstructionMap& operator=(const LLVMInstructionMap& other);

    // Destructor
    virtual ~LLVMInstructionMap();

    // Return true if the LLVMInstructionMap is empty
    virtual bool isEmpty() const override;

    // Return the path to the LLVM file utilized to populate the map
    virtual std::string getPath() const override;

    // Return the instructions corresponding to the given file/line range.
    virtual Utils::Optional<Instructions>
    getCompilationData(const std::string& filePath,
                       const LineRange& lineRange)
                       const override;
private:
    std::string m_llvmPath;
    FilesMap m_filesMap;
};

}

#endif
