cmake_minimum_required(VERSION 3.10)
project(ClientServerApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# Явно указываем ВСЕ include директории
include_directories(
    ${CMAKE_SOURCE_DIR}/source
    ${CMAKE_SOURCE_DIR}/source/common
    ${CMAKE_SOURCE_DIR}/source/utils
    ${CMAKE_SOURCE_DIR}/source/server
    ${CMAKE_SOURCE_DIR}/source/client
)

# Сервер
add_executable(server
    source/server/Server.cpp
    source/server/ServerMain.cpp
    source/common/Graph.cpp
    source/common/Protocol.cpp
    source/utils/InputParser.cpp
)

# Клиент
add_executable(client
    source/client/Client.cpp
    source/client/ClientMain.cpp
    source/common/Graph.cpp
    source/common/Protocol.cpp
    source/utils/InputParser.cpp
)

# Добавляем utils файлы если они существуют
if(EXISTS "${CMAKE_SOURCE_DIR}/source/utils/Logger.cpp")
    target_sources(server PRIVATE source/utils/Logger.cpp)
    target_sources(client PRIVATE source/utils/Logger.cpp)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/source/utils/Validator.cpp")
    target_sources(server PRIVATE source/utils/Validator.cpp)
    target_sources(client PRIVATE source/utils/Validator.cpp)
endif()

target_link_libraries(server pthread)
target_link_libraries(client pthread)
