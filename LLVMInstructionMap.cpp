#include "LLVMInstructionMap.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/raw_ostream.h"

#include "Utils.h"

namespace clang_mutate {

LLVMInstructionMap::LLVMInstructionMap() {}

LLVMInstructionMap::LLVMInstructionMap(const std::string &llvm) {
    m_llvmPath = Utils::safe_realpath(llvm);

    if (!m_llvmPath.empty() && Utils::fileExists(m_llvmPath)) {
        llvm::SMDiagnostic err;
        std::unique_ptr<llvm::Module> mod =
            parseIRFile(m_llvmPath, err, llvm::getGlobalContext());

        for (auto functionIter = mod->begin();
             functionIter != mod->end();
             functionIter++) {
            for (auto basicBlockIter = functionIter->begin();
                 basicBlockIter != functionIter->end();
                 basicBlockIter++) {
                for (auto instrIter = basicBlockIter->begin();
                     instrIter != basicBlockIter->end();
                     instrIter++) {
                    llvm::DILocation* debugLoc = instrIter->getDebugLoc().get();

                    if (debugLoc) {
                        std::string directory = debugLoc->getDirectory();
                        std::string filename = debugLoc->getFilename();
                        std::string path = Utils::rtrim(directory, "/") + "/" +
                                           filename;
                        unsigned int line = debugLoc->getLine();
                        std::string instrStr;
                        llvm::raw_string_ostream instrStream(instrStr);

                        path = Utils::safe_realpath(path);
                        instrIter->print(instrStream);
                        instrStr = Utils::trim(instrStr);
                        instrStr = instrStr.find(", !dbg") != std::string::npos ?
                                   instrStr.substr(0, instrStr.find(", !dbg")) :
                                   instrStr;

                        m_filesMap[path][line].push_back(instrStr);
                    }
                }
            }
        }
    }
}

LLVMInstructionMap::LLVMInstructionMap(const LLVMInstructionMap& other):
    m_llvmPath(other.m_llvmPath),
    m_filesMap(other.m_filesMap) {
}

LLVMInstructionMap&
LLVMInstructionMap::operator=(const LLVMInstructionMap& other) {
    m_llvmPath = other.m_llvmPath;
    m_filesMap = other.m_filesMap;

    return *this;
}

LLVMInstructionMap::~LLVMInstructionMap() {
}

bool LLVMInstructionMap::isEmpty() const {
    return m_filesMap.empty();
}

std::string LLVMInstructionMap::getPath() const {
    return m_llvmPath;
}

Utils::Optional<Instructions>
LLVMInstructionMap::getCompilationData(const std::string& filePath,
                                       const LineRange& lineRange) const {
    Instructions instructions;
    LineRange tempLineRange(lineRange);

    if (m_filesMap.find(filePath) != m_filesMap.end()) {
        LineNumsToInstructionsMap ln2im = m_filesMap.at(filePath);
        LineNumsToInstructionsMap::const_iterator search = ln2im.end();

        while (search == ln2im.end() &&
               tempLineRange.first <= lineRange.second) {
            search = ln2im.find(tempLineRange.first);
            tempLineRange.first++;
        }

        while (search != ln2im.end() && search->first <= lineRange.second) {
            instructions.insert(instructions.end(),
                                search->second.begin(),
                                search->second.end());
            ++search;
        }
    }

    return instructions.empty() ?
           Utils::Optional<Instructions>() :
           Utils::Optional<Instructions>(instructions);
}

}
