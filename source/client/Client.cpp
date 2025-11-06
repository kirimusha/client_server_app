#include "Client.h"
#include "Logger.h"
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

// Размер буфера для приёма данных
const int BUFFER_SIZE = 4096;
// Таймаут для UDP в секундах
const int UDP_TIMEOUT_SEC = 2;
// Количество попыток для UDP
const int UDP_MAX_RETRIES = 3;

// Конструктор клиента
Client::Client(const string& serverIP, int serverPort, const string& protocol)
    : serverIP(serverIP), serverPort(serverPort), protocol(protocol), 
      clientSocket(-1), connected(false) {
    // Инициализируем структуру адреса сервера
    memset(&serverAddr, 0, sizeof(serverAddr));
}

// Деструктор
Client::~Client() {
    disconnect();
}

// Подключается к серверу
bool Client::connect() {
    // Создаём сокет
    if (!createSocket()) {
        return false;
    }
    
    // Настраиваем адрес сервера
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    
    // Преобразуем IP-адрес из строки в бинарный формат
    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0) {
        Logger::error("Неверный IP-адрес");
        close(clientSocket);
        return false;
    }
    
    // Для TCP устанавливаем соединение
    if (protocol == "tcp") {
        if (::connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            Logger::error("Не удалось подключиться к серверу");
            close(clientSocket);
            return false;
        }
    }
    
    connected = true;
    Logger::info("Соединение с сервером установлено");
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
    // Определяем тип сокета
    int socketType = (protocol == "tcp") ? SOCK_STREAM : SOCK_DGRAM;
    
    // Создаём сокет
    clientSocket = socket(AF_INET, socketType, 0);
    if (clientSocket < 0) {
        Logger::error("Не удалось создать сокет");
        return false;
    }
    
    // Для UDP устанавливаем таймаут
    if (protocol == "udp") {
        struct timeval tv;
        tv.tv_sec = UDP_TIMEOUT_SEC;  // Секунды
        tv.tv_usec = 0;               // Микросекунды
        
        // SO_RCVTIMEO - таймаут для операции чтения
        if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            Logger::warning("Не удалось установить таймаут для UDP");
        }
    }
    
    return true;
}

// Отправляет запрос и получает ответ
bool Client::sendRequest(const ClientRequest& request, const vector<vector<int>>& edges, ServerResponse& response) {
    if (!connected) {
        Logger::error("Не подключён к серверу");
        return false;
    }
    
    // Сериализуем запрос (start_node + end_node = 8 байт)
    vector<char> requestData = requestToBytes(request);
    
    // Формируем данные о рёбрах
    vector<char> edgesData;
    
    // Первые 4 байта - количество рёбер
    int numEdges = edges.size();
    edgesData.resize(sizeof(int));
    memcpy(edgesData.data(), &numEdges, sizeof(int));
    
    // Добавляем сами рёбра (каждое ребро = 8 байт: from + to)
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
    
    // Отправляем данные
    bool sent = false;
    
    if (protocol == "tcp") {
        // TCP: отправляем запрос и рёбра отдельно
        sent = sendTCP(requestData) && sendTCP(edgesData);
    } else {
        // UDP: объединяем всё в один пакет
        vector<char> allData = requestData;
        allData.insert(allData.end(), edgesData.begin(), edgesData.end());
        sent = sendUDP(allData);
    }
    
    if (!sent) {
        Logger::error("Не удалось отправить запрос");
        return false;
    }
    
    // Получаем ответ
    vector<char> responseData;
    bool received = false;
    
    if (protocol == "tcp") {
        received = receiveTCP(responseData);
    } else {
        received = receiveUDP(responseData);
    }
    
    if (!received) {
        Logger::error("Потеряна связь с сервером");
        return false;
    }
    
    // Десериализуем ответ
    response = bytesToResponse(responseData);
    
    return true;
}

// Проверяет состояние подключения
bool Client::isConnected() const {
    return connected;
}

// Отправляет данные по TCP
bool Client::sendTCP(const string& data) {
    // Сначала отправляем размер данных
    uint32_t dataSize = data.size();
    uint32_t networkSize = htonl(dataSize);
    
    if (send(clientSocket, &networkSize, sizeof(networkSize), 0) < 0) {
        return false;
    }
    
    // Затем отправляем сами данные
    if (send(clientSocket, data.c_str(), data.size(), 0) < 0) {
        return false;
    }
    
    return true;
}

// Получает данные по TCP
bool Client::receiveTCP(string& data) {
    // Читаем размер данных
    uint32_t networkSize;
    int bytesRead = recv(clientSocket, &networkSize, sizeof(networkSize), 0);
    
    if (bytesRead <= 0) {
        return false;
    }
    
    uint32_t dataSize = ntohl(networkSize);
    
    // Проверяем разумность размера
    if (dataSize > BUFFER_SIZE) {
        Logger::error("Слишком большой размер данных");
        return false;
    }
    
    // Читаем данные
    char buffer[BUFFER_SIZE];
    bytesRead = recv(clientSocket, buffer, dataSize, 0);
    
    if (bytesRead <= 0) {
        return false;
    }
    
    data.assign(buffer, bytesRead);
    return true;
}

// Отправляет данные по UDP
bool Client::sendUDP(const string& data) {
    int bytesSent = sendto(clientSocket, data.c_str(), data.size(), 0,
                          (sockaddr*)&serverAddr, sizeof(serverAddr));
    return bytesSent > 0;
}

// Получает данные по UDP с повторными попытками
// UDP не гарантирует доставку, поэтому делаем несколько попыток
bool Client::receiveUDP(string& data) {
    char buffer[BUFFER_SIZE];
    
    // Делаем до 3 попыток получить ответ
    for (int attempt = 0; attempt < UDP_MAX_RETRIES; attempt++) {
        sockaddr_in fromAddr;
        socklen_t fromLen = sizeof(fromAddr);
        
        // recvfrom блокируется на время таймаута (2 секунды)
        int bytesRead = recvfrom(clientSocket, buffer, BUFFER_SIZE, 0,
                                (sockaddr*)&fromAddr, &fromLen);
        
        if (bytesRead > 0) {
            // Успешно получили данные
            data.assign(buffer, bytesRead);
            return true;
        }
        
        // Если это не последняя попытка, выводим предупреждение
        if (attempt < UDP_MAX_RETRIES - 1) {
            Logger::warning("Попытка " + to_string(attempt + 1) + 
                          " не удалась, повторяем...");
        }
    }
    
    // Все попытки исчерпаны
    Logger::error("Потеряна связь с сервером (после " + 
                 to_string(UDP_MAX_RETRIES) + " неудачных попыток)");
    return false;
}