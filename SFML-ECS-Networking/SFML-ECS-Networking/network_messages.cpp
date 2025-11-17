#include "network_messages.h"

namespace NetworkUtils {
    // PlayerData serialization
    /**
     * Serializes PlayerData to packet for network transmission in multiplayer sync.
     * @param packet SFML packet reference for binary data appending.
     * @param player Const PlayerData reference to avoid copying during serialization.
     * @return Packet reference for chaining per SFML standards.
     */
    sf::Packet& operator<<(sf::Packet& packet, const PlayerData& player) {
        packet << player.playerId
            << player.playerName
            << player.x
            << player.y
            << player.bodyRotation
            << player.barrelRotation
            << player.color
            << player.isMoving_forward
            << player.isMoving_backward
            << player.isMoving_left
            << player.isMoving_right
            << player.health
            << player.maxHealth
            << player.score
            << player.isDead;
        return packet;
    }
    /**
     * Deserializes PlayerData from packet for receiving game state updates.
     * @param packet SFML packet reference for data extraction.
     * @param player PlayerData reference to populate with received data.
     * @return Packet reference for chaining per SFML standards.
     */
    sf::Packet& operator>>(sf::Packet& packet, PlayerData& player) {
        packet >> player.playerId
            >> player.playerName
            >> player.x
            >> player.y
            >> player.bodyRotation
            >> player.barrelRotation
            >> player.color
            >> player.isMoving_forward
            >> player.isMoving_backward
            >> player.isMoving_left
            >> player.isMoving_right
            >> player.health
            >> player.maxHealth
            >> player.score
            >> player.isDead;
        return packet;
    }
    // EnemyData serialization
    /**
     * Serializes EnemyData to packet for syncing enemy states across clients.
     * @param packet SFML packet reference for appending enemy info.
     * @param enemy Const EnemyData reference for efficient serialization.
     * @return Packet reference for operator chaining.
     */
    sf::Packet& operator<<(sf::Packet& packet, const EnemyData& enemy) {
        packet << enemy.enemyId
            << enemy.enemyType
            << enemy.x
            << enemy.y
            << enemy.bodyRotation
            << enemy.barrelRotation
            << enemy.health
            << enemy.maxHealth;
        return packet;
    }
    /**
     * Deserializes EnemyData from packet to update local enemy representations.
     * @param packet SFML packet reference for data reading.
     * @param enemy EnemyData reference to fill with deserialized values.
     * @return Packet reference for chaining.
     */
    sf::Packet& operator>>(sf::Packet& packet, EnemyData& enemy) {
        packet >> enemy.enemyId
            >> enemy.enemyType
            >> enemy.x
            >> enemy.y
            >> enemy.bodyRotation
            >> enemy.barrelRotation
            >> enemy.health
            >> enemy.maxHealth;
        return packet;
    }
    // BulletData serialization
    /**
     * Serializes BulletData to packet for transmitting bullet spawns/updates.
     * @param packet SFML packet for appending bullet properties.
     * @param bullet Const BulletData ref for serialization without modification.
     * @return Packet ref for chaining.
     */
    sf::Packet& operator<<(sf::Packet& packet, const BulletData& bullet) {
        packet << bullet.bulletId
            << bullet.ownerId
            << bullet.bulletType
            << bullet.x
            << bullet.y
            << bullet.velocityX
            << bullet.velocityY
            << bullet.rotation
            << bullet.damage
            << bullet.lifetime
            << bullet.spawnTime;
        return packet;
    }
    /**
     * Deserializes BulletData from packet to recreate bullets client-side.
     * @param packet SFML packet for extracting bullet data.
     * @param bullet BulletData ref to populate.
     * @return Packet ref for chaining.
     */
    sf::Packet& operator>>(sf::Packet& packet, BulletData& bullet) {
        packet >> bullet.bulletId
            >> bullet.ownerId
            >> bullet.bulletType
            >> bullet.x
            >> bullet.y
            >> bullet.velocityX
            >> bullet.velocityY
            >> bullet.rotation
            >> bullet.damage
            >> bullet.lifetime
            >> bullet.spawnTime;
        return packet;
    }
    // JoinMessage serialization
    /**
     * Serializes JoinMessage to packet for client join requests.
     * @param packet Packet for appending join details.
     * @param msg Const JoinMessage ref for data.
     * @return Packet ref.
     */
    sf::Packet& operator<<(sf::Packet& packet, const JoinMessage& msg) {
        packet << static_cast<uint8_t>(msg.type)
            << msg.playerName
            << msg.preferredColor
            << msg.timestamp
            << msg.sequenceNumber;
        return packet;
    }
    /**
     * Deserializes JoinMessage from packet to handle new players.
     * @param packet Packet for reading.
     * @param msg JoinMessage ref to fill.
     * @return Packet ref.
     */
    sf::Packet& operator>>(sf::Packet& packet, JoinMessage& msg) {
        uint8_t type;
        packet >> type
            >> msg.playerName
            >> msg.preferredColor
            >> msg.timestamp
            >> msg.sequenceNumber;
        msg.type = static_cast<NetMessageType>(type);
        return packet;
    }
    // PlayerUpdateMessage serialization
    /**
     * Serializes PlayerUpdateMessage for sending position/rotation updates.
     * @param packet Packet for data.
     * @param msg Const msg ref.
     * @return Packet ref.
     */
    sf::Packet& operator<<(sf::Packet& packet, const PlayerUpdateMessage& msg) {
        packet << static_cast<uint8_t>(msg.type)
            << msg.playerId
            << msg.x
            << msg.y
            << msg.bodyRotation
            << msg.barrelRotation
            << msg.isMoving_forward
            << msg.isMoving_backward
            << msg.isMoving_left
            << msg.isMoving_right
            << msg.timestamp
            << msg.sequenceNumber;
        return packet;
    }
    /**
     * Deserializes PlayerUpdateMessage to apply updates.
     * @param packet Packet.
     * @param msg Msg ref.
     * @return Packet ref.
     */
    sf::Packet& operator>>(sf::Packet& packet, PlayerUpdateMessage& msg) {
        uint8_t type;
        packet >> type
            >> msg.playerId
            >> msg.x
            >> msg.y
            >> msg.bodyRotation
            >> msg.barrelRotation
            >> msg.isMoving_forward
            >> msg.isMoving_backward
            >> msg.isMoving_left
            >> msg.isMoving_right
            >> msg.timestamp
            >> msg.sequenceNumber;
        msg.type = static_cast<NetMessageType>(type);
        return packet;
    }
    
    // PlayerInputMessage serialization
    /**
     * Serializes PlayerInputMessage for client inputs to server.
     * @param packet Packet.
     * @param msg Const ref.
     * @return Ref.
     */
    sf::Packet& operator<<(sf::Packet& packet, const PlayerInputMessage& msg) {
        packet << static_cast<uint8_t>(msg.type)
            << msg.playerId
            << msg.isMoving_forward
            << msg.isMoving_backward
            << msg.isMoving_left
            << msg.isMoving_right
            << msg.timestamp
            << msg.sequenceNumber
            << msg.barrelRotation;
        return packet;
    }
    /**
     * Deserializes PlayerInputMessage for server processing.
     * @param packet Packet.
     * @param msg Ref.
     * @return Ref.
     */
    sf::Packet& operator>>(sf::Packet& packet, PlayerInputMessage& msg) {
        uint8_t type;
        packet >> type
            >> msg.playerId
            >> msg.isMoving_forward
            >> msg.isMoving_backward
            >> msg.isMoving_left
            >> msg.isMoving_right
            >> msg.timestamp
            >> msg.sequenceNumber
            >> msg.barrelRotation;
        msg.type = static_cast<NetMessageType>(type);
        return packet;
    }
    // GameStateMessage serialization
    /**
     * Serializes GameStateMessage with players/enemies for full state sync.
     * @param packet Packet.
     * @param msg Const ref.
     * @return Ref.
     */
    sf::Packet& operator<<(sf::Packet& packet, const GameStateMessage& msg) {
        packet << static_cast<uint8_t>(msg.type);
        // Serialize player count and players
        packet << static_cast<uint32_t>(msg.players.size());
        for (const auto& player : msg.players) {
            packet << player;
        }
        // Serialize enemy count and enemies
        packet << static_cast<uint32_t>(msg.enemies.size());
        for (const auto& enemy : msg.enemies) {
            packet << enemy;
        }
        packet << msg.timestamp
            << msg.sequenceNumber
            << msg.lastAckedInput;
        return packet;
    }
    /**
     * Deserializes GameStateMessage to reconstruct game state.
     * @param packet Packet.
     * @param msg Ref.
     * @return Ref.
     */
    sf::Packet& operator>>(sf::Packet& packet, GameStateMessage& msg) {
        uint8_t type;
        uint32_t playerCount;
        uint32_t enemyCount;
        packet >> type >> playerCount;
        msg.type = static_cast<NetMessageType>(type);
        // Deserialize players
        msg.players.clear();
        msg.players.reserve(playerCount);
        for (uint32_t i = 0; i < playerCount; ++i) {
            PlayerData player;
            packet >> player;
            msg.players.push_back(player);
        }
        // Deserialize enemies
        packet >> enemyCount;
        msg.enemies.clear();
        msg.enemies.reserve(enemyCount);
        for (uint32_t i = 0; i < enemyCount; ++i) {
            EnemyData enemy;
            packet >> enemy;
            msg.enemies.push_back(enemy);
        }
        packet >> msg.timestamp
            >> msg.sequenceNumber
            >> msg.lastAckedInput;
        return packet;
    }
    // PingMessage serialization
    /**
     * Serializes PingMessage for latency measurement.
     * @param packet Packet.
     * @param msg Const ref.
     * @return Ref.
     */
    sf::Packet& operator<<(sf::Packet& packet, const PingMessage& msg) {
        packet << static_cast<uint8_t>(msg.type)
            << msg.timestamp
            << msg.sequenceNumber;
        return packet;
    }
    /**
     * Deserializes PingMessage to respond with pong.
     * @param packet Packet.
     * @param msg Ref.
     * @return Ref.
     */
    sf::Packet& operator>>(sf::Packet& packet, PingMessage& msg) {
        uint8_t type;
        packet >> type
            >> msg.timestamp
            >> msg.sequenceNumber;
        msg.type = static_cast<NetMessageType>(type);
        return packet;
    }
    // PongMessage serialization
    /**
     * Serializes PongMessage as ping response.
     * @param packet Packet.
     * @param msg Const ref.
     * @return Ref.
     */
    sf::Packet& operator<<(sf::Packet& packet, const PongMessage& msg) {
        packet << static_cast<uint8_t>(msg.type)
            << msg.originalTimestamp
            << msg.sequenceNumber;
        return packet;
    }
    /**
     * Deserializes PongMessage to calculate RTT.
     * @param packet Packet.
     * @param msg Ref.
     * @return Ref.
     */
    sf::Packet& operator>>(sf::Packet& packet, PongMessage& msg) {
        uint8_t type;
        packet >> type
            >> msg.originalTimestamp
            >> msg.sequenceNumber;
        msg.type = static_cast<NetMessageType>(type);
        return packet;
    }
    // InputAcknowledgmentMessage serialization
    /**
     * Serializes InputAcknowledgmentMessage for confirming inputs.
     * @param packet Packet.
     * @param msg Const ref.
     * @return Ref.
     */
    sf::Packet& operator<<(sf::Packet& packet, const InputAcknowledgmentMessage& msg) {
        packet << static_cast<uint8_t>(msg.type)
            << msg.playerId
            << msg.acknowledgedSequence
            << msg.serverTimestamp;
        return packet;
    }
    /**
     * Deserializes InputAcknowledgmentMessage to process acks.
     * @param packet Packet.
     * @param msg Ref.
     * @return Ref.
     */
    sf::Packet& operator>>(sf::Packet& packet, InputAcknowledgmentMessage& msg) {
        uint8_t type;
        packet >> type
            >> msg.playerId
            >> msg.acknowledgedSequence
            >> msg.serverTimestamp;
        msg.type = static_cast<NetMessageType>(type);
        return packet;
    }
    // BulletSpawnMessage serialization
    /**
     * Serializes BulletSpawnMessage for notifying bullet fires.
     * @param packet Packet.
     * @param msg Const ref.
     * @return Ref.
     */
    sf::Packet& operator<<(sf::Packet& packet, const BulletSpawnMessage& msg) {
        packet << static_cast<uint8_t>(msg.type)
            << msg.playerId
            << msg.spawnX
            << msg.spawnY
            << msg.directionX
            << msg.directionY
            << msg.barrelRotation
            << msg.timestamp
            << msg.sequenceNumber;
        return packet;
    }
    /**
     * Deserializes BulletSpawnMessage to spawn bullets.
     * @param packet Packet.
     * @param msg Ref.
     * @return Ref.
     */
    sf::Packet& operator>>(sf::Packet& packet, BulletSpawnMessage& msg) {
        uint8_t type;
        packet >> type
            >> msg.playerId
            >> msg.spawnX
            >> msg.spawnY
            >> msg.directionX
            >> msg.directionY
            >> msg.barrelRotation
            >> msg.timestamp
            >> msg.sequenceNumber;
        msg.type = static_cast<NetMessageType>(type);
        return packet;
    }
    // BulletUpdateMessage serialization
    /**
     * Serializes BulletUpdateMessage with multiple bullets for sync.
     * @param packet Packet.
     * @param msg Const ref.
     * @return Ref.
     */
    sf::Packet& operator<<(sf::Packet& packet, const BulletUpdateMessage& msg) {
        packet << static_cast<uint8_t>(msg.type);
        // Serialize bullet count and bullets
        uint32_t bulletCount = static_cast<uint32_t>(msg.bullets.size());
        packet << bulletCount;
        for (const auto& bullet : msg.bullets) {
            packet << bullet;
        }
        packet << msg.timestamp << msg.sequenceNumber;
        return packet;
    }
    /**
     * Deserializes BulletUpdateMessage to update bullets.
     * @param packet Packet.
     * @param msg Ref.
     * @return Ref.
     */
    sf::Packet& operator>>(sf::Packet& packet, BulletUpdateMessage& msg) {
        uint8_t type;
        uint32_t bulletCount;
        packet >> type >> bulletCount;
        msg.type = static_cast<NetMessageType>(type);
        // Deserialize bullets
        msg.bullets.clear();
        msg.bullets.reserve(bulletCount);
        for (uint32_t i = 0; i < bulletCount; ++i) {
            BulletData bullet;
            packet >> bullet;
            msg.bullets.push_back(bullet);
        }
        packet >> msg.timestamp >> msg.sequenceNumber;
        return packet;
    }
    // BulletDestroyMessage serialization
    /**
     * Serializes BulletDestroyMessage for removing bullets on hit/expiry.
     * @param packet Packet.
     * @param msg Const ref.
     * @return Ref.
     */
    sf::Packet& operator<<(sf::Packet& packet, const BulletDestroyMessage& msg) {
        packet << static_cast<uint8_t>(msg.type)
            << msg.bulletId
            << msg.destroyReason
            << msg.hitTargetId
            << msg.hitX
            << msg.hitY
            << msg.timestamp
            << msg.sequenceNumber;
        return packet;
    }
    /**
     * Deserializes BulletDestroyMessage to handle destructions.
     * @param packet Packet.
     * @param msg Ref.
     * @return Ref.
     */
    sf::Packet& operator>>(sf::Packet& packet, BulletDestroyMessage& msg) {
        uint8_t type;
        packet >> type
            >> msg.bulletId
            >> msg.destroyReason
            >> msg.hitTargetId
            >> msg.hitX
            >> msg.hitY
            >> msg.timestamp
            >> msg.sequenceNumber;
        msg.type = static_cast<NetMessageType>(type);
        return packet;
    }
} // namespace NetworkUtils