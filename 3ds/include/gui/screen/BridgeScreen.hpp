/*
 *   This file is part of PKSM
 *   Copyright (C) 2016-2021 Bernardo Giordano, Admiral Fish, piepie62
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

#ifndef BRIDGESCREEN_HPP
#define BRIDGESCREEN_HPP

#include "RunnableScreen.hpp"
#include "Configuration.hpp"
#include "enums/Language.hpp"
#include "gui.hpp"
#include "pksmbridge_tcp.h"
#include "Sav.hpp"
#include "loader.hpp"
#include "utils/crypto.hpp"

#include <sys/socket.h>
#include <unistd.h>

class BridgeScreen : public RunnableScreen<bool>
{
public:
    BridgeScreen(bool receive, struct in_addr* returnAddress, pksm::Language lang = pksm::Language::ENG)
        : RunnableScreen(true), receive(receive), returnAddress(returnAddress), lang(lang)
    {
        if (receive) {
            abortIfFailed(startListeningForFileReceive(PKSM_PORT, &sockfd, &socketAddress));
        }
        else {
            bridgeFile.size = TitleLoader::save->getLength();
            bridgeFile.contents = TitleLoader::save->rawData().get();
            std::array<u8, 32> checksum = pksm::crypto::sha256(bridgeFile.contents, bridgeFile.size);
            bridgeFile.checksumSize = checksum.size();
            bridgeFile.checksum = new u8[bridgeFile.checksumSize];
            memcpy(bridgeFile.checksum, checksum.data(), bridgeFile.checksumSize);

            connected = true;
            abortIfFailed(initializeSendConnection(PKSM_PORT, *returnAddress, &connfd));
        }
    }
    void drawTop() const override;
    void drawBottom() const override;
    void update(touchPosition* touch) override;

private:
    bool receive;
    struct in_addr* returnAddress;
    pksm::Language lang;

    bool connected = false;
    bool processedMetadata = false;
    size_t current = 0;
    int sockfd = -1;
    int connfd = -1;
    struct sockaddr_in socketAddress;
    struct pksmBridgeFile bridgeFile;

    void abort() {
        if (connected) {
            close(connfd);
        }
        else {
            close(sockfd);
        }
        if (!receive) {
            delete[] bridgeFile.checksum;
        }
        finalValue = false;
        done = true;
    }

    bool abortIfFailed(enum pksmBridgeError error) {
        if (error != PKSM_BRIDGE_ERROR_NONE) {
            switch (error)
            {
                case PKSM_BRIDGE_ERROR_UNSUPPORTED_PROTCOL_VERSION:
                    Gui::error(i18n::localize("BRIDGE_ERROR_UNSUPPORTED_PROTOCOL_VERISON"), errno);
                    break;
                case PKSM_BRIDGE_ERROR_CONNECTION_ERROR:
                    Gui::error(i18n::localize("SOCKET_CONNECTION_FAIL"), errno);
                    break;
                case PKSM_BRIDGE_DATA_READ_FAILURE:
                    Gui::error(i18n::localize("DATA_RECEIVE_FAIL"), errno);
                    break;
                case PKSM_BRIDGE_DATA_WRITE_FAILURE:
                    Gui::error(i18n::localize("DATA_SEND_FAIL"), errno);
                    break;
                case PKSM_BRIDGE_DATA_FILE_CORRUPTED:
                    Gui::error(i18n::localize("BRIDGE_ERROR_FILE_DATA_CORRUPTED"), errno);
                    break;
                case PKSM_BRIDGE_ERROR_UNEXPECTED_MESSAGE:
                    Gui::error(i18n::localize("BRIDGE_ERROR_UNEXPECTED_MESSAGE"), errno);
                    break;
                default:
                    Gui::error(i18n::localize("BRIDGE_ERROR_UNHANDLED"), errno);
                    break;
            }

            abort();
            return true;
        }
        return false;
    }
};

#endif
