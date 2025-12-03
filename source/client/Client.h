#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <vector>
#include <atomic>
#include <netinet/in.h>
#include "Protocol.h"

using namespace std;

// Класс клиента для связи с сервером

// Поддерживает работу по протоколам TCP и UDP
// Отправляет запросы на поиск кратчайшего пути в графе
class Client {
public:
    // Конструктор клиента
    // serverIP IP-адрес сервера (например, "127.0.0.1")
    // serverPort Порт сервера (1024-65535)
    // protocol Тип протокола ("tcp" или "udp")
    Client(const string& serverIP, int serverPort, const string& protocol);

    // Деструктор - закрывает соединение
    ~Client();

    // Подключается к серверу
    // true, если подключение успешно

    // Для TCP устанавливает соединение
    // Для UDP просто создаёт сокет (UDP не требует установки соединения)
    bool connect();

    // Отключается от сервера

    // Закрывает сокет и освобождает ресурсы
    void disconnect();

    // Отправляет запрос на сервер и получает ответ
    // request Запрос с данными о графе
    // response Ответ от сервера (выходной параметр)
    // true, если запрос успешно обработан

    // Пример использования:
    // Protocol::Request request;
    // request.edges = {{"A", "B"}, {"B", "C"}};
    // request.startVertex = "A";
    // request.endVertex = "C";

    // Protocol::Response response;
    // if (client.sendRequest(request, response)) {
    //     if (response.success) {
    //         cout << "Длина пути: " << response.pathLength << endl;
    //     }
    // }
    bool sendRequest(const ClientRequest& request, const vector<vector<int>>& edges, ServerResponse& response);

    // Проверяет, установлено ли соединение
    // true, если клиент подключён к серверу
    bool isConnected() const;

private:
    string serverIP;             // IP-адрес сервера
    int serverPort;              // Порт сервера
    string protocol;             // Тип протокола (tcp/udp)
    int clientSocket;            // Дескриптор сокета
    sockaddr_in serverAddr;      // Адрес сервера
    bool connected;              // Флаг состояния подключения
    
    // Для надёжной UDP-доставки
    atomic<uint32_t> nextPacketId;

    // Создаёт сокет клиента
    // true, если сокет успешно создан
    bool createSocket();
    
    // Отправляет данные с подтверждением (надёжная UDP-доставка)
    // payload Данные для отправки
    bool sendWithAck(const vector<char>& payload);
    
    // Ожидает подтверждение (ACK) от сервера
    // expected_packet_id Ожидаемый ID пакета
    bool waitForAck(uint32_t expected_packet_id);
    
    // Получает ответ от сервера
    // responseData Буфер для полученных данных
    bool receiveResponse(vector<char>& responseData);
    
    // Получает следующий ID пакета
    uint32_t getNextPacketId();

    // Отправляет запрос по TCP
    // data Сериализованные данные запроса
    // true, если отправка успешна
    bool sendTCP(const vector<char>& data);

    // Получает ответ по TCP
    // data Буфер для полученных данных
    // true, если получение успешно
    bool receiveTCP(vector<char>& data);

    // Отправляет запрос по UDP
    // data Сериализованные данные запроса
    // true, если отправка успешна
    bool sendUDP(const vector<char>& data);
};

#endif