// реализация функций сериализации и десериализации

#include "Protocol.h"
#include <vector>

using namespace std;

// функция преобразует структуру с двумя целыми числами в плоский массив байтов
// для передачи по сети или сохранения в файл.
vector<char> requestToBytes(const ClientRequest& request) {
    vector<char> data;
    
    // Цель заранее выделить память для двух целых чисел
    for (size_t i = 0; i < sizeof(int) * 2; i++) {
        data.push_back(0);
    }
    
    // reinterpret_cast - преобразует указатель на int в указатель на char
    // мы можем обращаться к числу как к массиву байтов
    // Цикл копирует каждый байт числа start_node в вектор
    const char* start_bytes = reinterpret_cast<const char*>(&request.start_node);
    for (size_t i = 0; i < sizeof(int); i++) {
        data[i] = start_bytes[i];
    }
    // Пример для числа 0x12345678 (4 байта):
    // Память числа: [0x12, 0x34, 0x56, 0x78]
    // Вектор data: [0x12, 0x34, 0x56, 0x78, 0, 0, 0, 0]
    
    // Копируем end_node побайтово
    // Аналогично первому, но записывается со смещением sizeof(int)
    // Записывается во вторую половину вектора
    const char* end_bytes = reinterpret_cast<const char*>(&request.end_node);
    for (size_t i = 0; i < sizeof(int); i++) {
        data[sizeof(int) + i] = end_bytes[i];
    }
    // После второго числа:
    // Вектор data: [0x12, 0x34, 0x56, 0x78, 0xAB, 0xCD, 0xEF, 0x99]
        
    return data;
}

// функция преобразует массив байтов обратно в структуру с двумя целыми числами
// используется для восстановления данных после передачи по сети или чтения из файла
ClientRequest bytesToRequest(const vector<char>& data) {
    ClientRequest request;
    
    // reinterpret_cast - преобразует указатель на int в указатель на char
    // теперь мы можем записывать байты непосредственно в память числа
    // Цикл восстанавливает каждый байт числа start_node из вектора
    char* start_bytes = reinterpret_cast<char*>(&request.start_node);
    for (size_t i = 0; i < sizeof(int); i++) {
        start_bytes[i] = data[i];
    }
    // Пример восстановления числа из байтов:
    // Вектор data: [0x12, 0x34, 0x56, 0x78, 0xAB, 0xCD, 0xEF, 0x99]
    // Память start_node после восстановления: [0x12, 0x34, 0x56, 0x78]
    // Число start_node = 0x12345678
    
    // Восстанавливаем end_node побайтово
    // Аналогично первому, но читается со смещением sizeof(int)
    // Читается из второй половины вектора
    char* end_bytes = reinterpret_cast<char*>(&request.end_node);
    for (size_t i = 0; i < sizeof(int); i++) {
        end_bytes[i] = data[sizeof(int) + i];
    }
    // Память end_node после восстановления: [0xAB, 0xCD, 0xEF, 0x99]
    // Число end_node = 0xABCDEF99
    
    return request;
}

// функция преобразует сложную структуру ServerResponse в плоский массив байтов
// структура содержит: код ошибки, длину пути, размер пути и сам путь (вектор чисел)
vector<char> responseToBytes(const ServerResponse& response) {
    vector<char> data;
    // Вычисляем количество элементов в пути и общий размер данных
    int path_size = static_cast<int>(response.path.size());
    int total_size = sizeof(int) + sizeof(int) + sizeof(int) + (path_size * sizeof(int));
    
    // Заранее выделяем память для всего блока данных
    // total_size = error_code + path_length + path_size + path_data
    for (int i = 0; i < total_size; i++) {
        data.push_back(0);
    }
    
    int offset = 0; // Текущая позиция в буфере для записи
    
    // Копируем error_code (целое число)
    // reinterpret_cast позволяет работать с числом как с массивом байтов
    const char* error_bytes = reinterpret_cast<const char*>(&response.error_code);
    for (size_t i = 0; i < sizeof(int); i++) {
        data[offset + i] = error_bytes[i];
    }
    offset += sizeof(int); // Сдвигаем позицию на размер int
    // После копирования error_code:
    // data: [error_code_byte0, error_code_byte1, error_code_byte2, error_code_byte3, ...]
    
    // Копируем path_length (число с плавающей точкой double)
    const char* length_bytes = reinterpret_cast<const char*>(&response.path_length);
    for (size_t i = 0; i < sizeof(int); i++) {
        data[offset + i] = length_bytes[i];
    }
    offset += sizeof(int); // Сдвигаем позицию на размер double
    // После копирования path_length:
    // data: [error_code(4b), path_length_byte0, path_length_byte1, ..., path_length_byte7, ...]
    
    // Копируем размер пути path_size (целое число)
    // Это количество элементов в векторе response.path
    const char* size_bytes = reinterpret_cast<const char*>(&path_size);
    for (size_t i = 0; i < sizeof(int); i++) {
        data[offset + i] = size_bytes[i];
    }
    offset += sizeof(int); // Сдвигаем позицию на размер int
    // После копирования path_size:
    // data: [error_code(4b), path_length(8b), path_size_byte0, path_size_byte1, ...]
    
    // Копируем элементы пути (каждый элемент - целое число)
    for (int i = 0; i < path_size; i++) {
        const char* path_bytes = reinterpret_cast<const char*>(&response.path[i]);
        for (size_t j = 0; j < sizeof(int); j++) {
            data[offset + j] = path_bytes[j];
        }
        offset += sizeof(int); // Сдвигаем позицию для следующего элемента
    }
    // После копирования всех элементов пути:
    // data: [error_code(4b), path_length(8b), path_size(4b), path[0](4b), path[1](4b), ...]
    
    return data;
}

// функция преобразует массив байтов обратно в сложную структуру ServerResponse
// выполняет десериализацию данных, восстановление структуры из плоского байтового потока
ServerResponse bytesToResponse(const vector<char>& data) {
    ServerResponse response;
    int offset = 0; // Текущая позиция в буфере для чтения
    
    // Восстанавливаем error_code (целое число)
    // reinterpret_cast позволяет записывать байты непосредственно в память числа
    char* error_bytes = reinterpret_cast<char*>(&response.error_code);
    for (size_t i = 0; i < sizeof(int); i++) {
        error_bytes[i] = data[offset + i];
    }
    offset += sizeof(int); // Сдвигаем позицию на размер int
    // После восстановления error_code:
    // response.error_code = число, восстановленное из первых 4 байтов
    
    // Восстанавливаем path_length (число с плавающей точкой double)
    char* length_bytes = reinterpret_cast<char*>(&response.path_length);
    for (size_t i = 0; i < sizeof(int); i++) {
        length_bytes[i] = data[offset + i];
    }
    offset += sizeof(int); // Сдвигаем позицию на размер double
    // После восстановления path_length:
    // response.path_length = число double, восстановленное из следующих 8 байтов
    
    // Восстанавливаем размер пути path_size (целое число)
    // Это количество элементов в векторе пути, которое нужно восстановить
    int path_size;
    char* size_bytes = reinterpret_cast<char*>(&path_size);
    for (size_t i = 0; i < sizeof(int); i++) {
        size_bytes[i] = data[offset + i];
    }
    offset += sizeof(int); // Сдвигаем позицию на размер int
    // После восстановления path_size:
    // path_size = число, определяющее сколько элементов пути нужно прочитать
    
    // Восстанавливаем элементы пути через push_back
    // Читаем path_size элементов и добавляем их в вектор response.path
    for (int i = 0; i < path_size; i++) {
        int path_element; // Временная переменная для восстановления одного элемента
        char* path_bytes = reinterpret_cast<char*>(&path_element);
        // Восстанавливаем один элемент пути (целое число)
        for (size_t j = 0; j < sizeof(int); j++) {
            path_bytes[j] = data[offset + j];
        }
        response.path.push_back(path_element); // Добавляем элемент в вектор
        offset += sizeof(int); // Сдвигаем позицию для следующего элемента
    }
    // После восстановления всех элементов пути:
    // response.path содержит path_size элементов, восстановленных из байтового потока
    
    return response;
}
