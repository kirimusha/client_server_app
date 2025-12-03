#ifndef UDP_PROTOCOL_H
#define UDP_PROTOCOL_H

#include <cstdint>
#include <vector>
#include <cstring>

// Типы UDP-сообщений
enum UDPPacketType : uint8_t {
    PACKET_DATA = 0,    // Полезная нагрузка (запрос/ответ)
    PACKET_ACK = 1      // Подтверждение получения
};

// Структура заголовка UDP-пакета
struct UDPPacketHeader {
    uint8_t type;       // Тип пакета (PACKET_DATA или PACKET_ACK)
    uint32_t packet_id; // Уникальный ID пакета
    uint32_t data_len;  // Длина полезной нагрузки (байты)
    
    // Метод для сериализации
    std::vector<char> serialize() const {
        std::vector<char> data(sizeof(UDPPacketHeader));
        memcpy(data.data(), this, sizeof(UDPPacketHeader));
        return data;
    }
    
    // Метод для десериализации
    static UDPPacketHeader deserialize(const std::vector<char>& data) {
        UDPPacketHeader header;
        if (data.size() >= sizeof(UDPPacketHeader)) {
            memcpy(&header, data.data(), sizeof(UDPPacketHeader));
        }
        return header;
    }
};

// Функции для создания пакетов
namespace UDPProtocol {
    // Создаёт пакет с данными
    inline std::vector<char> createDataPacket(uint32_t packet_id, const std::vector<char>& payload) {
        UDPPacketHeader header;
        header.type = PACKET_DATA;
        header.packet_id = packet_id;
        header.data_len = payload.size();
        
        std::vector<char> packet = header.serialize();
        packet.insert(packet.end(), payload.begin(), payload.end());
        return packet;
    }
    
    // Создаёт ACK-пакет
    inline std::vector<char> createAckPacket(uint32_t packet_id) {
        UDPPacketHeader header;
        header.type = PACKET_ACK;
        header.packet_id = packet_id;
        header.data_len = 0;
        return header.serialize();
    }
    
    // Парсит пакет и извлекает заголовок + данные
    inline std::pair<UDPPacketHeader, std::vector<char>> parsePacket(const std::vector<char>& packet) {
        if (packet.size() < sizeof(UDPPacketHeader)) {
            return {UDPPacketHeader(), std::vector<char>()};
        }
        
        UDPPacketHeader header = UDPPacketHeader::deserialize(packet);
        std::vector<char> payload;
        
        if (packet.size() > sizeof(UDPPacketHeader)) {
            payload.assign(packet.begin() + sizeof(UDPPacketHeader), packet.end());
        }
        
        return {header, payload};
    }
}

#endif