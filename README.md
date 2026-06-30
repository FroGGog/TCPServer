# TCPServer: Echo Service with PostgreSQL

Простой многопоточный TCP-echo сервер на C++ (POSIX sockets), сохраняющий входящие сообщения (получаемые в json формате) в PostgreSQL.

## Требования
- macOS / Linux
- CMake >= 3.26
- Clang / GCC с поддержкой C++17
- PostgreSQL 15+ (локально или в Docker)

## Автоматические зависимости
Cmake автоматические подтягивает 
- nlohmann/json
- spdlog

## Сборка
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

## Настройка БД
1. Создайте БД и пользователя (или используйте существующие).
2. Обновите параметры подключения в [db.cpp](db.cpp) (строка conninfo).
3. Выполните инициализацию таблицы:

```SQL
CREATE TABLE IF NOT EXISTS messages (
    id SERIAL PRIMARY KEY,
    content TEXT NOT NULL,
    received_at TIMESTAMP DEFAULT NOW()
);
```

## Запуск 
```bash
./bin/TCPServer
```

## Тестирование
В другом терминале:
```
nc 127.0.0.1 4124
# или 
telnet 127.0.0.1 4124
{"action" : "save", "value" : "Your text"}
# Сервер вернёт: {"id" : id}
```

## Использование
На данный момент сервер принимает только сообщения формата: 
- {"action" : "save", "value" : "Your text"}

Доступные "action":
- save - принимает один аргумент "value" - текст сообщения std::string

## Остановка
Нажмите Ctrl+C. Сервер корректно закроет сокеты и соединение с БД.