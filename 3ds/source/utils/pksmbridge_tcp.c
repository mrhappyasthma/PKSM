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

#include "pksmbridge_tcp.h"
#include "pksmbridge_api.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/** Creates a socket and returns the file descriptor. If an error occurs, returns -1. */
static int createSocket(void)
{
    return socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
}

/** Creates a socket address with the provided port and address. */
static struct sockaddr_in createSocketAddress(uint16_t port, in_addr_t address)
{
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_port        = htons(port);
    servaddr.sin_addr.s_addr = address;
    return servaddr;
}

/** Sends chunks of `data` from a buffer to a socket. Returns the number of sent bytes. */
static int sendDataFromBufferToSocket(int sockfd, void* buffer, size_t size)
{
    size_t total = 0;
    size_t chunk = 1024;
    while (total < size)
    {
        size_t tosend = size - total > chunk ? chunk : size - total;
        int n         = send(sockfd, ((char*)buffer) + total, tosend, 0);
        if (n == -1)
        {
            break;
        }
        total += n;
    }
    return total;
}

/**
 * Reads chunks of data from a socket into the provided `buffer`. Returns the
 * number of read bytes.
 */
static int receiveDataFromSocketIntoBuffer(int sockfd, void* buffer, size_t size)
{
    size_t total = 0;
    size_t chunk = 1024;
    while (total < size)
    {
        size_t torecv = size - total > chunk ? chunk : size - total;
        int n         = recv(sockfd, ((char*)buffer) + total, torecv, 0);
        if (n <= 0)
        {
            break;
        }
        total += n;
    }
    return total;
}

/** Expects the `protocol_name` field from either pksmBridgeRequest or pksmBridgeResponse. */
static bool verifyPKSMBridgeHeader(char protocol_name[10])
{
    int result = strncmp(protocol_name, PKSM_BRIDGE_PROTOCOL_NAME, 10);
    return (result == 0) ? true : false;
}

/** Checks if the specified action is ready to be taken by the input connection file
 * descriptor in the provided timeout.
 */
static enum pksmBridgeError actionReady(int fdconn, int timeout, bool* readyOut, short action)
{
    struct pollfd fdEvents {
        .fd = fdconn,
        .events = action,
        .revents = 0
    };
    if (poll(&fdEvents, 1 /* number of fds to track */, timeout) < 0) {
        close(fdconn);
        return PKSM_BRIDGE_ERROR_CONNECTION_ERROR;
    }

    *readyOut = fdEvents.revents & action;
    return PKSM_BRIDGE_ERROR_NONE;
}

bool checkSupportedPKSMBridgeProtocolVersionForTCP(int version)
{
    // This logic should be updated if newer protocol versions are introduced.
    // For now, there is only a single protocol version.
    return (version == 1);
}

enum pksmBridgeError initializeSendConnection(uint16_t port,
    struct in_addr address, int* fdOut)
{
    int fd = createSocket();
    if (fd < 0)
    {
        return PKSM_BRIDGE_ERROR_CONNECTION_ERROR;
    }

    struct sockaddr_in servaddr = createSocketAddress(port, address.s_addr);
    if (connect(fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
    {
        close(fd);
        return PKSM_BRIDGE_ERROR_CONNECTION_ERROR;
    }

    // Expect a pksmBridgeRequest, which specifies the protocol version.
    struct pksmBridgeRequest request;
    int bytesRead = receiveDataFromSocketIntoBuffer(fd, &request, sizeof(request));
    if (bytesRead != sizeof(request))
    {
        close(fd);
        return PKSM_BRIDGE_DATA_READ_FAILURE;
    }
    if (!verifyPKSMBridgeHeader(request.protocol_name))
    {
        close(fd);
        return PKSM_BRIDGE_ERROR_UNEXPECTED_MESSAGE;
    }

    // Construct and send the pksmBridgeResponse.
    struct pksmBridgeResponse response =
        createPKSMBridgeResponseForRequest(request, &checkSupportedPKSMBridgeProtocolVersionForTCP);
    int sentBytes = sendDataFromBufferToSocket(fd, &response, sizeof(response));
    if (sentBytes != sizeof(response))
    {
        close(fd);
        return PKSM_BRIDGE_DATA_WRITE_FAILURE;
    }
    if (response.protocol_version == PKSM_BRIDGE_UNSUPPORTED_PROTOCOL_VERSION)
    {
        close(fd);
        return PKSM_BRIDGE_ERROR_UNSUPPORTED_PROTCOL_VERSION;
    }

    *fdOut = fd;
    return PKSM_BRIDGE_ERROR_NONE;
}

enum pksmBridgeError writeReady(int fdconn, int timeout, bool* readyOut)
{
    return actionReady(fdconn, timeout, readyOut, POLLOUT);
}

enum pksmBridgeError sendPKSMBridgeFileMetadataToSocket(int fdconn, struct pksmBridgeFile file)
{
    uint32_t sentBytes =
        sendDataFromBufferToSocket(fdconn, &file.checksumSize, sizeof(file.checksumSize));
    if (sentBytes != sizeof(file.checksumSize))
    {
        close(fdconn);
        return PKSM_BRIDGE_DATA_WRITE_FAILURE;
    }
    sentBytes = sendDataFromBufferToSocket(fdconn, file.checksum, file.checksumSize);
    if (sentBytes != file.checksumSize)
    {
        close(fdconn);
        return PKSM_BRIDGE_DATA_WRITE_FAILURE;
    }
    sentBytes = sendDataFromBufferToSocket(fdconn, &file.size, sizeof(file.size));
    if (sentBytes != sizeof(file.size))
    {
        close(fdconn);
        return PKSM_BRIDGE_DATA_WRITE_FAILURE;
    }
    return PKSM_BRIDGE_ERROR_NONE;
}

enum pksmBridgeError sendFileSegment(int fdconn, uint8_t* buffer, size_t position, size_t size)
{
    if (sendDataFromBufferToSocket(fdconn, buffer + position, size) != size) {
        close(fdconn);
        return PKSM_BRIDGE_DATA_WRITE_FAILURE;
    }
    return PKSM_BRIDGE_ERROR_NONE;
}

enum pksmBridgeError fileSendFinalization(int fdconn)
{
    close(fdconn);
    return PKSM_BRIDGE_ERROR_NONE;
}

enum pksmBridgeError startListeningForFileReceive(uint16_t port, int* fdOut,
    struct sockaddr_in* servaddrOut)
{
    int fd = createSocket();
    if (fd < 0)
    {
        return PKSM_BRIDGE_ERROR_CONNECTION_ERROR;
    }
    // makes sure the socket is non-blocking
    if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) < 0)
    {
        close(fd);
        return PKSM_BRIDGE_ERROR_CONNECTION_ERROR;
    }

    struct sockaddr_in servaddr = createSocketAddress(port, INADDR_ANY);
    if (bind(fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
    {
        close(fd);
        return PKSM_BRIDGE_ERROR_CONNECTION_ERROR;
    }
    if (listen(fd, /*backlog=*/1) < 0)
    {
        close(fd);
        return PKSM_BRIDGE_ERROR_CONNECTION_ERROR;
    }

    *fdOut = fd;
    *servaddrOut = servaddr;
    return PKSM_BRIDGE_ERROR_NONE;
}

enum pksmBridgeError checkForFileReceiveConnection(int fd, struct sockaddr_in* servaddr, int* fdconnOut)
{
    int addrlen = sizeof(*servaddr);
    int fdconn = accept(fd, (struct sockaddr*)servaddr, (socklen_t*)&addrlen);

    if (errno == EAGAIN || errno == EWOULDBLOCK)
    {
        *fdconnOut = fdconn;
        return PKSM_BRIDGE_ERROR_NONE;
    }
    if (fdconn < 0)
    {
        close(fd);
        return PKSM_BRIDGE_ERROR_CONNECTION_ERROR;
    }

    close(fd);
    // makes sure the socket is blocking. we actually want to have it be blocking for simplicity
    if (fcntl(fdconn, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK) < 0) {
        close(fdconn);
        return PKSM_BRIDGE_ERROR_CONNECTION_ERROR;
    }

    // Send a pksmBridgeRequest, requesting a specific protocol version.
    struct pksmBridgeRequest request = createPKSMBridgeRequest(PKSM_BRIDGE_LATEST_PROTOCOL_VERSION);
    uint32_t sentBytes = sendDataFromBufferToSocket(fdconn, &request, sizeof(request));
    if (sentBytes != sizeof(request))
    {
        close(fdconn);
        return PKSM_BRIDGE_DATA_WRITE_FAILURE;
    }

    // Expect a pksmBridgeResponse and see if the protocol version was supported.
    struct pksmBridgeResponse response;
    uint32_t readBytes = receiveDataFromSocketIntoBuffer(fdconn, &response, sizeof(response));
    if (readBytes != sizeof(response))
    {
        close(fdconn);
        return PKSM_BRIDGE_DATA_READ_FAILURE;
    }
    if (!verifyPKSMBridgeHeader(response.protocol_name))
    {
        close(fdconn);
        return PKSM_BRIDGE_ERROR_UNEXPECTED_MESSAGE;
    }
    if (response.protocol_version == PKSM_BRIDGE_UNSUPPORTED_PROTOCOL_VERSION)
    {
        close(fdconn);
        return PKSM_BRIDGE_ERROR_UNSUPPORTED_PROTCOL_VERSION;
    }

    *fdconnOut = fdconn;
    return PKSM_BRIDGE_ERROR_NONE;
}

enum pksmBridgeError readReady(int fdconn, int timeout, bool* readyOut)
{
    return actionReady(fdconn, timeout, readyOut, POLLIN);
}

enum pksmBridgeError receiveFileMetadata(int fdconn, uint32_t* checksumSizeOut, uint8_t** checksumOut, uint32_t* fileSizeOut)
{
    uint32_t checksumSize;
    uint32_t bytesRead =
        receiveDataFromSocketIntoBuffer(fdconn, &checksumSize, sizeof(checksumSize));
    if (bytesRead != sizeof(checksumSize))
    {
        close(fdconn);
        return PKSM_BRIDGE_DATA_READ_FAILURE;
    }
    uint8_t* checksumBuffer = (uint8_t*)malloc(checksumSize);
    bytesRead               = receiveDataFromSocketIntoBuffer(fdconn, checksumBuffer, checksumSize);
    if (bytesRead != checksumSize)
    {
        close(fdconn);
        free(checksumBuffer);
        return PKSM_BRIDGE_DATA_READ_FAILURE;
    }
    uint32_t fileSize;
    bytesRead = receiveDataFromSocketIntoBuffer(fdconn, &fileSize, sizeof(fileSize));
    if (bytesRead != sizeof(fileSize))
    {
        close(fdconn);
        free(checksumBuffer);
        return PKSM_BRIDGE_DATA_READ_FAILURE;
    }

    *checksumSizeOut = checksumSize;
    *checksumOut = checksumBuffer;
    *fileSizeOut = fileSize;
    return PKSM_BRIDGE_ERROR_NONE;
}

enum pksmBridgeError receiveFileSegment(int fdconn, uint8_t* buffer, size_t position, size_t size)
{
    if (receiveDataFromSocketIntoBuffer(fdconn, buffer + position, size) != size) {
        close(fdconn);
        return PKSM_BRIDGE_DATA_READ_FAILURE;
    }
    return PKSM_BRIDGE_ERROR_NONE;
}

enum pksmBridgeError fileReceiptFinalization(int fdconn, struct sockaddr_in servaddr,
    struct in_addr* outAddress, struct pksmBridgeFile fileStruct, bool (*validateChecksumFunc)(struct pksmBridgeFile))
{
    close(fdconn);
    *outAddress = servaddr.sin_addr;
    struct pksmBridgeFile file = {
        .checksumSize = checksumSize,
        .checksum = checksum,
        .size = saveSize,
        .contents = save
    };
    return validateChecksumFunc(file) ? PKSM_BRIDGE_ERROR_NONE : PKSM_BRIDGE_DATA_FILE_CORRUPTED;
}
