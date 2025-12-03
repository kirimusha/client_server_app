#include "Client.h"
#include "Logger.h"
#include "common/UDPProtocol.h"
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <chrono>
#include <thread>

using namespace std;

// Размер буфера для приёма данных
const int BUFFER_SIZE = 4096;
// Таймаут ожидания ACK (3 секунды - требование 2.9.3)
const int UDP_ACK_TIMEOUT_MS = 3000;
// Количество попыток отправки (3 раза - требование 2.9.4)
const int UDP_MAX_ATTEMPTS = 3;

// Конструктор клиента
Client::Client(const string& serverIP, int serverPort, const string& protocol)
    : serverIP(serverIP), serverPort(serverPort), protocol(protocol), 
      clientSocket(-1), connected(false), nextPacketId(1) {
    memset(&serverAddr, 0, sizeof(serverAddr));
}

// Деструктор
Client::~Client() {
    disconnect();
}

// Подключается к серверу
bool Client::connect() {
    if (!createSocket()) {
        return false;
    }
    
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    
    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0) {
        Logger::error("Неверный IP-адрес");
        close(clientSocket);
        return false;
    }
    
    if (protocol == "tcp") {
        if (::connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            Logger::error("Не удалось подключиться к серверу");
            close(clientSocket);
            return false;
        }
    }
    
    connected = true;
    Logger::info("Соединение с сервером установлено (" + protocol + ")");
    return true;
}

// Отключается от сервера
void Client::disconnect() {
    if (clientSocket >= 0) {
        close(clientSocket);
        clientSocket = -1;
    }
    connected = false;
}

// Создаёт сокет
bool Client::createSocket() {
    int socketType = (protocol == "tcp") ? SOCK_STREAM : SOCK_DGRAM;
    
    clientSocket = socket(AF_INET, socketType, 0);
    if (clientSocket < 0) {
        Logger::error("Не удалось создать сокет");
        return false;
    }
    
    if (protocol == "udp") {
        // Устанавливаем таймаут для операций сокета
        struct timeval tv;
        tv.tv_sec = UDP_ACK_TIMEOUT_MS / 1000;
        tv.tv_usec = (UDP_ACK_TIMEOUT_MS % 1000) * 1000;
        
        if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            Logger::warning("Не удалось установить таймаут для UDP");
        }
    }
    
    return true;
}

// Отправляет запрос и получает ответ
bool Client::sendRequest(const ClientRequest& request, 
                         const vector<vector<int>>& edges, 
                         ServerResponse& response) {
    if (!connected) {
        Logger::error("Не подключён к серверу");
        return false;
    }
    
    // Сериализуем данные для отправки
    vector<char> requestData = requestToBytes(request);
    
    // Формируем данные о рёбрах
    vector<char> edgesData;
    int numEdges = edges.size();
    edgesData.resize(sizeof(int));
    memcpy(edgesData.data(), &numEdges, sizeof(int));
    
    for (const auto& edge : edges) {
        if (edge.size() != 2) {
            Logger::error("Некорректное ребро (должно быть 2 вершины)");
            return false;
        }
        
        int from = edge[0];
        int to = edge[1];
        
        size_t oldSize = edgesData.size();
        edgesData.resize(oldSize + 2 * sizeof(int));
        
        memcpy(edgesData.data() + oldSize, &from, sizeof(int));
        memcpy(edgesData.data() + oldSize + sizeof(int), &to, sizeof(int));
    }
    
    // Отправляем данные с подтверждением
    vector<char> responseData;
    
    if (protocol == "tcp") {
        // TCP: отправляем ДВА отдельных сообщения
        Logger::info("Отправка TCP запроса (2 сообщения)...");
        
        // 1. Отправляем requestData
        if (!sendTCP(requestData)) {
            Logger::error("Не удалось отправить requestData по TCP");
            return false;
        }
        Logger::info("requestData отправлен (" + to_string(requestData.size()) + " байт)");
        
        // 2. Отправляем edgesData
        if (!sendTCP(edgesData)) {
            Logger::error("Не удалось отправить edgesData по TCP");
            return false;
        }
        Logger::info("edgesData отправлен (" + to_string(edgesData.size()) + " байт)");
        
        // 3. Получаем ответ
        if (!receiveTCP(responseData)) {
            Logger::error("Не удалось получить ответ по TCP");
            return false;
        }
        Logger::info("Ответ получен (" + to_string(responseData.size()) + " байт)");
        
    } else {
        // UDP: отправляем всё вместе в одном пакете
        // Объединяем данные
        vector<char> payload = requestData;
        payload.insert(payload.end(), edgesData.begin(), edgesData.end());
        
        Logger::info("=== Начало UDP обмена ===");
        Logger::info("Размер полезной нагрузки: " + to_string(payload.size()) + " байт");
        
        if (!sendWithAck(payload)) {
            return false;
        }
        
        Logger::info("Запрос подтверждён сервером, ожидаем ответ...");
        
        if (!receiveResponse(responseData)) {
            Logger::error("Не удалось получить ответ от сервера");
            return false;
        }
        
        Logger::info("=== UDP обмен завершён успешно ===");
    }
    
    // Десериализуем ответ
    response = bytesToResponse(responseData);
    return true;
}

// Отправляет данные с подтверждением (надежная UDP-доставка)
bool Client::sendWithAck(const vector<char>& payload) {
    uint32_t packet_id = getNextPacketId();
    vector<char> dataPacket = UDPProtocol::createDataPacket(packet_id, payload);
    
    // Пытаемся отправить до 3 раз (требование 2.9.4)
    for (int attempt = 0; attempt < UDP_MAX_ATTEMPTS; attempt++) {
        // 1. Отправляем данные
        if (!sendUDP(dataPacket)) {
            Logger::warning("Не удалось отправить пакет (попытка " + 
                           to_string(attempt + 1) + ")");
            continue;
        }
        
        // 2. Ждём подтверждение (ACK)
        if (waitForAck(packet_id)) {
            Logger::info("Пакет подтверждён сервером");
            return true; // Успех!
        }
        
        // 3. ACK не пришёл - логируем и повторяем
        if (attempt < UDP_MAX_ATTEMPTS - 1) {
            Logger::warning("Подтверждение не получено, повторная отправка...");
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }
    
    // Все попытки исчерпаны
    Logger::error("Потеряна связь с сервером");
    return false;
}

// Ожидает подтверждение (ACK) от сервера
bool Client::waitForAck(uint32_t expected_packet_id) {
    char buffer[BUFFER_SIZE];
    sockaddr_in fromAddr;
    socklen_t fromLen = sizeof(fromAddr);
    
    // Пытаемся получить ACK несколько раз
    for (int attempt = 0; attempt < UDP_MAX_ATTEMPTS; attempt++) {
        int bytesRead = recvfrom(clientSocket, buffer, sizeof(buffer), 0,
                                (sockaddr*)&fromAddr, &fromLen);
        
        if (bytesRead > 0) {
            auto [header, payload] = UDPProtocol::parsePacket(
                vector<char>(buffer, buffer + bytesRead)
            );
            
            Logger::info("Получен пакет: тип=" + to_string(header.type) + 
                        ", ID=" + to_string(header.packet_id));
            
            if (header.type == PACKET_ACK && header.packet_id == expected_packet_id) {
                Logger::info("Получен ACK для пакета " + to_string(expected_packet_id));
                return true;
            } else if (header.type == PACKET_DATA) {
                // Это не ACK, а уже ответ! Сохраняем его для последующего использования
                Logger::warning("Получен DATA вместо ACK - сервер отправил ответ слишком быстро");
                // TODO: Нужно сохранить этот пакет, чтобы не потерять!
            }
        } else {
            Logger::warning("Таймаут ожидания ACK (попытка " + to_string(attempt + 1) + ")");
        }
        
        if (attempt < UDP_MAX_ATTEMPTS - 1) {
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }
    
    return false;
}

// Получает ответ от сервера
bool Client::receiveResponse(vector<char>& responseData) {
    char buffer[BUFFER_SIZE];
    sockaddr_in fromAddr;
    socklen_t fromLen = sizeof(fromAddr);
    
    Logger::info("Ожидание ответа от сервера (до 3 попыток)...");
    
    for (int attempt = 0; attempt < UDP_MAX_ATTEMPTS; attempt++) {
        Logger::info("Попытка " + to_string(attempt + 1) + " получить ответ...");
        
        int bytesRead = recvfrom(clientSocket, buffer, sizeof(buffer), 0,
                                (sockaddr*)&fromAddr, &fromLen);
        
        if (bytesRead > 0) {
            Logger::info("Получено " + to_string(bytesRead) + " байт");
            
            auto [header, payload] = UDPProtocol::parsePacket(
                vector<char>(buffer, buffer + bytesRead)
            );
            
            Logger::info("Тип пакета: " + to_string(header.type) + ", ID: " + to_string(header.packet_id));
            
            if (header.type == PACKET_DATA) {
                Logger::info("Получен пакет с данными ответа");
                responseData = payload;
                return true;
            } else if (header.type == PACKET_ACK) {
                Logger::warning("Получен ACK вместо данных, игнорируем...");
            }
        } else {
            Logger::warning("Таймаут при получении данных (попытка " + to_string(attempt + 1) + ")");
        }
        
        if (attempt < UDP_MAX_ATTEMPTS - 1) {
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }
    
    Logger::error("Все попытки получения ответа исчерпаны");
    return false;
}

// Получает следующий ID пакета
uint32_t Client::getNextPacketId() {
    return nextPacketId.fetch_add(1);
}

// Проверяет состояние подключения
bool Client::isConnected() const {
    return connected;
}

// Отправляет данные по TCP
bool Client::sendTCP(const vector<char>& data) {
    uint32_t dataSize = data.size();
    uint32_t networkSize = htonl(dataSize);
    
    if (send(clientSocket, &networkSize, sizeof(networkSize), 0) < 0) {
        return false;
    }
    
    if (send(clientSocket, data.data(), data.size(), 0) < 0) {
        return false;
    }
    
    return true;
}

// Получает данные по TCP
bool Client::receiveTCP(vector<char>& data) {
    uint32_t networkSize;
    int bytesRead = recv(clientSocket, &networkSize, sizeof(networkSize), 0);
    
    if (bytesRead <= 0) {
        return false;
    }
    
    uint32_t dataSize = ntohl(networkSize);
    
    if (dataSize > BUFFER_SIZE) {
        Logger::error("Слишком большой размер данных");
        return false;
    }
    
    char buffer[BUFFER_SIZE];
    bytesRead = recv(clientSocket, buffer, dataSize, 0);
    
    if (bytesRead <= 0) {
        return false;
    }
    
    data.assign(buffer, buffer + bytesRead);
    return true;
}

// Отправляет данные по UDP
bool Client::sendUDP(const vector<char>& data) {
    int bytesSent = sendto(clientSocket, data.data(), data.size(), 0,
                          (sockaddr*)&serverAddr, sizeof(serverAddr));
    return bytesSent > 0;
}