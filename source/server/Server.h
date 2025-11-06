#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>

// Подключаем классы из проекта
#include "common/Graph.h"
#include "common/Protocol.h"

using namespace std;

// Класс сервера для обработки запросов клиентов

// Поддерживает работу по протоколам TCP и UDP
// Может обслуживать несколько клиентов одновременно (минимум 3)

class Server {
public:
    // Конструктор сервера
    // port Порт для прослушивания (1024-65535)
    // protocol Тип протокола ("tcp" или "udp")
    
    // Пример:
    // Server server(8080, "tcp");
    Server(int port, const string& protocol);

    // Деструктор - закрывает сокет и освобождает ресурсы
    ~Server();

    // Запускает сервер
    // true, если сервер успешно запущен
    
    // Создаёт сокет, привязывает его к порту и начинает слушать подключения
    bool start();

    // Останавливает сервер
    // Закрывает все соединения и завершает работу потоков
    void stop();

    // Главный цикл обработки запросов
    // Принимает подключения клиентов и создаёт для них отдельные потоки
    void run();

private:
    int port;                    // Порт сервера
    string protocol;        // Тип протокола (tcp/udp)
    int serverSocket;            // Дескриптор серверного сокета
    atomic<bool> isRunning; // Флаг работы сервера (атомарный для многопоточности)
    
    // Вектор потоков для обработки клиентов
    vector<thread> clientThreads;

    // Создаёт и настраивает серверный сокет
    // true, если сокет успешно создан
    bool createSocket();

    
    // Обрабатывает одного клиента (TCP)
    // clientSocket Дескриптор сокета клиента
 
    // Эта функция запускается в отдельном потоке для каждого клиента
    // Читает запросы, обрабатывает их и отправляет ответы
    void handleTCPClient(int clientSocket);

    // Обрабатывает UDP-клиента

    // UDP не имеет постоянного соединения, поэтому обработка отличается от TCP
    // Читает датаграммы, обрабатывает и отправляет ответы
    void handleUDPClient();

    // Обрабатывает запрос на поиск пути в графе
    // request Запрос от клиента
    // response Ответ для клиента

    // Парсит граф из запроса, выполняет алгоритм Дейкстры,
    // формирует ответ с результатом или ошибкой
    void processRequest(const ClientRequest& request, const vector<vector<int>>& edges, ServerResponse& response);

    // Отправляет данные по TCP
    // socket Сокет клиента
    // data Данные для отправки
    // true, если отправка успешна
    bool sendTCP(int socket, const string& data);

    // Получает данные по TCP
    // socket Сокет клиента
    // data Буфер для полученных данных
    // true, если получение успешно
    bool receiveTCP(int socket, string& data);

    // Отправляет данные по UDP
    // data Данные для отправки
    // clientAddr Адрес клиента
    // true, если отправка успешна
    bool sendUDP(const string& data, const sockaddr_in& clientAddr);

    // Получает данные по UDP
    // data Буфер для полученных данных
    // clientAddr Адрес клиента (выходной параметр)
    // true, если получение успешно
    bool receiveUDP(string& data, sockaddr_in& clientAddr);
};

#endif