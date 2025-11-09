#include "Validator.h"
#include <sstream>
#include <algorithm>

using namespace std;

// Проверяет корректность порта

bool Validator::isValidPort(int port) {
    // Порт должен быть в диапазоне от 1024 до 65535
    return port >= MIN_PORT && port <= MAX_PORT;
}

// Проверяет корректность IP-адреса в формате IPv4

bool Validator::isValidIP(const string& ip) {
    // Используем stringstream для разбора строки
    stringstream ss(ip);
    string segment;
    int count = 0; // Счётчик октетов (должно быть ровно 4)
    
    // Разбиваем строку по точкам
    while (getline(ss, segment, '.')) {
        count++;
        
        // Проверяем, что сегмент не пустой
        if (segment.empty()) {
            return false;
        }
        
        // Проверяем, что все символы - цифры
        for (char c : segment) {
            if (!isdigit(c)) {
                return false;
            }
        }
        
        // Преобразуем в число и проверяем диапазон 0-255
        try {
            int value = stoi(segment);
            if (value < 0 || value > 255) {
                return false;
            }
        } catch (...) {
            return false;
        }
    }
    
    // IP-адрес должен состоять ровно из 4 октетов
    // Например: 192.168.1.1 (4 части)
    return count == 4;
}

// Проверяет корректность протокола
bool Validator::isValidProtocol(const string& protocol) {
    // Допустимы только "tcp" и "udp" (регистронезависимо)
    string lower = protocol;
    
    // Приводим к нижнему регистру для сравнения
    transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    return lower == "tcp" || lower == "udp";
}

// Проверяет размер графа на соответствие требованиям
bool Validator::isValidGraphSize(int numVertices, int numEdges, string& errorMessage) {
    // Проверка минимального размера
    if (numVertices < MIN_VERTICES || numEdges < MIN_EDGES) {
        errorMessage = "Граф должен содержать не менее 6 вершин и 6 рёбер";
        return false;
    }
    
    // Проверка максимального размера
    if (numVertices > MAX_VERTICES || numEdges > MAX_EDGES) {
        errorMessage = "Граф должен содержать не более 20 вершин и 20 рёбер";
        return false;
    }
    
    // Всё в порядке
    errorMessage = "";
    return true;
}

// Проверяет существование вершины в списке
bool Validator::isValidVertex(const string& vertex, const vector<string>& vertices) {
    // Ищем вершину в векторе
    // find возвращает итератор на найденный элемент
    // или vertices.end(), если элемент не найден
    return find(vertices.begin(), vertices.end(), vertex) != vertices.end();
}