#include "Logger.h"
#include <iostream>
#include <ctime>

using namespace std;

// Инициализация статического мьютекса
mutex Logger::logMutex;

// Логирует информационное сообщение
void Logger::info(const string& message) {
    log(Level::INFO, message);
}

// Логирует предупреждение
void Logger::warning(const string& message) {
    log(Level::WARNING, message);
}

// Логирует ошибку
void Logger::error(const string& message) {
    log(Level::ERROR, message);
}

// Основная функция логирования

// Выводит сообщение в формате: [УРОВЕНЬ] сообщение
// Использует мьютекс для предотвращения смешивания вывода из разных потоков
void Logger::log(Level level, const string& message) {
    // Блокируем мьютекс, чтобы только один поток мог писать в консоль
    lock_guard<mutex> lock(logMutex);
    
    // Определяем префикс в зависимости от уровня
    string prefix;
    switch (level) {
        case Level::INFO:
            prefix = "[INFO] ";
            break;
        case Level::WARNING:
            prefix = "[WARNING] ";
            break;
        case Level::ERROR:
            prefix = "[ERROR] ";
            break;
    }
    
    // Выводим сообщение с префиксом
    cout << prefix << message << endl;
}