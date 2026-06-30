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

include(FetchContent)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.16.0
  GIT_SHALLOW    TRUE
)

list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/libpq")
find_package(PostgreSQL REQUIRED)

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(SPDLOG_BUILD_TESTS OFF CACHE INTERNAL "")

FetchContent_MakeAvailable(nlohmann_json spdlog)

target_include_directories(${PROJECT_NAME} PRIVATE ${PostgreSQL_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PostgreSQL_LIBRARIES})
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)

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

set(COPY_FILES
    "${CMAKE_SOURCE_DIR}/config.json"
)

add_custom_command(TARGET TCPServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${COPY_FILES} $<TARGET_FILE_DIR:TCPServer>/
    COMMENT "Copying files to output directory"
)

message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")