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
#include "DateTime.hpp"
#include "MainMenu.hpp"
#include "format.h"
#include "gui.hpp"
#include "loader.hpp"
#include "pksmbridge_api.h"
#include <arpa/inet.h>

bool saveFromBridge = false;
struct in_addr lastIPAddr = {0};

bool isLoadedSaveFromBridge(void)
{
    return saveFromBridge;
}
void setLoadedSaveFromBridge(bool v)
{
    saveFromBridge = v;
}

bool receiveSaveFromBridge(void)
{
    BridgeScreen screen(true, &lastIPAddr);
    if (Gui::runScreen(screen)) {
        saveFromBridge = true;
        Gui::setScreen(std::make_unique<MainMenu>());
        return true;
    }
    return false;
}

bool sendSaveToBridge(void)
{
    BridgeScreen screen(false, &lastIPAddr);
    return Gui::runScreen(screen);
}

void backupBridgeChanges()
{
    DateTime now = DateTime::now();
    std::string path =
        fmt::format(FMT_STRING("/3ds/PKSM/backups/bridge/{0:d}-{1:d}-{2:d}_{3:d}-{4:d}-{5:d}.bak"),
            now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    FILE* out = fopen(path.c_str(), "wb");
    if (out)
    {
        fwrite(TitleLoader::save->rawData().get(), 1, TitleLoader::save->getLength(), out);
        fclose(out);
    }
}
