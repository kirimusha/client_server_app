#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <vector>

using namespace std;

// Типы сообщений
enum MessageType {
    CLIENT_REQUEST = 1,    // Запрос от клиента
    SERVER_RESPONSE = 2,   // Ответ от сервера
    ERROR_RESPONSE = 3     // Ошибка
};

// Коды ошибок
enum ErrorCode {
    SUCCESS = 0,           // Успех
    INVALID_REQUEST = 1,   // Неправильный запрос
    NO_PATH = 2           // Путь не найден
};

// Структура для ребра графа
struct Edge {
    int from;
    int to;
    // double weight;
};

// Запрос от клиента
struct ClientRequest { // Клиент будет отправлять это
    int start_node;
    int end_node;
};

// Ответ от сервера
struct ServerResponse { // Сервер будет отвечать этим
    int error_code;
    int path_length;
    vector<int> path;  // Список узлов пути
};

// Функции для работы с данными: сериализация и десериализация

// Преобразование запросов:

// Клиент - Сервер: преобразуем запрос в байты для отправки
vector<char> requestToBytes(const ClientRequest& request);

// Сервер - Клиент: преобразуем байты обратно в запрос
ClientRequest bytesToRequest(const vector<char>& data);

// Преобразование ответов:

// Сервер - Клиент: преобразуем ответ в байты для отправки
vector<char> responseToBytes(const ServerResponse& response);

// Клиент - Сервер: преобразуем байты обратно в ответ
ServerResponse bytesToResponse(const vector<char>& data);

#endif
