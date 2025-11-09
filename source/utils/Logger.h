#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <mutex>
#include <iostream>

using namespace std;

// Класс для потокобезопасного логирования сообщений

// Используется для вывода информационных сообщений, предупреждений и ошибок
// Потокобезопасен - можно использовать из разных потоков одновременно

class Logger {
public:
    // Типы сообщений для логирования
    enum class Level {
        INFO,     // Информационное сообщение (например: "Сервер запущен")
        WARNING,  // Предупреждение (например: "Некорректный формат")
        ERROR     // Ошибка (например: "Не удалось подключиться")
    };

    
    // Логирует информационное сообщение
    // message Текст сообщения
    
    // Пример: Logger::info("Сервер запущен на порту 8080");
    // Вывод: [INFO] Сервер запущен на порту 8080
    
    static void info(const string& message);

    // Логирует предупреждение
    // message Текст предупреждения
     
    // Пример: Logger::warning("Получены неполные данные");
    // Вывод: [WARNING] Получены неполные данные
    
    static void warning(const string& message);

    // Логирует ошибку
    // message Текст ошибки
    
    // Пример: Logger::error("Не удалось подключиться к серверу");
    // Вывод: [ERROR] Не удалось подключиться к серверу
    
    static void error(const string& message);

private:
    // Общая функция для логирования с указанием уровня
    // level Уровень сообщения (INFO, WARNING, ERROR)
    // message Текст сообщения
    static void log(Level level, const string& message);

    // Мьютекс для синхронизации доступа к консоли из разных потоков
    static mutex logMutex;
};

#endif