#include "Server.h"
#include "Logger.h"
#include "Validator.h"
#include "common/UDPProtocol.h"
#include "common/Dijkstra.h"
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <chrono>
#include <thread>
#include <fcntl.h>     // For fcntl(), F_GETFL, F_SETFL, O_NONBLOCK
#include <unistd.h>    // For close() - if you're using close() for sockets

using namespace std;

// Размер буфера для приёма данных
const int BUFFER_SIZE = 4096;
// Таймаут для потери связи с клиентом (секунды)
const int CLIENT_TIMEOUT_SEC = 10;

// Конструктор сервера
Server::Server(int port, const string& protocol)
    : port(port), protocol(protocol), serverSocket(-1), isRunning(false),
      nextPacketId(1) {
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
    Logger::info("Сервер запущен на порту " + to_string(port) + " (" + protocol + ")");
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
    
    // Очищаем информацию о клиентах
    {
        lock_guard<mutex> lock(clientsMutex);
        activeClients.clear();
    }
}

// Создаёт и настраивает сокет
bool Server::createSocket() {
    // Определяем тип сокета в зависимости от протокола
    int socketType = (protocol == "tcp") ? SOCK_STREAM : SOCK_DGRAM;
    
    // Создаём сокет
    serverSocket = socket(AF_INET, socketType, 0);
    if (serverSocket < 0) {
        Logger::error("Не удалось создать сокет");
        return false;
    }
    
    // Устанавливаем опцию SO_REUSEADDR
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        Logger::warning("Не удалось установить SO_REUSEADDR");
    }
    
    // Настраиваем адрес сервера
    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    
    // Привязываем сокет к адресу
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        Logger::error("Не удалось привязать сокет к порту " + to_string(port));
        close(serverSocket);
        return false;
    }
    
    // Для TCP нужно начать слушать входящие подключения
    if (protocol == "tcp") {
        if (listen(serverSocket, 5) < 0) {
            Logger::error("Не удалось начать прослушивание");
            close(serverSocket);
            return false;
        }
    } else {
        // Для UDP устанавливаем неблокирующий режим (опционально)
        // Это позволит лучше контролировать цикл обработки
        int flags = fcntl(serverSocket, F_GETFL, 0);
        fcntl(serverSocket, F_SETFL, flags | O_NONBLOCK);
    }
    
    return true;
}

// Главный цикл сервера
void Server::run() {
    if (protocol == "tcp") {
        runTCP();
    } else {
        runUDP();
    }
}

// Работа с TCP-клиентами
void Server::runTCP() {
    while (isRunning) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);
        
        if (clientSocket < 0) {
            if (isRunning) {
                Logger::error("Ошибка при принятии подключения");
            }
            continue;
        }
        
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        Logger::info("Подключён TCP-клиент: " + string(clientIP));
        
        // Создаём новый поток для обработки клиента
        clientThreads.emplace_back(&Server::handleTCPClient, this, clientSocket);
    }
}

// Работа с UDP-клиентами
void Server::runUDP() {
    Logger::info("UDP-сервер ожидает запросы...");
    
    while (isRunning) {
        vector<char> packetData;
        sockaddr_in clientAddr;
        
        // Получаем пакет от клиента
        if (!receiveUDPPacket(packetData, clientAddr)) {
            this_thread::sleep_for(chrono::milliseconds(10));
            continue;
        }
        
        // Парсим пакет
        auto [header, payload] = UDPProtocol::parsePacket(packetData);
        
        // Обновляем время последней активности клиента
        updateClientActivity(clientAddr);
        
        // Обрабатываем пакет в зависимости от типа
        if (header.type == PACKET_DATA) {
            handleUDPDataPacket(header, payload, clientAddr);
        } else if (header.type == PACKET_ACK) {
            // Для сервера ACK не требуется (требование 2.9.1)
            // Клиенты не подтверждают получение ACK
        }
        
        // Проверяем таймауты клиентов
        checkClientTimeouts();
    }
}

// Обновляет информацию об активности клиента
void Server::updateClientActivity(const sockaddr_in& clientAddr) {
    lock_guard<mutex> lock(clientsMutex);
    
    string clientKey = getClientKey(clientAddr);
    activeClients[clientKey] = chrono::steady_clock::now();
}

// Проверяет таймауты клиентов
void Server::checkClientTimeouts() {
    lock_guard<mutex> lock(clientsMutex);
    auto now = chrono::steady_clock::now();
    
    for (auto it = activeClients.begin(); it != activeClients.end(); ) {
        auto duration = chrono::duration_cast<chrono::seconds>(now - it->second).count();
        
        if (duration > CLIENT_TIMEOUT_SEC) {
            Logger::warning("Потеряна связь с клиентом " + it->first);
            it = activeClients.erase(it);
        } else {
            ++it;
        }
    }
}

// Получает ключ клиента из адреса
string Server::getClientKey(const sockaddr_in& clientAddr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, ip, INET_ADDRSTRLEN);
    return string(ip) + ":" + to_string(ntohs(clientAddr.sin_port));
}

// Обрабатывает UDP-пакет с данными
void Server::handleUDPDataPacket(const UDPPacketHeader& header, 
                                 const vector<char>& payload, 
                                 const sockaddr_in& clientAddr) {
    // 1. Немедленно отправляем ACK (требование 2.9.1)
    sendAck(header.packet_id, clientAddr);
    
    // 2. Обрабатываем полезную нагрузку
    processUDPRequest(payload, clientAddr);
}

// Отправляет ACK-пакет
bool Server::sendAck(uint32_t packet_id, const sockaddr_in& clientAddr) {
    vector<char> ackPacket = UDPProtocol::createAckPacket(packet_id);
    return sendUDP(ackPacket, clientAddr);
}

// Обрабатывает UDP-запрос
void Server::processUDPRequest(const vector<char>& payload, const sockaddr_in& clientAddr) {
    try {
        // Минимальный размер: запрос (8 байт) + количество рёбер (4 байта)
        if (payload.size() < 12) {
            Logger::error("Слишком маленький пакет данных");
            return;
        }
        
        // Первые 8 байт - запрос
        vector<char> requestData(payload.begin(), payload.begin() + 8);
        ClientRequest request = bytesToRequest(requestData);
        
        // Остальное - данные о рёбрах
        vector<char> edgesData(payload.begin() + 8, payload.end());
        
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
        
        Logger::info("Получен UDP-запрос от " + getClientKey(clientAddr));
        
        // Обрабатываем запрос
        ServerResponse response;
        processRequest(request, edges, response);
        
        // Сериализуем ответ
        vector<char> responseData = responseToBytes(response);
        
        // Создаём пакет с данными
        uint32_t packet_id = getNextPacketId();
        vector<char> dataPacket = UDPProtocol::createDataPacket(packet_id, responseData);
        
        // Даём клиенту время перейти из waitForAck() в receiveResponse()
        this_thread::sleep_for(chrono::milliseconds(50));
        
        // Отправляем ответ (без ожидания подтверждения для сервера)
        if (sendUDP(dataPacket, clientAddr)) {
            Logger::info("Ответ отправлен клиенту " + getClientKey(clientAddr));
        } else {
            Logger::error("Не удалось отправить ответ клиенту");
        }
        
    } catch (const exception& e) {
        Logger::error("Ошибка обработки UDP-запроса: " + string(e.what()));
    }
}

// Получает следующий ID пакета
uint32_t Server::getNextPacketId() {
    return nextPacketId.fetch_add(1);
}

// Обрабатывает TCP-клиента
void Server::handleTCPClient(int clientSocket) {
    while (isRunning) {
        vector<char> requestData;
        
        if (!receiveTCP(clientSocket, requestData)) {
            break;
        }
        
        ClientRequest request = bytesToRequest(requestData);
        
        vector<char> edgesData;
        if (!receiveTCP(clientSocket, edgesData)) {
            break;
        }
        
        if (edgesData.size() < sizeof(int)) {
            Logger::error("Некорректные данные о рёбрах");
            break;
        }
        
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
        
        ServerResponse response;
        processRequest(request, edges, response);
        
        vector<char> responseData = responseToBytes(response);
        
        if (!sendTCP(clientSocket, responseData)) {
            break;
        }
    }
    
    close(clientSocket);
    Logger::info("TCP-клиент отключён");
}

// Получает UDP-пакет
bool Server::receiveUDPPacket(vector<char>& data, sockaddr_in& clientAddr) {
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

// Отправляет данные по UDP
bool Server::sendUDP(const vector<char>& data, const sockaddr_in& clientAddr) {
    int bytesSent = sendto(serverSocket, data.data(), data.size(), 0,
                          (sockaddr*)&clientAddr, sizeof(clientAddr));
    return bytesSent > 0;
}

// Отправляет данные по TCP
bool Server::sendTCP(int socket, const vector<char>& data) {
    uint32_t dataSize = data.size();
    uint32_t networkSize = htonl(dataSize);
    
    if (send(socket, &networkSize, sizeof(networkSize), 0) < 0) {
        return false;
    }
    
    if (send(socket, data.data(), data.size(), 0) < 0) {
        return false;
    }
    
    return true;
}

// Получает данные по TCP
bool Server::receiveTCP(int socket, vector<char>& data) {
    uint32_t networkSize;
    int bytesRead = recv(socket, &networkSize, sizeof(networkSize), 0);
    
    if (bytesRead <= 0) {
        return false;
    }
    
    uint32_t dataSize = ntohl(networkSize);
    
    if (dataSize > BUFFER_SIZE) {
        Logger::error("Слишком большой размер данных");
        return false;
    }
    
    char buffer[BUFFER_SIZE];
    bytesRead = recv(socket, buffer, dataSize, 0);
    
    if (bytesRead <= 0) {
        return false;
    }
    
    data.assign(buffer, buffer + bytesRead);
    return true;
}

// Обрабатывает запрос клиента
void Server::processRequest(const ClientRequest& request, 
                           const vector<vector<int>>& edges, 
                           ServerResponse& response) {
    Graph graph;
    
    try {
        graph.addEdges(edges);
        
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
    
    int maxNode = 0;
    for (const auto& edge : edges) {
        maxNode = max(maxNode, max(edge[0], edge[1]));
    }
    
    Dijkstra dijkstra(maxNode + 1);
    
    for (const auto& edge : edges) {
        dijkstra.addEdge(edge[0], edge[1]);
        dijkstra.addEdge(edge[1], edge[0]);
    }
    
    pair<int, vector<int>> result = dijkstra.findPath(request.start_node, request.end_node);
    
    if (result.first == INF) {
        response.error_code = NO_PATH;
        response.path_length = 0;
        Logger::warning("Путь между вершинами не существует");
    } else {
        response.error_code = SUCCESS;
        response.path_length = result.first;
        response.path = result.second;
        Logger::info("Путь найден, длина: " + to_string(result.first));
    }
}