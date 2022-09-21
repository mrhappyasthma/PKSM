/*
 *   This file is part of PKSM
 *   Copyright (C) 2016-2021 mrhappyasthma, Flagbrew
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
 *       * Requiring preservation of specified reasonable legal notices or
 *         author attributions in that material or in the Appropriate Legal
 *         Notices displayed by works containing it.
 *       * Prohibiting misrepresentation of the origin of that material,
 *         or requiring that modified versions of such material be marked in
 *         reasonable ways as different from the original version.
 */

#ifndef PKSMBRIDGE_TCP_H
#define PKSMBRIDGE_TCP_H

#include "pksmbridge_api.h"
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Returns a boolean indicating whether or not the `version` is supported by this PKSM Bridge
 * TCP implementation.
 */
bool checkSupportedPKSMBridgeProtocolVersionForTCP(int version);

/**
 * @brief Connects to a host and verifies that it uses a valid protocol.
 * 
 * @param port Port to bind to.
 * @param address Address to connect to.
 * @param fdOut Pointer to set output socket file descriptor to.
 */
enum pksmBridgeError initializeSendConnection(uint16_t port,
    struct in_addr address, int* fdOut);

/**
 * @brief Checks if the file descriptor is ready to written to. The utility of this function is unknown.
 * 
 * @param fdconn Connection file descriptor.
 * @param timeout Millisecond timeout for data readiness check. Gets cut short if there is.
 * @param readyOut Pointer to store whether the connection has data ready to be read.
 */
enum pksmBridgeError writeReady(int fdconn, int timeout, bool* readyOut);

/**
 * @brief Sends save file length, hash, and hash length.
 * 
 * @param fdconn Connection file descriptor.
 * @param file File struct containing relevant metadata.
 */
enum pksmBridgeError sendPKSMBridgeFileMetadataToSocket(int fdconn, struct pksmBridgeFile file);

/**
 * @brief Sends save file segment. Blocks until complete
 * 
 * @param fdconn Connection file descriptor.
 * @param buffer Buffer to load bytes from. Not freed upon failure.
 * @param position Position within buffer to send data from.
 * @param size Exact amount of data to send in bytes.
 * @return PKSM_BRIDGE_DATA_WRITE_FAILURE if an error, including size sent mismatch, occurs. PKSM_BRIDGE_ERROR_NONE otherwise.
 */
enum pksmBridgeError sendFileSegment(int fdconn, uint8_t* buffer, size_t position, size_t size);

/**
 * @brief Performs connection finalization and closure. (Currently only closure.)
 * 
 * @param fdconn Connection file descriptor.
 */
enum pksmBridgeError sendClosure(int fdconn);

/**
 * @brief Registers a non-blocking socket to listen for a connection to a port.
 * 
 * @param port Port to bind to.
 * @param fdOut Pointer to store output socket file descriptor to.
 * @param servaddrOut Pointer to store socket address information to.
 */
enum pksmBridgeError startListeningForFileReceive(uint16_t port, int* fdOut,
    struct sockaddr_in* servaddrOut);

/**
 * @brief Checks if there's a connection waiting to be accepted on a given socket, and if so sets the connection to be blocking and checks if it is from a supported application
 * 
 * @param fd Socket file descriptor.
 * @param servaddr Pointer to address information. Gets written over.
 * @param fdconnOut Pointer to store connection file descriptor to. Negative if there's no connection to be accepted, whether because of another error or not. In the case of an error generated when making the socket blocking, the stored value is invalid.
 */
enum pksmBridgeError checkForFileReceiveConnection(int fd, struct sockaddr_in* servaddr, int* fdconnOut);

/**
 * @brief Checks if there's data ready to be read on a file descriptor. The utility of this function is unknown.
 * 
 * @param fdconn Connection file descriptor.
 * @param timeout Millisecond timeout for data readiness check. Gets cut short if there is.
 * @param readyOut Pointer to store whether the connection has data ready to be read.
 */
enum pksmBridgeError readReady(int fdconn, int timeout, bool* readyOut);

/**
 * @brief Receives save file length, hash, and hash length.
 * 
 * @param fdconn Connection file descriptor.
 * @param file A pointer to a pre-existing struct to write the metadata into.
 */
enum pksmBridgeError receiveFileMetadata(int fdconn, struct pksmBridgeFile* file);

/**
 * @brief Receives save file segment. Blocks until complete.
 * 
 * @param fdconn Connection file descriptor.
 * @param buffer Buffer to load bytes into. Not freed upon failure.
 * @param position Position within buffer to place data in.
 * @param size Exact amount of data to receive in bytes.
 * @return PKSM_BRIDGE_DATA_READ_FAILURE if an error, including size receipt mismatch, occurs. PKSM_BRIDGE_ERROR_NONE otherwise.
 */
enum pksmBridgeError receiveFileSegment(int fdconn, uint8_t* buffer, size_t position, size_t size);

/**
 * @brief Performs connection closure and retrieves return address
 * 
 * @param fdconn Connection file descriptor. Gets closed by this function.
 * @param servaddr Connection address information.
 * @param outAddress Pointer to store the return IP address to.
 */
enum pksmBridgeError receiveClosure(int fdconn, struct sockaddr_in servaddr,
    struct in_addr* outAddress);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // PKSMBRIDGE_TCP_H