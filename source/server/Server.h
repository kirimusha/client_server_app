#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdint>

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
    string protocol;             // Тип протокола (tcp/udp)
    int serverSocket;            // Дескриптор серверного сокета
    atomic<bool> isRunning;      // Флаг работы сервера (атомарный для многопоточности)
    
    // Вектор потоков для обработки клиентов
    vector<thread> clientThreads;
    
    // Для надёжной UDP-доставки - атомарный счётчик идентификаторов пакетов
    // 
    // Назначение:
    //   - Генерирует уникальные последовательные номера для каждого UDP-пакета
    //   - Обеспечивает идентификацию пакетов при механизме подтверждения доставки (ACK)
    //   - Позволяет сопоставлять отправленные пакеты с полученными подтверждениями
    // 
    // Почему atomic (атомарный тип):
    //   - Многопоточная среда: сервер обрабатывает несколько клиентов параллельно
    //   - Безопасность данных: исключает race conditions (состояния гонки)
    //   - Гарантирует уникальность: даже при одновременных запросах от разных клиентов
    //   - Не требует мьютексов: операции инкремента и чтения выполняются атомарно
    // 
    // Почему uint32_t:
    //   - Диапазон значений: 0..4,294,967,295 (более 4 миллиардов пакетов)
    //   - Достаточно для длительной работы без переполнения
    //   - При переполнении счётчик вернётся к 0 (циклическое поведение)
    //   - Стандартный размер для сетевых протоколов
    // 
    // Механизм работы:
    //   1. Клиент при отправке запроса:
    //      - nextPacketId++ → получает ID (например, 42)
    //      - Отправляет пакет с этим ID
    //      - Ожидает ACK с ID=42
    //   
    //   2. Сервер при получении:
    //      - Принимает пакет с ID=42
    //      - Немедленно отправляет ACK с тем же ID=42
    //      - Для ответа генерирует свой ID (например, 57)
    //   
    //   3. Клиент при получении ответа:
    //      - Получает пакет с ID=57
    //      - Не отправляет ACK (согласно требованию 2.9.1)
    // 
    // Без этого механизма:
    //   - Невозможно реализовать надёжную доставку
    //   - Нельзя отличить подтверждения для разных пакетов
    //   - Возможны ложные подтверждения (дубликаты, старые ACK)
    // 
    // Пример использования в коде:
    //   // Серверная часть
    //   uint32_t packet_id = nextPacketId.fetch_add(1); // Атомарный инкремент
    //   vector<char> ack_packet = createAckPacket(packet_id);
    //   
    //   // Клиентская часть  
    //   uint32_t request_id = nextPacketId.fetch_add(1);
    //   if (waitForAck(request_id)) { // Ожидание подтверждения
    //       // Пакет доставлен успешно
    //   }
    // 
    // Альтернативы и их недостатки:
    //   - Обычный int: небезопасно в многопоточной среде
    //   - Генератор случайных чисел: сложно сопоставлять ACK
    //   - Временные метки: возможны коллизии
    atomic<uint32_t> nextPacketId;
    
    // Для отслеживания активности клиентов
    mutex clientsMutex;
    unordered_map<string, chrono::steady_clock::time_point> activeClients;

    // Создаёт и настраивает серверный сокет
    // true, если сокет успешно создан
    bool createSocket();
    
    // Запускает работу с TCP-клиентами
    void runTCP();
    
    // Запускает работу с UDP-клиентами
    void runUDP();

    // Обрабатывает одного клиента (TCP)
    // clientSocket Дескриптор сокета клиента
 
    // Эта функция запускается в отдельном потоке для каждого клиента
    // Читает запросы, обрабатывает их и отправляет ответы
    void handleTCPClient(int clientSocket);
    
    // Обрабатывает UDP-пакет с данными
    // header Заголовок пакета
    // payload Полезная нагрузка
    // clientAddr Адрес клиента
    void handleUDPDataPacket(const struct UDPPacketHeader& header, 
                            const vector<char>& payload, 
                            const sockaddr_in& clientAddr);
    
    // Обрабатывает UDP-запрос
    // payload Данные запроса
    // clientAddr Адрес клиента
    void processUDPRequest(const vector<char>& payload, const sockaddr_in& clientAddr);
    
    // Отправляет ACK-пакет
    // packet_id ID подтверждаемого пакета
    // clientAddr Адрес клиента
    bool sendAck(uint32_t packet_id, const sockaddr_in& clientAddr);
    
    // Обновляет информацию об активности клиента
    // clientAddr Адрес клиента
    void updateClientActivity(const sockaddr_in& clientAddr);
    
    // Проверяет таймауты клиентов
    void checkClientTimeouts();
    
    // Получает ключ клиента из адреса
    // clientAddr Адрес клиента
    string getClientKey(const sockaddr_in& clientAddr);
    
    // Получает следующий ID пакета
    uint32_t getNextPacketId();

    // Обрабатывает запрос на поиск пути в графе
    // request Запрос от клиента
    // response Ответ для клиента

    // Парсит граф из запроса, выполняет алгоритм Дейкстры,
    // формирует ответ с результатом или ошибкой
    void processRequest(const ClientRequest& request, const vector<vector<int>>& edges, ServerResponse& response);

    // Отправляет данные по TCP
    // socket Сокет клиента
    // data Данные для отправки
    bool sendTCP(int socket, const vector<char>& data);

    // Получает данные по TCP
    // socket Сокет клиента
    // data Буфер для полученных данных
    bool receiveTCP(int socket, vector<char>& data);

    // Отправляет данные по UDP
    // data Данные для отправки
    // clientAddr Адрес клиента
    bool sendUDP(const vector<char>& data, const sockaddr_in& clientAddr);

    // Получает данные по UDP
    // data Буфер для полученных данных
    // clientAddr Адрес клиента (выходной параметр)
    bool receiveUDPPacket(vector<char>& data, sockaddr_in& clientAddr);
};

#endif