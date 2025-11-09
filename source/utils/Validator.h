#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <string>
#include <vector>

using namespace std;

// Класс для валидации входных данных
// Проверяет корректность параметров графа, сетевых параметров и т.д.

class Validator {
public:
    // Проверяет корректность порта
    // port Номер порта
    // true, если порт в допустимом диапазоне (1024-65535)
    
    // Пример:
    // Validator::isValidPort(8080) -> true
    // Validator::isValidPort(80) -> false (слишком маленький)
    // Validator::isValidPort(70000) -> false (слишком большой)
    
    static bool isValidPort(int port);

    // Проверяет корректность IP-адреса
    // ip IP-адрес в строковом формате
    // true, если IP-адрес корректен (формат IPv4)
     
    // Пример:
    // Validator::isValidIP("127.0.0.1") true
    // Validator::isValidIP("192.168.1.1") true
    // Validator::isValidIP("256.1.1.1") false (256 > 255)
    // Validator::isValidIP("192.168.1") false (неполный адрес)
    
    static bool isValidIP(const string& ip);

    // Проверяет корректность типа протокола
    // protocol Название протокола
    // true, если протокол "tcp" или "udp"
    
    // Пример:
    // Validator::isValidProtocol("tcp") true
    // Validator::isValidProtocol("udp") true
    // Validator::isValidProtocol("http") false
    
    static bool isValidProtocol(const string& protocol);

    // Проверяет, что граф соответствует требованиям по размеру
    // numVertices Количество вершин
    // numEdges Количество рёбер
    // errorMessage Выходной параметр для текста ошибки
    // true, если граф валиден
    
    // Требования:
    // - Минимум 6 вершин и 6 рёбер
    // - Максимум 20 вершин и 20 рёбер
    
    static bool isValidGraphSize(int numVertices, int numEdges, string& errorMessage);

    // Проверяет, что вершина существует в списке
    // vertex Имя вершины
    // vertices Список всех вершин графа
    // true, если вершина найдена в списке
    
    static bool isValidVertex(const string& vertex, const vector<string>& vertices);

private:
    // Константы для ограничений
    static const int MIN_PORT = 1024;      // Минимальный номер порта
    static const int MAX_PORT = 65535;     // Максимальный номер порта
    static const int MIN_VERTICES = 6;     // Минимум вершин в графе
    static const int MIN_EDGES = 6;        // Минимум рёбер в графе
    static const int MAX_VERTICES = 20;    // Максимум вершин в графе
    static const int MAX_EDGES = 20;       // Максимум рёбер в графе
};

#endif