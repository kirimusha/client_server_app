#include "Server.h"
#include "Logger.h"
#include "Validator.h"
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <cstring>


using namespace std;

// Глобальный указатель на сервер для обработки сигналов
Server* globalServer = nullptr;

// Обработчик сигнала завершения (Ctrl+C)
// Когда пользователь нажимает Ctrl+C, эта функция останавливает сервер
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        Logger::info("Получен сигнал завершения");
        if (globalServer != nullptr) {
            globalServer->stop();
        }
        exit(0);
    }
}

// Выводит справку по использованию программы
void printUsage(const char* programName) {
    cout << "Использование: " << programName << " <порт> <протокол>" << endl;
    cout << endl;
    cout << "Параметры:" << endl;
    cout << "  <порт>     - Номер порта для прослушивания (1024-65535)" << endl;
    cout << "  <протокол> - Протокол: tcp или udp" << endl;
    cout << endl;
    cout << "Примеры:" << endl;
    cout << "  " << programName << " 8080 tcp" << endl;
    cout << "  " << programName << " 12345 udp" << endl;
}

// Главная функция сервера
int main(int argc, char* argv[]) {
    // Проверяем количество аргументов
    // argv[0] - имя программы
    // argv[1] - порт
    // argv[2] - протокол
    if (argc != 3) {
        Logger::error("Неверное количество аргументов");
        printUsage(argv[0]);
        return 1;
    }
    
    // Парсим порт из строки в число
    int port;
    try {
        port = stoi(argv[1]);
    } catch (...) {
        Logger::error("Порт должен быть числом");
        printUsage(argv[0]);
        return 1;
    }
    
    // Валидируем порт
    if (!Validator::isValidPort(port)) {
        Logger::error("Порт должен быть в диапазоне 1024-65535");
        return 1;
    }
    
    // Получаем протокол (теперь обязательный параметр)
    string protocol = argv[2];
    if (protocol != "tcp" && protocol != "udp") {
        Logger::error("Протокол должен быть 'tcp' или 'udp'");
        printUsage(argv[0]);
        return 1;
    }
    
    // Создаём сервер
    Server server(port, protocol);
    globalServer = &server;
    
    // Устанавливаем обработчик сигналов
    // SIGINT - сигнал прерывания (Ctrl+C)
    // SIGTERM - сигнал завершения (kill)
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Запускаем сервер
    if (!server.start()) {
        Logger::error("Не удалось запустить сервер");
        return 1;
    }
    
    // Запускаем главный цикл обработки клиентов
    try {
        server.run();
    } catch (const exception& e) {
        Logger::error(string("Ошибка выполнения: ") + e.what());
        return 1;
    }
    
    return 0;
}
