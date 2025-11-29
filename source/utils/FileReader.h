#ifndef FILEREADER_H
#define FILEREADER_H

#include <string>
#include <vector>
#include "../common/Graph.h"

class FileReader {
public:
    static Graph readGraphFromFile(const std::string& filename);
    static bool validateGraphFile(const std::string& filename);
    static std::vector<std::string> readGraphFileLines(const std::string& filename);
};

#endif