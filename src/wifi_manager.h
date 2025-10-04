#pragma once

/**
 * @brief Initialize Wi-Fi in station mode and connect to the configured network
 * 
 * This will:
 *  - Initialize TCP/IP stack
 *  - Start Wi-Fi in STA mode
 *  - Automatically reconnect if disconnected
 *  - Print IP address once connected
 */
void wifi_init_sta(void);
