/*
 *   This file is part of PKSM
 *   Copyright (C) 2016-2019 Bernardo Giordano, Admiral Fish, piepie62
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

#include "CloudAccess.hpp"
#include "PK7.hpp"
#include "base64.hpp"
#include "fetch.hpp"
#include "thread.hpp"

static Generation numToGen(int num)
{
    switch (num)
    {
        case 4:
            return Generation::FOUR;
        case 5:
            return Generation::FIVE;
        case 6:
            return Generation::SIX;
        case 7:
            return Generation::SEVEN;
        case 254:
            return Generation::LGPE;
    }
    return Generation::UNUSED;
}

void downloadCloudPage(CloudAccess::PageDownloadInfo* info)
{
    if (info)
    {
        std::string retData;
        auto fetch = Fetch::init(CloudAccess::makeURL(info->number, info->type, info->ascend, info->legal), false, true, &retData, nullptr, "");
        if (fetch && fetch->perform() == CURLE_OK)
        {
            long status_code;
            fetch->getinfo(CURLINFO_RESPONSE_CODE, &status_code);
            switch (status_code)
            {
                case 200:
                    break;
                default:
                    info->page->data = {};
            }
            info->page->data = nlohmann::json::parse(retData, nullptr, false);
        }
        else
        {
            info->page->data = {};
        }
        info->page->available = true;
        delete info;
    }
}

CloudAccess::CloudAccess() : pageNumber(1)
{
    current = std::make_shared<Page>();
    if (current)
    {
        current->data      = grabPage(pageNumber);
        current->available = true;
        isGood             = !current->data.is_discarded() && current->data.size() > 0;
    }
}

void CloudAccess::refreshPages()
{
    current = std::make_shared<Page>();
    if (current)
    {
        current->data      = grabPage(pageNumber);
        current->available = true;
        isGood             = !current->data.is_discarded() && current->data.size() > 0;
    }
}

nlohmann::json CloudAccess::grabPage(int num)
{
    std::string retData;
    auto fetch = Fetch::init(makeURL(num, sort, ascend, legal), false, true, &retData, nullptr, "");
    if (fetch && fetch->perform() == CURLE_OK)
    {
        long status_code;
        fetch->getinfo(CURLINFO_RESPONSE_CODE, &status_code);
        switch (status_code)
        {
            case 200:
                break;
            default:
                return {};
        }
        return nlohmann::json::parse(retData, nullptr, false);
    }
    return {};
}

static std::string sortTypeToString(CloudAccess::SortType type)
{
    switch (type)
    {
        case CloudAccess::SortType::LATEST:
            return "latest";
        case CloudAccess::SortType::POPULAR:
            return "popular";
    }
    return "";
}

std::string CloudAccess::makeURL(int num, SortType type, bool ascend, bool legal)
{
    return "https://flagbrew.org/api/v1/gpss/all?pksm=yes&count=30&sort=" + sortTypeToString(type) +
           "&dir=" + (ascend ? std::string("ascend") : std::string("descend")) + "&legal_only=" + (legal ? std::string("yes") : std::string("no")) +
           "&page=" + std::to_string(num);
}

std::shared_ptr<PKX> CloudAccess::pkm(size_t slot) const
{
    if (slot < current->data["results"].size())
    {
        std::string b64Data = current->data["results"][slot]["base_64"].get<std::string>();
        Generation gen      = numToGen(current->data["results"][slot]["generation"].get<int>());
        // Legal info: needs thought
        auto retData = base64_decode(b64Data.data(), b64Data.size());

        size_t targetLength = 0;
        switch (gen)
        {
            case Generation::FOUR:
            case Generation::FIVE:
                targetLength = 138;
                break;
            case Generation::SIX:
            case Generation::SEVEN:
                targetLength = 234;
                break;
            case Generation::LGPE:
                targetLength = 261;
                break;
            default:
                break;
        }
        if (targetLength != retData.size())
        {
            return std::make_shared<PK7>();
        }

        return PKX::getPKM(gen, retData.data());
    }
    return std::make_shared<PK7>();
}

bool CloudAccess::isLegal(size_t slot) const
{
    if (slot < current->data["results"].size())
    {
        return current->data["results"][slot]["pokemon"]["legal"].get<bool>();
    }
    return false;
}

void incrementPkmDownloadCount(std::string* num)
{
    if (num)
    {
        if (auto fetch = Fetch::init("https://flagbrew.org/gpss/download/" + *num, false, true, nullptr, nullptr, ""))
        {
            fetch->perform();
        }
        delete num;
    }
}

std::shared_ptr<PKX> CloudAccess::fetchPkm(size_t slot) const
{
    if (slot < current->data["results"].size())
    {
        std::string b64Data = current->data["results"][slot]["base_64"].get<std::string>();
        Generation gen      = numToGen(current->data["results"][slot]["generation"].get<int>());
        // Legal info: needs thought
        auto retData = base64_decode(b64Data.data(), b64Data.size());

        size_t targetLength = 0;
        switch (gen)
        {
            case Generation::FOUR:
            case Generation::FIVE:
                targetLength = 138;
                break;
            case Generation::SIX:
            case Generation::SEVEN:
                targetLength = 234;
                break;
            case Generation::LGPE:
                targetLength = 261;
                break;
            default:
                break;
        }
        if (targetLength != retData.size())
        {
            return std::make_shared<PK7>();
        }

        if (auto fetch = Fetch::init(
                "https://flagbrew.org/gpss/download/" + current->data["results"][slot]["code"].get<std::string>(), false, true, nullptr, nullptr, ""))
        {
            fetch->perform();
        }

        return PKX::getPKM(gen, retData.data());
    }
    return std::make_shared<PK7>();
}

void CloudAccess::nextPage()
{
    if (current->data["pages"].get<int>() > 1)
    {
        pageNumber    = (pageNumber % current->data["pages"].get<int>()) + 1;
        current->data = grabPage(pageNumber);
    }
}

void CloudAccess::prevPage()
{
    if (current->data["pages"].get<int>() > 1)
    {
        pageNumber    = pageNumber - 1 == 0 ? current->data["pages"].get<int>() : pageNumber - 1;
        current->data = grabPage(pageNumber);
    }
}

bool CloudAccess::pkm(std::shared_ptr<PKX> mon)
{
    bool ret             = false;
    std::string postdata = base64_encode(mon->rawData(), mon->getLength());
    std::string version  = "Generation: " + genToString(mon->generation());
    std::string size     = "Size: " + std::to_string(mon->getLength());
    std::string info     = "Info: " + mon->nickname() + "," + mon->otName() + "," + std::to_string((int)mon->level()) + "," +
                       std::to_string(mon->species()) + "," + std::to_string(mon->move(0)) + "," + std::to_string(mon->move(1)) + "," +
                       std::to_string(mon->move(2)) + "," + std::to_string(mon->move(3)) + "," + std::to_string((int)mon->nature()) + "," +
                       std::to_string((int)mon->iv(0)) + "," + std::to_string((int)mon->iv(1)) + "," + std::to_string((int)mon->iv(2)) // HP, Atk, Def
                       + "," + std::to_string((int)mon->iv(5)) + "," + std::to_string((int)mon->iv(3)) + "," +
                       std::to_string((int)mon->iv(4)) // Sp. Atk, Sp. Def, Speed
                       + "," + std::to_string((int)mon->gender()) + "," + std::to_string((bool)mon->shiny()) + "," +
                       std::to_string((int)mon->ability()) + "," + std::to_string((int)mon->heldItem()) + "," + std::to_string((int)mon->TID()) +
                       "," + std::to_string((int)mon->ball()) + "," + std::to_string((int)mon->pkrs());
    struct curl_slist* headers = NULL;
    headers                    = curl_slist_append(headers, "Content-Type: application/base64");
    headers                    = curl_slist_append(headers, version.c_str());
    headers                    = curl_slist_append(headers, size.c_str());
    headers                    = curl_slist_append(headers, info.c_str());

    std::string writeData = "";
    if (auto fetch = Fetch::init("https://flagbrew.org/gpss/share", true, true, &writeData, headers, postdata))
    {
        CURLcode res = fetch->perform();
        if (res == CURLE_OK)
        {
            long status_code;
            fetch->getinfo(CURLINFO_RESPONSE_CODE, &status_code);
            switch (status_code)
            {
                case 201:
                    refreshPages();
                case 200:
                    ret = true;
                    break;
                default:
                    break;
            }
        }
    }
    curl_slist_free_all(headers);
    return ret;
}
