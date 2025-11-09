#include "InputParser.h"
#include <sstream>
#include <fstream>
#include <algorithm>

using namespace std;

// Парсит описание графа из строки
// Ожидаемый формат: "A B, B C, C D" - пары вершин через запятую

bool InputParser::parseGraph(const string& input, vector<Edge>& edges) {
    edges.clear(); // Очищаем вектор рёбер
    
    // Разбиваем строку по запятым
    // Например: "A B, B C, C D" -> ["A B", " B C", " C D"]
    vector<string> edgeStrings = split(input, ',');
    
    // Обрабатываем каждую пару вершин
    for (const string& edgeStr : edgeStrings) {
        // Убираем пробелы в начале и конце
        string trimmedEdge = trim(edgeStr);
        
        // Разбиваем пару по пробелам
        // Например: "A B" -> ["A", "B"]
        stringstream ss(trimmedEdge);
        string v1, v2;
        
        // Читаем две вершины
        // Он работает так же, как cin >> a >> b, только вместо cin используется поток ss
        if (!(ss >> v1 >> v2)) {
            // Если не получилось прочитать две вершины - ошибка формата
            return false;
        }
        
        // Проверяем, что после двух вершин больше ничего нет
        string extra;
        if (ss >> extra) {
            // Если есть третье слово - это ошибка
            return false;
        }
        
        // Добавляем ребро в список
        edges.push_back(Edge(v1, v2));
    }
    
    return true;
}

// Парсит начальную и конечную вершины
// Ожидаемый формат: "A B" - две вершины через пробел

bool InputParser::parseVertices(const string& input, string& start, string& end) {
    // Убираем лишние пробелы
    string trimmedInput = trim(input);
    
    // Используем stringstream для разбора
    stringstream ss(trimmedInput);
    
    // Читаем две вершины
    if (!(ss >> start >> end)) {
        // Не удалось прочитать две вершины
        return false;
    }
    
    // Проверяем, что после двух вершин больше ничего нет
    string extra;
    if (ss >> extra) {
        // Если есть третье слово - это ошибка
        return false;
    }
    
    return true;
}

// Читает граф из файла
// Каждая строка файла должна содержать два имени вершин

bool InputParser::readGraphFromFile(const string& filename, vector<Edge>& edges) {
    edges.clear();
    
    // Открываем файл для чтения
    ifstream file(filename);
    if (!file.is_open()) {
        // Не удалось открыть файл
        return false;
    }
    
    string line;
    // Читаем файл построчно
    while (getline(file, line)) {
        // Пропускаем пустые строки
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        
        // Парсим строку как пару вершин
        stringstream ss(line);
        string v1, v2;
        
        if (ss >> v1 >> v2) {
            // Успешно прочитали две вершины
            edges.push_back(Edge(v1, v2));
        } else {
            // Некорректный формат строки
            file.close();
            return false;
        }
    }
    
    file.close();
    return true;
}

// Удаляет пробелы в начале и конце строки

string InputParser::trim(const string& str) {
    // Находим первый непробельный символ
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == string::npos) {
        // Строка состоит только из пробелов
        return "";
    }
    
    // Находим последний непробельный символ
    size_t end = str.find_last_not_of(" \t\n\r");
    
    // Возвращаем подстроку от start до end включительно
    return str.substr(start, end - start + 1);
}

// Разбивает строку по разделителю
vector<string> InputParser::split(const string& str, char delimiter) {
    vector<string> tokens;
    stringstream ss(str);
    string token;
    
    // getline с третьим параметром использует указанный разделитель
    // вместо '\n'
    while (getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}
