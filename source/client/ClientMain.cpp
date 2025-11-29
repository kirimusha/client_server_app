#include "Client.h"
#include "Logger.h"
#include "Validator.h"
#include "InputParser.h"
#include <iostream>
#include <algorithm>
#include <map>
#include <fstream>
#include <sstream>

using namespace std;

// Выводит справку по использованию программы
void printUsage(const char* programName) {
    cout << "Использование: " << programName << " <IP-адрес> <протокол> <порт>" << endl;
    cout << endl;
    cout << "Параметры:" << endl;
    cout << "  <IP-адрес>  - IP-адрес сервера (например, 127.0.0.1)" << endl;
    cout << "  <протокол>  - Тип протокола: tcp или udp" << endl;
    cout << "  <порт>      - Номер порта сервера (1024-65535)" << endl;
    cout << endl;
    cout << "Примеры:" << endl;
    cout << "  " << programName << " 127.0.0.1 tcp 8080" << endl;
    cout << "  " << programName << " 192.168.1.10 udp 12345" << endl;
}

// Проверяет, является ли ввод ссылкой на файл
bool isFileInput(const string& input) {
    return input.find("file:") == 0 || 
           input.find(".txt") != string::npos ||
           input.find(".graph") != string::npos;
}

// Извлекает имя файла из ввода
string extractFilename(const string& input) {
    if (input.find("file:") == 0) {
        return input.substr(5); // убираем "file:"
    }
    return input;
}

// Читает граф из файла
bool readGraphFromFile(const string& filename, vector<InputParser::Edge>& edges, string& errorMsg) {
    ifstream file(filename);
    if (!file.is_open()) {
        errorMsg = "Не удалось открыть файл: " + filename;
        return false;
    }
    
    string line;
    int lineNum = 0;
    
    while (getline(file, line)) {
        lineNum++;
        
        // Пропускаем пустые строки и комментарии
        if (line.empty() || line[0] == '#') continue;
        
        // Убираем лишние пробелы
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (line.empty()) continue;
        
        // Парсим строку с рёбрами
        if (!InputParser::parseGraph(line, edges)) {
            errorMsg = "Ошибка в строке " + to_string(lineNum) + ": " + line;
            file.close();
            return false;
        }
    }
    
    file.close();
    
    if (edges.empty()) {
        errorMsg = "Файл не содержит корректных рёбер графа";
        return false;
    }
    
    return true;
}

// Валидирует файл графа
bool validateGraphFile(const string& filename, string& errorMsg) {
    ifstream file(filename);
    if (!file.is_open()) {
        errorMsg = "Файл не найден: " + filename;
        return false;
    }
    
    string line;
    int lineNum = 0;
    vector<InputParser::Edge> testEdges;
    
    while (getline(file, line)) {
        lineNum++;
        if (line.empty() || line[0] == '#') continue;
        
        if (!InputParser::parseGraph(line, testEdges)) {
            errorMsg = "Неверный формат в строке " + to_string(lineNum) + ": " + line;
            file.close();
            return false;
        }
    }
    
    file.close();
    
    if (testEdges.empty()) {
        errorMsg = "Файл не содержит корректных данных графа";
        return false;
    }
    
    return true;
}

// Обрабатывает один запрос клиента
bool processClientRequest(Client& client) {
    // Ввод описания графа или имени файла
    cout << "\nВведите описание графа (формат: A B, B C, C D, ...)" << endl;
    cout << "или имя файла (file:graph.txt или просто graph.txt)" << endl;
    cout << "или 'exit' для завершения работы: ";
    
    string graphInput;
    getline(cin, graphInput);
    
    // Проверяем команду выхода
    if (graphInput == "exit") {
        return false;
    }
    
    vector<InputParser::Edge> stringEdges;
    
    // Обрабатываем ввод из файла
    if (isFileInput(graphInput)) {
        string filename = extractFilename(graphInput);
        string errorMsg;
        
        // Валидируем файл
        if (!validateGraphFile(filename, errorMsg)) {
            Logger::error(errorMsg);
            return true;
        }
        
        // Читаем граф из файла
        if (!readGraphFromFile(filename, stringEdges, errorMsg)) {
            Logger::error(errorMsg);
            return true;
        }
        
        Logger::info("Граф успешно загружен из файла: " + filename);
    } 
    // Обрабатываем прямой ввод
    else {
        if (!InputParser::parseGraph(graphInput, stringEdges)) {
            Logger::error("Неверный формат ввода графа");
            return true;
        }
    }
    
    // ПРЕОБРАЗУЕМ СТРОКОВЫЕ ИМЕНА В ЧИСЛОВЫЕ ИНДЕКСЫ ВРУЧНУЮ
    vector<vector<int>> edges;
    map<string, int> vertexMap;
    int nextIndex = 0;
    
    // Создаём отображение вершин и числовые рёбра
    for (const auto& edge : stringEdges) {
        // Добавляем вершины в отображение если их ещё нет
        if (vertexMap.find(edge.vertex1) == vertexMap.end()) {
            vertexMap[edge.vertex1] = nextIndex++;
        }
        if (vertexMap.find(edge.vertex2) == vertexMap.end()) {
            vertexMap[edge.vertex2] = nextIndex++;
        }
        
        // Создаём числовое ребро
        edges.push_back({vertexMap[edge.vertex1], vertexMap[edge.vertex2]});
    }
    
    // Валидация размера графа
    int numVertices = vertexMap.size();
    
    string errorMessage;
    if (!Validator::isValidGraphSize(numVertices, errorMessage)) {
        Logger::error(errorMessage);
        return true;
    }
    
    // 2. Ввод начальной и конечной вершин
    cout << "Введите начальную и конечную вершины (формат: A B): ";
    string verticesInput;
    getline(cin, verticesInput);
    
    string startVertexName, endVertexName;
    if (!InputParser::parseVertices(verticesInput, startVertexName, endVertexName)) {
        Logger::error("Неверный формат вершин");
        return true;
    }
    
    // ПРОВЕРЯЕМ ВЕРШИНЫ ВРУЧНУЮ (вместо Validator::isValidVertex)
    if (vertexMap.find(startVertexName) == vertexMap.end()) {
        Logger::error("Начальная вершина не найдена в графе: " + startVertexName);
        return true;
    }
    if (vertexMap.find(endVertexName) == vertexMap.end()) {
        Logger::error("Конечная вершина не найдена в графе: " + endVertexName);
        return true;
    }
    
    // ПОЛУЧАЕМ ИНДЕКСЫ ВЕРШИН ВРУЧНУЮ (вместо InputParser::getVertexIndex)
    int startVertex = vertexMap[startVertexName];
    int endVertex = vertexMap[endVertexName];
    
    // 3. Формируем запрос
    ClientRequest request;
    request.start_node = startVertex;
    request.end_node = endVertex;
    
    // 4. Отправляем запрос на сервер
    ServerResponse response;
    if (!client.sendRequest(request, edges, response)) {
        Logger::error("Не удалось получить ответ от сервера");
        return false; // Прекращаем работу при ошибке связи
    }
    
    // 5. Обрабатываем ответ
    if (response.error_code == SUCCESS) {
        cout << "\nРезультат: " << response.path.size() - 1 << endl;
        
        // Создаём обратный словарь (индекс -> имя)
        map<int, string> indexToName;
        for (const auto& pair : vertexMap) {
            indexToName[pair.second] = pair.first;
        }
        
        // Выводим путь с именами вершин
        cout << "Путь: ";
        for (size_t i = 0; i < response.path.size(); i++) {
            int nodeIndex = response.path[i];
            
            // Находим имя вершины по индексу
            if (indexToName.find(nodeIndex) != indexToName.end()) {
                cout << indexToName[nodeIndex];
            } else {
                cout << nodeIndex; // На случай, если имя не найдено
            }
            
            if (i < response.path.size() - 1) {
                cout << " -> ";
            }
        }
        cout << endl;
        
    } else if (response.error_code == NO_PATH) {
        Logger::error("Путь между вершинами не существует");
    } else if (response.error_code == INVALID_REQUEST) {
        Logger::error("Неверный запрос");
    } else {
        Logger::error("Неизвестная ошибка");
    }
    
    return true; // Продолжаем работу
}

// Главная функция клиента
int main(int argc, char* argv[]) {
    // Проверяем количество аргументов
    if (argc != 4) {
        Logger::error("Неверное количество аргументов");
        printUsage(argv[0]);
        return 1;
    }
    
    // Получаем параметры
    string serverIP = argv[1];
    string protocol = argv[2];
    
    // Приводим протокол к нижнему регистру
    transform(protocol.begin(), protocol.end(), protocol.begin(), ::tolower);
    
    // Парсим порт
    int port;
    try {
        port = stoi(argv[3]);
    } catch (...) {
        Logger::error("Порт должен быть числом");
        printUsage(argv[0]);
        return 1;
    }
    
    // Валидация параметров
    if (!Validator::isValidIP(serverIP)) {
        Logger::error("Неверный формат IP-адреса");
        return 1;
    }
    
    if (!Validator::isValidProtocol(protocol)) {
        Logger::error("Протокол должен быть tcp или udp");
        return 1;
    }
    
    if (!Validator::isValidPort(port)) {
        Logger::error("Порт должен быть в диапазоне 1024-65535");
        return 1;
    }
    
    // Создаём и подключаем клиента
    Client client(serverIP, port, protocol);
    
    if (!client.connect()) {
        Logger::error("Не удалось подключиться к серверу");
        return 1;
    }
    
    // Главный цикл обработки запросов
    while (true) {
        try {
            if (!processClientRequest(client)) {
                // Пользователь ввёл "exit" или произошла критическая ошибка
                break;
            }
        } catch (const exception& e) {
            Logger::error(string("Ошибка: ") + e.what());
            break;
        }
    }
    
    // Отключаемся от сервера
    client.disconnect();
    Logger::info("Работа клиента завершена");
    
    return 0;
}
