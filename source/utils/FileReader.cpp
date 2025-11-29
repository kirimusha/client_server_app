#include "FileReader.h"
#include "Logger.h"
#include "InputParser.h"
#include <fstream>
#include <sstream>
#include <map>

using namespace std;

Graph FileReader::readGraphFromFile(const std::string& filename) {
    ifstream file(filename);
    Graph graph;
    
    if (!file.is_open()) {
        Logger::error("Не удалось открыть файл: " + filename);
        return graph;
    }
    
    string fileContent;
    string line;
    
    // Читаем весь файл
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (!fileContent.empty()) fileContent += ", ";
        fileContent += line;
    }
    file.close();
    
    if (fileContent.empty()) {
        Logger::error("Файл пуст или содержит только комментарии: " + filename);
        return graph;
    }
    
    // Парсим граф используя существующий InputParser
    vector<InputParser::Edge> stringEdges;
    if (!InputParser::parseGraph(fileContent, stringEdges)) {
        Logger::error("Неверный формат графа в файле: " + filename);
        return graph;
    }
    
    // Преобразуем строковые имена в числовые индексы
    map<string, int> vertexMap;
    int nextIndex = 0;
    
    // Сначала создаем отображение всех вершин
    for (const auto& edge : stringEdges) {
        if (vertexMap.find(edge.vertex1) == vertexMap.end()) {
            vertexMap[edge.vertex1] = nextIndex++;
        }
        if (vertexMap.find(edge.vertex2) == vertexMap.end()) {
            vertexMap[edge.vertex2] = nextIndex++;
        }
    }
    
    // Создаем список числовых ребер
    vector<vector<int>> numericEdges;
    for (const auto& edge : stringEdges) {
        int from = vertexMap[edge.vertex1];
        int to = vertexMap[edge.vertex2];
        numericEdges.push_back({from, to});
    }
    
    // Добавляем все ребра в граф
    graph.addEdges(numericEdges);
    
    Logger::info("Граф загружен из файла: " + filename + " с " + 
                to_string(vertexMap.size()) + " вершинами и " + 
                to_string(stringEdges.size()) + " рёбрами");
    return graph;
}

bool FileReader::validateGraphFile(const std::string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    string fileContent;
    string line;
    
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (!fileContent.empty()) fileContent += ", ";
        fileContent += line;
    }
    file.close();
    
    if (fileContent.empty()) {
        return false;
    }
    
    vector<InputParser::Edge> testEdges;
    return InputParser::parseGraph(fileContent, testEdges);
}

std::vector<std::string> FileReader::readGraphFileLines(const std::string& filename) {
    vector<string> lines;
    ifstream file(filename);
    
    if (file.is_open()) {
        string line;
        while (getline(file, line)) {
            if (!line.empty() && line[0] != '#') {
                lines.push_back(line);
            }
        }
        file.close();
    }
    
    return lines;
}