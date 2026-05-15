# Минимальная версия CMake
cmake_minimum_required(VERSION 3.26)

# Название проекта и версия
project(TCPServer VERSION 1.0.0 LANGUAGES CXX)

# Установка стандарта C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Тип сборки по умолчанию (если не указан)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING 
        "Тип сборки: Debug, Release, RelWithDebInfo, MinSizeRel" FORCE)
endif()

# Опции компилятора в зависимости от типа сборки
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    # Предупреждения для GCC/Clang
    add_compile_options(-Wall -Wextra -Wpedantic)
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -DNDEBUG)
    endif()
endif()

# Исходные файлы (можно перечислить вручную или использовать GLOB)
set(SOURCES
    main.cpp
    db.cpp
    utils.cpp
)

# Создание исполняемого файла
add_executable(${PROJECT_NAME} ${SOURCES})

list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/libpq")
find_package(PostgreSQL REQUIRED)

target_include_directories(${PROJECT_NAME} PRIVATE ${PostgreSQL_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${PostgreSQL_LIBRARIES})

# Для macOS: фреймворки
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework CoreFoundation")
endif()

# Установка (опционально)
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(FILES ${HEADERS} DESTINATION include)

# Настройка вывода (опционально)
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)


# Печать информации о конфигурации
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")