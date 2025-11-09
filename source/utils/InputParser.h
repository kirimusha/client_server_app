#ifndef INPUT_PARSER_H
#define INPUT_PARSER_H

#include <string>
#include <vector>
#include <utility>

using namespace std;

//  Класс для парсинга входных данных
//  Обрабатывает ввод пользователя: описание графа, вершины, параметры командной строки

class InputParser {
public:
    // Структура для хранения ребра графа
    
    struct Edge {
        string vertex1;  // Первая вершина ребра
        string vertex2;  // Вторая вершина ребра
        
        // Конструктор для удобного создания
        Edge(const string& v1, const string& v2) 
            : vertex1(v1), vertex2(v2) {}
    };

    // Парсит описание графа из строки
    // input Строка с описанием рёбер (например: "A B, B C, C D")
    // edges Выходной параметр - вектор рёбер
    // true, если парсинг успешен
    
    // Формат ввода: пары вершин, разделённые запятыми
    // Пример входа: "A B, B C, C D, D E, E F, F A"
    // Результат: [(A,B), (B,C), (C,D), (D,E), (E,F), (F,A)]

    static bool parseGraph(const string& input, vector<Edge>& edges);

    // Парсит начальную и конечную вершины
    // input Строка с двумя вершинами (например: "A B")
    // start Выходной параметр - начальная вершина
    // end Выходной параметр - конечная вершина
    // true, если парсинг успешен
    
    // Пример входа: "A B"
    // Результат: start="A", end="B"
    
    static bool parseVertices(const string& input, string& start, string& end);

    // Читает граф из файла
    // filename Путь к файлу
    // edges Выходной параметр - вектор рёбер
    // true, если чтение успешно
    
    // Формат файла:
    // A B
    // B C
    // C D
    // ...
     
    // Каждая строка содержит два имени вершин, разделённых пробелом
    
    static bool readGraphFromFile(const string& filename, vector<Edge>& edges);

    // Удаляет пробелы в начале и конце строки
    
    // Пример:
    // trim("  hello  ") -> "hello"
    // trim("\ttest\n") -> "test"
    
    static string trim(const string& str);

private:
    // Разбивает строку по разделителю
    // str Исходная строка
    // delimiter Символ-разделитель
    // return Вектор подстрок
    
    // Пример:
    // split("A,B,C", ',') -> ["A", "B", "C"]
    
    static vector<string> split(const string& str, char delimiter);
};

#endif
