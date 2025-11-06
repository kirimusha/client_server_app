#include "Server.h"
#include "Logger.h"
#include "Validator.h"
#include "common/Dijkstra.h"
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

// Размер буфера для приёма данных
const int BUFFER_SIZE = 4096;

// Конструктор сервера
Server::Server(int port, const string& protocol)
    : port(port), protocol(protocol), serverSocket(-1), isRunning(false) {
}

// Деструктор
Server::~Server() {
    stop();
}

// Запускает сервер
bool Server::start() {
    // Создаём сокет
    if (!createSocket()) {
        return false;
    }
    
    isRunning = true;
    Logger::info("Сервер запущен на порту " + to_string(port));
    return true;
}

// Останавливает сервер
void Server::stop() {
    if (!isRunning) {
        return;
    }
    
    Logger::info("Сервер завершает работу");
    isRunning = false;
    
    // Закрываем сокет
    if (serverSocket >= 0) {
        close(serverSocket);
        serverSocket = -1;
    }
    
    // Ждём завершения всех потоков клиентов
    for (auto& thread : clientThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    clientThreads.clear();
}

// Создаёт и настраивает сокет
bool Server::createSocket() {
    // Определяем тип сокета в зависимости от протокола
    int socketType = (protocol == "tcp") ? SOCK_STREAM : SOCK_DGRAM;
    
    // Создаём сокет
    // AF_INET - адресное семейство IPv4
    // SOCK_STREAM - TCP, SOCK_DGRAM - UDP
    serverSocket = socket(AF_INET, socketType, 0);
    if (serverSocket < 0) {
        Logger::error("Не удалось создать сокет");
        return false;
    }
    
    // Устанавливаем опцию SO_REUSEADDR, чтобы можно было переиспользовать адрес
    // Это полезно при быстром перезапуске сервера
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        Logger::warning("Не удалось установить SO_REUSEADDR");
    }
    
    // Настраиваем адрес сервера
    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr)); // Обнуляем структуру
    serverAddr.sin_family = AF_INET;            // IPv4
    serverAddr.sin_addr.s_addr = INADDR_ANY;    // Слушаем на всех интерфейсах
    serverAddr.sin_port = htons(port);          // Преобразуем порт в сетевой порядок байт
    
    // Привязываем сокет к адресу
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        Logger::error("Не удалось привязать сокет к порту");
        close(serverSocket);
        return false;
    }
    
    // Для TCP нужно начать слушать входящие подключения
    if (protocol == "tcp") {
        // Второй параметр - размер очереди ожидающих подключений (backlog)
        // 5 означает, что максимум 5 клиентов могут ждать подключения одновременно
        if (listen(serverSocket, 5) < 0) {
            Logger::error("Не удалось начать прослушивание");
            close(serverSocket);
            return false;
        }
    }
    
    return true;
}

// Главный цикл сервера
void Server::run() {
    if (protocol == "tcp") {
        // TCP: принимаем подключения и создаём для каждого клиента отдельный поток
        while (isRunning) {
            sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            
            // accept() блокирует выполнение до прихода нового клиента
            int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);
            
            if (clientSocket < 0) {
                if (isRunning) {
                    Logger::error("Ошибка при принятии подключения");
                }
                continue;
            }
            
            Logger::info("Подключён новый клиент");
            
            // Создаём новый поток для обработки клиента
            // thread автоматически запускает функцию в отдельном потоке
            clientThreads.emplace_back(&Server::handleTCPClient, this, clientSocket);
        }
    } else {
        // UDP: один поток обрабатывает все запросы
        handleUDPClient();
    }
}

// Обрабатывает TCP-клиента
void Server::handleTCPClient(int clientSocket) {
    while (isRunning) {
        vector<char> requestData;
        
        // Получаем данные от клиента
        if (!receiveTCP(clientSocket, requestData)) {
            // Клиент отключился или произошла ошибка
            break;
        }
        
        // Десериализуем запрос
        // requestData содержит: start_node (4 байта) + end_node (4 байта)
        ClientRequest request = bytesToRequest(requestData);
        
        // Теперь нужно получить рёбра графа
        // Для простоты: клиент отправляет сначала количество рёбер, потом сами рёбра
        vector<char> edgesData;
        if (!receiveTCP(clientSocket, edgesData)) {
            break;
        }
        
        // Парсим рёбра: первые 4 байта - количество рёбер
        if (edgesData.size() < sizeof(int)) {
            Logger::error("Некорректные данные о рёбрах");
            break;
        }
        
        int numEdges;
        memcpy(&numEdges, edgesData.data(), sizeof(int));
        
        // Остальные данные - сами рёбра (каждое ребро = 8 байт: from + to)
        vector<vector<int>> edges;
        size_t offset = sizeof(int);
        
        for (int i = 0; i < numEdges && offset + 2 * sizeof(int) <= edgesData.size(); i++) {
            int from, to;
            memcpy(&from, edgesData.data() + offset, sizeof(int));
            offset += sizeof(int);
            memcpy(&to, edgesData.data() + offset, sizeof(int));
            offset += sizeof(int);
            
            edges.push_back({from, to});
        }
        
        // Обрабатываем запрос
        ServerResponse response;
        processRequest(request, edges, response);
        
        // Сериализуем ответ
        vector<char> responseData = responseToBytes(response);
        
        // Отправляем ответ клиенту
        if (!sendTCP(clientSocket, responseData)) {
            break;
        }
    }
    
    // Закрываем соединение с клиентом
    close(clientSocket);
    Logger::info("Клиент отключён");
}

// Обрабатывает UDP-клиентов
void Server::handleUDPClient() {
    while (isRunning) {
        vector<char> allData;
        sockaddr_in clientAddr;
        
        // Получаем датаграмму
        if (!receiveUDP(allData, clientAddr)) {
            continue;
        }
        
        // Минимальный размер: запрос (8 байт) + количество рёбер (4 байта)
        if (allData.size() < 12) {
            Logger::error("Слишком маленький пакет данных");
            continue;
        }
        
        // Первые 8 байт - запрос
        vector<char> requestData(allData.begin(), allData.begin() + 8);
        ClientRequest request = bytesToRequest(requestData);
        
        // Остальное - данные о рёбрах
        vector<char> edgesData(allData.begin() + 8, allData.end());
        
        int numEdges;
        memcpy(&numEdges, edgesData.data(), sizeof(int));
        
        vector<vector<int>> edges;
        size_t offset = sizeof(int);
        
        for (int i = 0; i < numEdges && offset + 2 * sizeof(int) <= edgesData.size(); i++) {
            int from, to;
            memcpy(&from, edgesData.data() + offset, sizeof(int));
            offset += sizeof(int);
            memcpy(&to, edgesData.data() + offset, sizeof(int));
            offset += sizeof(int);
            
            edges.push_back({from, to});
        }
        
        Logger::info("Получен запрос от UDP-клиента");
        
        // Обрабатываем запрос
        ServerResponse response;
        processRequest(request, edges, response);
        
        // Сериализуем ответ
        vector<char> responseData = responseToBytes(response);
        
        // Отправляем ответ
        sendUDP(responseData, clientAddr);
    }
}

// Обрабатывает запрос клиента
void Server::processRequest(const ClientRequest& request, const vector<vector<int>>& edges, ServerResponse& response) {
    // Создаём граф
    Graph graph;
    
    try {
        // Добавляем все рёбра в граф
        graph.addEdges(edges);
        
        // Проверяем размер графа согласно требованиям
        if (!graph.hasMinimumSize()) {
            response.error_code = INVALID_REQUEST;
            response.path_length = 0;
            Logger::warning("Граф не соответствует минимальному размеру");
            return;
        }
        
        if (!graph.hasMaximumSize()) {
            response.error_code = INVALID_REQUEST;
            response.path_length = 0;
            Logger::warning("Граф превышает максимальный размер");
            return;
        }
        
        // Проверяем, что обе вершины существуют в графе
        if (!graph.containsVertices(request.start_node, request.end_node)) {
            response.error_code = INVALID_REQUEST;
            response.path_length = 0;
            Logger::warning("Вершины не найдены в графе");
            return;
        }
        
    } catch (const std::exception& e) {
        response.error_code = INVALID_REQUEST;
        response.path_length = 0;
        Logger::error(string("Ошибка при построении графа: ") + e.what());
        return;
    }
    
    // Запускаем алгоритм Дейкстры
    // Создаём объект Dijkstra с количеством вершин
    int maxNode = 0;
    for (const auto& edge : edges) {
        maxNode = max(maxNode, max(edge[0], edge[1]));
    }
    
    Dijkstra dijkstra(maxNode + 1); // +1 потому что индексы от 0
    
    // Добавляем все рёбра в алгоритм
    for (const auto& edge : edges) {
        dijkstra.addEdge(edge[0], edge[1]);
        dijkstra.addEdge(edge[1], edge[0]); // Неориентированный граф
    }
    
    // Находим путь
    pair<int, vector<int>> result = dijkstra.findPath(request.start_node, request.end_node);
    
    // Проверяем результат
    if (result.first == INF) {
        // Путь не найден
        response.error_code = NO_PATH;
        response.path_length = 0;
        Logger::warning("Путь между вершинами не существует");
    } else {
        // Путь найден
        response.error_code = SUCCESS;
        response.path_length = result.first; // Длина пути
        response.path = result.second;       // Сам путь
        Logger::info("Путь найден, длина: " + to_string(result.first));
    }
}

// Отправляет данные по TCP
bool Server::sendTCP(int socket, const vector<char>& data) {
    // Сначала отправляем размер данных (4 байта)
    uint32_t dataSize = data.size();
    uint32_t networkSize = htonl(dataSize); // Преобразуем в сетевой порядок байт
    
    if (send(socket, &networkSize, sizeof(networkSize), 0) < 0) {
        return false;
    }
    
    // Затем отправляем сами данные
    if (send(socket, data.data(), data.size(), 0) < 0) {
        return false;
    }
    
    return true;
}

// Получает данные по TCP
bool Server::receiveTCP(int socket, vector<char>& data) {
    // Сначала читаем размер данных
    uint32_t networkSize;
    int bytesRead = recv(socket, &networkSize, sizeof(networkSize), 0);
    
    if (bytesRead <= 0) {
        // Соединение закрыто или ошибка
        return false;
    }
    
    uint32_t dataSize = ntohl(networkSize); // Преобразуем из сетевого порядка
    
    // Проверяем разумность размера
    if (dataSize > BUFFER_SIZE) {
        Logger::error("Слишком большой размер данных");
        return false;
    }
    
    // Читаем данные
    char buffer[BUFFER_SIZE];
    bytesRead = recv(socket, buffer, dataSize, 0);
    
    if (bytesRead <= 0) {
        return false;
    }
    
    data.assign(buffer, buffer + bytesRead);
    return true;
}

// Отправляет данные по UDP
bool Server::sendUDP(const vector<char>& data, const sockaddr_in& clientAddr) {
    int bytesSent = sendto(serverSocket, data.data(), data.size(), 0,
                          (sockaddr*)&clientAddr, sizeof(clientAddr));
    return bytesSent > 0;
}

// Получает данные по UDP
bool Server::receiveUDP(vector<char>& data, sockaddr_in& clientAddr) {
    char buffer[BUFFER_SIZE];
    socklen_t addrLen = sizeof(clientAddr);
    
    int bytesRead = recvfrom(serverSocket, buffer, BUFFER_SIZE, 0,
                            (sockaddr*)&clientAddr, &addrLen);
    
    if (bytesRead <= 0) {
        return false;
    }
    
    data.assign(buffer, buffer + bytesRead);
    return true;
}
