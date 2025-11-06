#include "Client.h"
#include "Logger.h"
#include "Validator.h"
#include "InputParser.h"
#include <iostream>
#include <algorithm>

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

// Обрабатывает один запрос клиента
bool processClientRequest(Client& client) {
    // 1. Ввод описания графа
    std::cout << "\nВведите описание графа (формат: A B, B C, C D, ...):" << std::endl;
    std::cout << "или 'exit' для завершения работы: ";
    
    std::string graphInput;
    std::getline(std::cin, graphInput);
    
    // Проверяем команду выхода
    if (graphInput == "exit") {
        return false;
    }
    
    // Парсим граф (получаем рёбра со строковыми именами)
    std::vector<InputParser::Edge> stringEdges;
    if (!InputParser::parseGraph(graphInput, stringEdges)) {
        Logger::error("Неверный формат ввода графа");
        return true; // Продолжаем работу
    }
    
    // Преобразуем строковые имена в числовые индексы
    std::vector<std::pair<int, int>> numericEdges;
    std::map<std::string, int> vertexMap;
    InputParser::convertToNumeric(stringEdges, numericEdges, vertexMap);
    
    // Формируем список рёбер в формате vector<vector<int>>
    std::vector<std::vector<int>> edges;
    for (const auto& edge : numericEdges) {
        edges.push_back({edge.first, edge.second});
    }
    
    // Валидация размера графа
    int numVertices = vertexMap.size();
    int numEdges = edges.size();
    
    std::string errorMessage;
    if (!Validator::isValidGraphSize(numVertices, numEdges, errorMessage)) {
        Logger::error(errorMessage);
        return true;
    }
    
    // 2. Ввод начальной и конечной вершин
    std::cout << "Введите начальную и конечную вершины (формат: A B): ";
    std::string verticesInput;
    std::getline(std::cin, verticesInput);
    
    std::string startVertexName, endVertexName;
    if (!InputParser::parseVertices(verticesInput, startVertexName, endVertexName)) {
        Logger::error("Неверный формат вершин");
        return true;
    }
    
    // Проверяем, что вершины существуют в графе
    if (!Validator::isValidVertex(startVertexName, vertexMap)) {
        Logger::error("Вершины не найдены в графе");
        return true;
    }
    if (!Validator::isValidVertex(endVertexName, vertexMap)) {
        Logger::error("Вершины не найдены в графе");
        return true;
    }
    
    // Получаем числовые индексы вершин
    int startVertex = InputParser::getVertexIndex(startVertexName, vertexMap);
    int endVertex = InputParser::getVertexIndex(endVertexName, vertexMap);
    
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
        std::cout << "\nРезультат: " << response.path_length << endl;
        
        // Создаём обратный словарь (индекс -> имя)
        std::map<int, std::string> indexToName;
        for (const auto& pair : vertexMap) {
            indexToName[pair.second] = pair.first;
        }
        
        // Выводим путь с именами вершин
        std::cout << "Путь: ";
        for (size_t i = 0; i < response.path.size(); i++) {
            int nodeIndex = response.path[i];
            
            // Находим имя вершины по индексу
            if (indexToName.find(nodeIndex) != indexToName.end()) {
                std::cout << indexToName[nodeIndex];
            } else {
                std::cout << nodeIndex; // На случай, если имя не найдено
            }
            
            if (i < response.path.size() - 1) {
                std::cout << " -> ";
            }
        }
        std::cout << endl;
        
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
    // argv[0] - имя программы
    // argv[1] - IP-адрес
    // argv[2] - протокол
    // argv[3] - порт
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