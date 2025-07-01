#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_now.h"

#ifdef __cplusplus
extern "C" {
#endif

// Mesh Configuration
#define MESH_MAX_NODES              32
#define MESH_CHANNEL                1
#define MESH_MAX_RETRIES            3
#define MESH_TIMEOUT_MS             1000
#define MESH_HEARTBEAT_INTERVAL_MS  5000
#define MESH_MAX_PAYLOAD_SIZE       200

// Message Types
#define MESH_MSG_HEARTBEAT          0x01
#define MESH_MSG_AUDIO_DATA         0x02
#define MESH_MSG_SENSOR_DATA        0x03
#define MESH_MSG_COMMAND            0x04
#define MESH_MSG_DISCOVERY          0x05
#define MESH_MSG_SYNC               0x06
#define MESH_MSG_ACK                0x07

// Node Types
#define MESH_NODE_COORDINATOR       0x01
#define MESH_NODE_SENSOR            0x02
#define MESH_NODE_ANALYZER          0x03
#define MESH_NODE_RELAY             0x04

// Node States
#define MESH_STATE_DISCONNECTED     0
#define MESH_STATE_DISCOVERING      1
#define MESH_STATE_CONNECTED        2
#define MESH_STATE_COORDINATOR      3

// Audio Data Structure
typedef struct __attribute__((packed)) {
    uint8_t msg_type;
    uint8_t node_id;
    uint16_t sequence;
    float spl_value;
    float frequency_bands[8];
    uint32_t timestamp;
    uint8_t checksum;
} mesh_audio_packet_t;

// Sensor Data Structure
typedef struct __attribute__((packed)) {
    uint8_t msg_type;
    uint8_t node_id;
    uint16_t sequence;
    float temperature;
    float humidity;
    float pressure;
    float altitude;
    uint32_t timestamp;
    uint8_t checksum;
} mesh_sensor_packet_t;

// Node Information
typedef struct {
    uint8_t mac_addr[6];
    uint8_t node_id;
    uint8_t node_type;
    uint8_t state;
    int8_t rssi;
    uint16_t last_sequence;
    uint32_t last_seen;
    uint32_t packets_received;
    uint32_t packets_sent;
    float x_position;
    float y_position;
    float z_position;
} mesh_node_info_t;

// Mesh Statistics
typedef struct {
    uint32_t total_nodes;
    uint32_t active_nodes;
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t packets_lost;
    uint32_t retransmissions;
    float average_rssi;
    uint32_t network_uptime;
} mesh_stats_t;

// Mesh Configuration
typedef struct {
    uint8_t channel;
    uint8_t max_nodes;
    uint8_t node_type;
    uint16_t heartbeat_interval;
    bool encryption_enabled;
    bool auto_discovery;
    void (*on_node_joined)(const mesh_node_info_t *node);
    void (*on_node_left)(const mesh_node_info_t *node);
    void (*on_audio_data)(const mesh_audio_packet_t *packet);
    void (*on_sensor_data)(const mesh_sensor_packet_t *packet);
} mesh_config_t;

// Mesh API Functions

/**
 * @brief Initialize ESP-NOW mesh networking
 * @param config Mesh configuration structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mesh_audio_init(const mesh_config_t *config);

/**
 * @brief Deinitialize mesh networking
 * @return ESP_OK on success
 */
esp_err_t mesh_audio_deinit(void);

/**
 * @brief Start mesh networking (begin discovery or coordinator role)
 * @return ESP_OK on success
 */
esp_err_t mesh_audio_start(void);

/**
 * @brief Stop mesh networking
 * @return ESP_OK on success
 */
esp_err_t mesh_audio_stop(void);

/**
 * @brief Send audio data to the mesh network
 * @param packet Audio data packet
 * @return ESP_OK on success
 */
esp_err_t mesh_audio_send_audio_data(const mesh_audio_packet_t *packet);

/**
 * @brief Send sensor data to the mesh network
 * @param packet Sensor data packet
 * @return ESP_OK on success
 */
esp_err_t mesh_audio_send_sensor_data(const mesh_sensor_packet_t *packet);

/**
 * @brief Broadcast message to all nodes
 * @param data Message data
 * @param length Message length
 * @param msg_type Message type
 * @return ESP_OK on success
 */
esp_err_t mesh_audio_broadcast(const uint8_t *data, size_t length, uint8_t msg_type);

/**
 * @brief Send message to specific node
 * @param node_id Target node ID
 * @param data Message data
 * @param length Message length
 * @param msg_type Message type
 * @return ESP_OK on success
 */
esp_err_t mesh_audio_send_to_node(uint8_t node_id, const uint8_t *data, 
                                  size_t length, uint8_t msg_type);

/**
 * @brief Get list of active nodes
 * @param nodes Array to store node information
 * @param max_nodes Maximum number of nodes to return
 * @param count Output: actual number of nodes returned
 * @return ESP_OK on success
 */
esp_err_t mesh_audio_get_nodes(mesh_node_info_t *nodes, uint8_t max_nodes, uint8_t *count);

/**
 * @brief Get mesh network statistics
 * @param stats Output statistics structure
 * @return ESP_OK on success
 */
esp_err_t mesh_audio_get_stats(mesh_stats_t *stats);

/**
 * @brief Set node position for triangulation
 * @param x X coordinate (meters)
 * @param y Y coordinate (meters)
 * @param z Z coordinate (meters)
 * @return ESP_OK on success
 */
esp_err_t mesh_audio_set_position(float x, float y, float z);

/**
 * @brief Get current node ID
 * @return Node ID (0 if not initialized)
 */
uint8_t mesh_audio_get_node_id(void);

/**
 * @brief Get current mesh state
 * @return Current state
 */
uint8_t mesh_audio_get_state(void);

/**
 * @brief Force node discovery scan
 * @return ESP_OK on success
 */
esp_err_t mesh_audio_discover_nodes(void);

/**
 * @brief Synchronize time across mesh network
 * @param master_time Reference timestamp (microseconds)
 * @return ESP_OK on success
 */
esp_err_t mesh_audio_sync_time(uint64_t master_time);

/**
 * @brief Get synchronized network time
 * @return Synchronized timestamp in microseconds
 */
uint64_t mesh_audio_get_sync_time(void);

/**
 * @brief Calculate distance between nodes using RSSI
 * @param node_id Target node ID
 * @param distance Output: estimated distance in meters
 * @return ESP_OK on success
 */
esp_err_t mesh_audio_estimate_distance(uint8_t node_id, float *distance);

/**
 * @brief Set mesh encryption key
 * @param key 16-byte encryption key
 * @return ESP_OK on success
 */
esp_err_t mesh_audio_set_encryption_key(const uint8_t *key);

/**
 * @brief Enable/disable mesh debugging
 * @param enabled True to enable debug output
 */
void mesh_audio_set_debug(bool enabled);

/**
 * @brief Get node information by MAC address
 * @param mac_addr MAC address to search for
 * @param node_info Output node information
 * @return ESP_OK if found, ESP_ERR_NOT_FOUND if not found
 */
esp_err_t mesh_audio_get_node_by_mac(const uint8_t *mac_addr, mesh_node_info_t *node_info);

/**
 * @brief Remove node from mesh network
 * @param node_id Node ID to remove
 * @return ESP_OK on success
 */
esp_err_t mesh_audio_remove_node(uint8_t node_id);

/**
 * @brief Calculate mesh network topology
 * @param topology Output: 2D array of connections [node_a][node_b] = connection_strength
 * @param max_nodes Maximum nodes to analyze
 * @return ESP_OK on success
 */
esp_err_t mesh_audio_get_topology(float topology[][MESH_MAX_NODES], uint8_t max_nodes);

// Utility Functions

/**
 * @brief Calculate checksum for packet data
 * @param data Packet data
 * @param length Data length
 * @return Checksum value
 */
uint8_t mesh_audio_calculate_checksum(const uint8_t *data, size_t length);

/**
 * @brief Verify packet checksum
 * @param data Packet data
 * @param length Data length
 * @param checksum Expected checksum
 * @return True if checksum is valid
 */
bool mesh_audio_verify_checksum(const uint8_t *data, size_t length, uint8_t checksum);

/**
 * @brief Convert RSSI to estimated distance
 * @param rssi RSSI value in dBm
 * @return Estimated distance in meters
 */
float mesh_audio_rssi_to_distance(int8_t rssi);

#ifdef __cplusplus
}
#endif