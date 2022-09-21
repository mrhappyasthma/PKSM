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

#include "BridgeScreen.hpp"
#include "Configuration.hpp"
#include "format.h"
#include "gui.hpp"
#include "loader.hpp"
#include <unistd.h>

namespace
{
    char* getHostId()
    {
        static sockaddr_in addr;
        addr.sin_addr.s_addr = gethostid();
        return inet_ntoa(addr.sin_addr);
    }

    bool verifyPKSMBridgeFileSHA256Checksum(struct pksmBridgeFile file)
    {
        uint32_t expectedSHA256ChecksumSize = 32;
        if (file.checksumSize != expectedSHA256ChecksumSize)
        {
            return false;
        }
        std::array<u8, 32> checksum = pksm::crypto::sha256(file.contents, file.size);
        int result                  = memcmp(checksum.data(), file.checksum, file.checksumSize);
        free(file.checksum);
        return (result == 0) ? true : false;
    }
}

void BridgeScreen::drawTop() const
{
    if (receive) {
        if (!connected) {
            Gui::sprite(ui_sheet_part_info_top_idx, 0, 0);
            auto parsed   = Gui::parseText("Waiting for connection...\n" + fmt::format(i18n::localize("WIRELESS_IP"), getHostId()) + "\n(Press B to cancel.)", FONT_SIZE_15);

            Gui::text(parsed, 200, 110, FONT_SIZE_15, FONT_SIZE_15,
                PKSM_Color(255, 255, 255, Gui::transparencyWaver()), TextPosX::CENTER, TextPosY::CENTER);
        }
        else if (!processedMetadata) {
            Gui::sprite(ui_sheet_part_info_top_idx, 0, 0);
            auto parsed   = Gui::parseText("Waiting for save metadata...\n(Press B to cancel.)", FONT_SIZE_15);

            Gui::text(parsed, 200, 110, FONT_SIZE_15, FONT_SIZE_15,
                PKSM_Color(255, 255, 255, Gui::transparencyWaver()), TextPosX::CENTER, TextPosY::CENTER);
        }
        else {
            Gui::showDownloadProgress("the save. (Press B to cancel.)", current, bridgeFile.size);
        }
    }
    else {
        if (!processedMetadata) {
            Gui::sprite(ui_sheet_part_info_top_idx, 0, 0);
            auto parsed   = Gui::parseText("Waiting to send save metadata...\n(Press B to cancel.)", FONT_SIZE_15);

            Gui::text(parsed, 200, 110, FONT_SIZE_15, FONT_SIZE_15,
                PKSM_Color(255, 255, 255, Gui::transparencyWaver()), TextPosX::CENTER, TextPosY::CENTER);
        }
        else {
            Gui::showDownloadProgress("the save to the source. (Press B to cancel.)", current, bridgeFile.size);\
        }
    }
}

void BridgeScreen::drawBottom() const
{
    Gui::sprite(ui_sheet_part_info_bottom_idx, 0, 0);
}

void BridgeScreen::update(touchPosition* touch)
{
    if (hidKeysDown() & KEY_B) {
        abort();
        return;
    }
    if (receive) {
        if (!connected) {
            if (abortIfFailed(checkForFileReceiveConnection(sockfd, &socketAddress, &connfd))) {
                return;
            }
            else if (connfd >= 0) {
                connected = true;
            }
        }
        else {
            if (!processedMetadata) {
                if (abortIfFailed(receiveFileMetadata(connfd, &bridgeFile))) {
                    return;
                }
                processedMetadata = true;
                bridgeFile.contents = new u8[bridgeFile.size];
            }
            else {
                if (current != bridgeFile.size) {
                    size_t sizeToReceive = std::min<size_t>(0x3000, bridgeFile.size - current);
                    if (abortIfFailed(receiveFileSegment(connfd, bridgeFile.contents, current, sizeToReceive))) {
                        return;
                    }
                    current += sizeToReceive;
                }
                else {
                    receiveClosure(connfd, socketAddress, returnAddress);

                    if (verifyPKSMBridgeFileSHA256Checksum(bridgeFile)) {
                        if (TitleLoader::load(std::shared_ptr<u8[]>(bridgeFile.contents, free), bridgeFile.size)) {
                            done = true;
                            return;
                        }
                    }
                    Gui::error(i18n::localize("BRIDGE_ERROR_FILE_DATA_CORRUPTED"), 2);
                    return;
                }
            }
        }
    }
    else {
        if (!processedMetadata) {
            if (abortIfFailed(sendPKSMBridgeFileMetadataToSocket(connfd, bridgeFile))) {
                return;
            }
            processedMetadata = true;
        }
        else {
            if (current != bridgeFile.size) {
                size_t sizeToSend = std::min<size_t>(0x3000, bridgeFile.size - current);
                if (abortIfFailed(sendFileSegment(connfd, bridgeFile.contents, current, sizeToSend))) {
                    return;
                }
                current += sizeToSend;
            }
            else {
                sendClosure(connfd);
                done = true;
            }
        }
    }
}
