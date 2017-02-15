#ifndef COMPILATION_DATA_MAP_H
#define COMPILATION_DATA_MAP_H

#include <string>

#include "Optional.h"

namespace clang_mutate {

typedef std::pair<unsigned long, unsigned long> LineRange;

template <class T>
class CompilationDataMap {
public:
    virtual ~CompilationDataMap() {}

    // Return true if the CompilationDataMap is empty
    virtual bool isEmpty() const = 0;

    // Return path to the file which initialized the CompilationDataMap
    virtual std::string getPath() const = 0;

    // Return the Compilation Data assocated with the given file:begin,end
    virtual Utils::Optional<T>
    getCompilationData(const std::string& filePath,
                       const LineRange& lineRange) const = 0;
};

}

#endif
