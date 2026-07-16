# TCPServer

Многопоточный TCP-сервис на C++ с JSON API, PostgreSQL хранилищем и Docker-упаковкой.

## Возможности
- JSON-протокол: `save`, `register`, `health`, `stats`
- Аутентификация через API-ключи
- Rate-limiting (100 запросов / 60 сек на ключ)
- Пул соединений PostgreSQL (5 коннектов)
- Graceful shutdown по SIGINT/SIGTERM
- Ротация логов через `spdlog`
- Контейнеризация через Docker Compose
- Автоматическая инициализация БД

## Быстрый старт

### Требования
- Docker и Docker Compose

### Запуск
```bash
git clone <https://github.com/FroGGog/TCPServer.git>
cd TCPServer
docker compose up --build
```

Сервер будет слушать порт `4124`.

## Тестирование

### Регистрация клиента
```bash
nc 127.0.0.1 4124
{"action":"register"}
# Ответ: {"api_key":"Ab3xxqwo213"}
```

### Сохранение сообщения
```bash
{"action":"save", "api_key":"Ab3xxqwo213", "value":"Hello from Docker!"}
# Ответ: {"id":1}
```

### Health-check
```bash
{"action":"health"}
# Ответ: {"status":"ok","uptime":42}
```

### Статистика
```bash
{"action":"stats"}
# Ответ: {"active_clients":1,"requests_handled":5}
```

## Архитектура

```
┌─────────────┐      ┌──────────────┐      ┌─────────────┐
│   Client    │─────▶│  TCP Server  │─────▶│ PostgreSQL  │
│  (nc/curl)  │◀─────│  (C++ 17)    │◀─────│  (libpq)    │
└─────────────┘      └──────────────┘      └─────────────┘
                            │
                            ▼
                     ┌──────────────┐
                     │  Connection  │
                     │    Pool      │
                     │              │
                     └──────────────┘
```

### Структура проекта
```
TCPServer/
├── main.cpp              # Точка входа, цикл accept()
├── db.cpp/h              # PostgreConnection + пул
├── utils.cpp/h           # Config, генератор ключей
├── rate_limiter.h        # ClientRegistry + rate-limit
├── CMakeLists.txt        # Сборка + FetchContent
├── Dockerfile            # Multi-stage build
├── docker-compose.yml    # Сервер + PostgreSQL
├── init.sql              # Инициализация схемы БД
├── config.json           # Конфигурация
└── .github/workflows/    # CI-сборка
```

## Конфигурация ([config.json](config.json))

```json
{
    "server_host": "0.0.0.0",
    "db_host": "db",
    "server_port": 4124,
    "db_port": 5432,
    "db_name": "TCPServer",
    "user": "postgres",
    "password": "123"
}
```

## Локальная сборка (без Docker)

```bash
brew install libpq
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
./bin/TCPServer
```

## Используемые технологии
- **C++17**, POSIX sockets
- **PostgreSQL 15** + libpq
- **nlohmann/json** — JSON-парсинг
- **spdlog** — логирование с ротацией
- **CMake FetchContent** — управление зависимостями
- **Docker Compose** — контейнеризация
- **GitHub Actions** — CI/CD