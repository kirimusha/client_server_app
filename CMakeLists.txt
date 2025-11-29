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

# Для клиента
add_executable(client
    source/client/ClientMain.cpp
    source/client/Client.cpp
    source/common/Protocol.cpp
    source/utils/InputParser.cpp
    source/utils/Validator.cpp
    source/utils/Logger.cpp
    source/utils/FileReader.cpp 
    source/common/Graph.cpp
)

# Для сервера
add_executable(server
    source/server/ServerMain.cpp
    source/server/Server.cpp
    source/common/Protocol.cpp
    source/common/Graph.cpp
    source/utils/InputParser.cpp
    source/utils/Validator.cpp
    source/utils/Logger.cpp
    source/utils/FileReader.cpp  # Добавьте если создали
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
