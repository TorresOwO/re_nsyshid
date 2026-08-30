#include "skylanderendpoints.h"
#include "../devices/Skylander.h"
#include "../utils/FSUtils.hpp"
#include "web_ui.h"

#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static void ensureSkylandersDirectory() {
    mkdir("/vol/external01/wiiu/re_nsyshid", 0777);
    mkdir("/vol/external01/wiiu/re_nsyshid/Skylanders", 0777);
}

static std::string sanitizeFileName(std::string name) {
    for (char &c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    return name;
}

static std::string toLowerString(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char) std::tolower(c); });
    return s;
}

static HttpResponse handleGetStatus(const HttpRequest &req) {
    miniJson::Json::_object ret;
    ret["success"] = true;
    ret["running"] = true;
    ret["message"] = "Connected to Dolphin Portal";

    miniJson::Json::_array slots;
    for (uint8_t i = 0; i < MAX_SKYLANDERS; i++) {
        miniJson::Json::_object slotObj;
        bool occupied     = g_skyportal.IsSlotOccupied(i);
        int8_t portalSlot = g_skyportal.GetPortalSlotFromUISlot(i);

        slotObj["slot"]       = (double) i;
        slotObj["portalSlot"] = (double) portalSlot;
        slotObj["occupied"]   = occupied;

        if (occupied) {
            std::string name = g_skyportal.GetSkylanderFromUISlot(i);
            const auto idvar = g_skyportal.GetSkylanderIdFromUISlot(i);
            uint32_t level = 1, money = 0;
            g_skyportal.GetSkylanderStats(i, level, money);

            slotObj["name"]    = name;
            slotObj["id"]      = (double) idvar.first;
            slotObj["variant"] = (double) idvar.second;
            slotObj["level"]   = (double) level;
            slotObj["money"]   = (double) money;
        }

        slots.push_back(slotObj);
    }
    ret["slots"] = slots;
    return HttpResponse{200, ret};
}

static HttpResponse handleLoad(const HttpRequest &req) {
    miniJson::Json::_object res;
    auto body = req.json();

    if (!body.isObject()) {
        res["success"] = false;
        res["message"] = "Invalid request body";
        return HttpResponse{400, res};
    }
    auto loadRequest = body.toObject();

    // Ranura (por defecto 0)
    uint8_t slot = 0;
    if (loadRequest.count("slot") && loadRequest["slot"].isNumber()) {
        double s = loadRequest["slot"].toDouble();
        if (s >= 0 && s < MAX_SKYLANDERS) {
            slot = (uint8_t) s;
        } else {
            res["success"] = false;
            res["message"] = "Invalid slot index";
            return HttpResponse{400, res};
        }
    }

    ensureSkylandersDirectory();

    std::string filePath   = "";
    std::string figureName = "";

    // Opción A: Por ruta directa de archivo
    if (loadRequest.count("path") && loadRequest["path"].isString()) {
        filePath = loadRequest["path"].toString();
        if (!filePath.starts_with("/vol/")) {
            filePath = "/vol/external01/wiiu/re_nsyshid/" + filePath;
        }
    }
    // Opción B: Por ID y Variante
    else if (loadRequest.count("id") && loadRequest["id"].isNumber()) {
        uint16_t id  = (uint16_t) loadRequest["id"].toDouble();
        uint16_t var = 0;
        if (loadRequest.count("variant") && loadRequest["variant"].isNumber()) {
            var = (uint16_t) loadRequest["variant"].toDouble();
        }
        figureName           = g_skyportal.FindSkylander(id, var);
        std::string safeName = sanitizeFileName(figureName);
        filePath             = "/vol/external01/wiiu/re_nsyshid/Skylanders/" + safeName + ".sky";

        struct stat st;
        if (stat(filePath.c_str(), &st) != 0) {
            g_skyportal.CreateSkylander(filePath, id, var);
        }
    }
    // Opción C: Por Nombre
    else if (loadRequest.count("name") && loadRequest["name"].isString()) {
        std::string targetName = loadRequest["name"].toString();
        uint16_t foundId = 0xFFFF, foundVar = 0;
        bool matched = false;

        auto skylanders = SkylanderPortal::GetListSkylanders();
        // Coincidencia exacta
        for (const auto &[idvar, name] : skylanders) {
            if (targetName == name) {
                foundId    = idvar.first;
                foundVar   = idvar.second;
                matched    = true;
                figureName = name;
                break;
            }
        }
        // Coincidencia sin distinguir mayúsculas/minúsculas
        if (!matched) {
            std::string lowerTarget = toLowerString(targetName);
            for (const auto &[idvar, name] : skylanders) {
                if (toLowerString(name) == lowerTarget) {
                    foundId    = idvar.first;
                    foundVar   = idvar.second;
                    matched    = true;
                    figureName = name;
                    break;
                }
            }
        }

        if (!matched) {
            res["success"] = false;
            res["message"] = "Figure not found in catalog";
            return HttpResponse{404, res};
        }

        std::string safeName = sanitizeFileName(figureName);
        filePath             = "/vol/external01/wiiu/re_nsyshid/Skylanders/" + safeName + ".sky";

        struct stat st;
        if (stat(filePath.c_str(), &st) != 0) {
            g_skyportal.CreateSkylander(filePath, foundId, foundVar);
        }
    } else {
        res["success"] = false;
        res["message"] = "Missing name, id, or path parameter";
        return HttpResponse{400, res};
    }

    // Cargar archivo en la ranura del portal
    std::array<uint8_t, 0x10 * 0x40> fileData;
    int ret_code = FSUtils::ReadFromFile(filePath.c_str(), fileData.data(), fileData.size());
    if (ret_code == (int) fileData.size()) {
        if (!g_skyportal.LoadSkylander(fileData.data(), filePath, slot)) {
            res["success"] = false;
            res["message"] = "Failed to load figure into portal";
            return HttpResponse{500, res};
        }

        if (figureName.empty()) {
            figureName = g_skyportal.GetSkylanderFromUISlot(slot);
        }
        int8_t portalSlot = g_skyportal.GetPortalSlotFromUISlot(slot);

        res["success"]    = true;
        res["slot"]       = (double) slot;
        res["portalSlot"] = (double) portalSlot;
        res["name"]       = figureName;
        res["message"]    = "Skylander " + figureName + " loaded successfully into slot " + std::to_string(slot);
        return HttpResponse{200, res};
    } else {
        res["success"] = false;
        res["message"] = "Failed to read figure file from SD";
        return HttpResponse{400, res};
    }
}

static HttpResponse handleRemove(const HttpRequest &req) {
    miniJson::Json::_object res;
    auto body = req.json();

    if (!body.isObject()) {
        res["success"] = false;
        res["message"] = "Invalid request body";
        return HttpResponse{400, res};
    }
    auto removeRequest = body.toObject();
    uint8_t slot       = 0;
    if (removeRequest.count("slot") && removeRequest["slot"].isNumber()) {
        slot = (uint8_t) removeRequest["slot"].toDouble();
    }

    if (slot >= MAX_SKYLANDERS) {
        res["success"] = false;
        res["message"] = "Invalid slot index";
        return HttpResponse{400, res};
    }

    g_skyportal.RemoveSkylander(slot);
    res["success"] = true;
    res["slot"]    = (double) slot;
    res["message"] = "Slot " + std::to_string(slot) + " cleared";
    return HttpResponse{200, res};
}

static HttpResponse handleClear(const HttpRequest &req) {
    for (uint8_t i = 0; i < MAX_SKYLANDERS; i++) {
        g_skyportal.RemoveSkylander(i);
    }
    miniJson::Json::_object res;
    res["success"] = true;
    res["message"] = "Portal completely cleared";
    return HttpResponse{200, res};
}

static const std::string BASE64_CHARS =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

static std::string base64_encode_bytes(const uint8_t *bytes_to_encode, size_t in_len) {
    std::string ret;
    int i = 0, j = 0;
    uint8_t char_array_3[3], char_array_4[4];

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++)
                ret += BASE64_CHARS[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (j = 0; (j < i + 1); j++)
            ret += BASE64_CHARS[char_array_4[j]];

        while ((i++ < 3))
            ret += '=';
    }

    return ret;
}

static std::vector<uint8_t> base64_decode_bytes(const std::string &encoded_string) {
    size_t in_len = encoded_string.size();
    int i = 0, j = 0, in_ = 0;
    uint8_t char_array_4[4], char_array_3[3];
    std::vector<uint8_t> ret;

    while (in_len-- && (encoded_string[in_] != '=') && (std::isalnum(encoded_string[in_]) || (encoded_string[in_] == '+') || (encoded_string[in_] == '/'))) {
        char_array_4[i++] = encoded_string[in_];
        in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++)
                char_array_4[i] = (uint8_t) BASE64_CHARS.find(char_array_4[i]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
                ret.push_back(char_array_3[i]);
            i = 0;
        }
    }

    if (i) {
        for (j = 0; j < i; j++)
            char_array_4[j] = (uint8_t) BASE64_CHARS.find(char_array_4[j]);

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);

        for (j = 0; (j < i - 1); j++)
            ret.push_back(char_array_3[j]);
    }

    return ret;
}

static std::string decodeUrlString(const std::string &urlEncoded) {
    std::string res;
    for (size_t i = 0; i < urlEncoded.length(); ++i) {
        if (urlEncoded[i] == '%' && i + 2 < urlEncoded.length()) {
            std::string hex = urlEncoded.substr(i + 1, 2);
            char ch = (char) std::strtol(hex.c_str(), nullptr, 16);
            res += ch;
            i += 2;
        } else if (urlEncoded[i] == '+') {
            res += ' ';
        } else {
            res += urlEncoded[i];
        }
    }
    return res;
}

static HttpResponse handleDownload(const HttpRequest &req) {
    std::string query = req.getQuery();

    // 1. Check if slot query parameter is provided (e.g. ?slot=0)
    int slot = -1;
    size_t slotPos = query.find("slot=");
    if (slotPos != std::string::npos) {
        std::string slotStr = query.substr(slotPos + 5);
        size_t amp = slotStr.find('&');
        if (amp != std::string::npos) slotStr = slotStr.substr(0, amp);
        slot = std::atoi(slotStr.c_str());
    }

    bool asJson = (query.find("format=json") != std::string::npos) || (req["Accept"].find("application/json") != std::string::npos);

    if (slot >= 0 && slot < MAX_SKYLANDERS) {
        std::vector<uint8_t> data;
        std::string name, path;
        if (g_skyportal.GetSkylanderRawData((uint8_t) slot, data, name, path) && !data.empty()) {
            std::string safeName = sanitizeFileName(name.empty() ? ("Slot" + std::to_string(slot)) : name);
            if (asJson) {
                miniJson::Json::_object res;
                res["success"]  = true;
                res["slot"]     = (double) slot;
                res["name"]     = name;
                res["filename"] = safeName + ".sky";
                res["data"]     = base64_encode_bytes(data.data(), data.size());
                return HttpResponse{200, res};
            } else {
                std::string body((char *) data.data(), data.size());
                HttpResponse resp{200, "application/octet-stream", body};
                resp["Content-Disposition"]        = "attachment; filename=\"" + safeName + ".sky\"";
                resp["Access-Control-Allow-Origin"] = "*";
                return resp;
            }
        } else {
            miniJson::Json::_object res;
            res["success"] = false;
            res["message"] = "No Skylander figure currently in slot " + std::to_string(slot);
            return HttpResponse{404, res};
        }
    }

    // 2. Check if filename / name / path parameter is provided
    std::string fileParam;
    size_t fnPos = query.find("filename=");
    if (fnPos != std::string::npos) {
        fileParam = query.substr(fnPos + 9);
    } else {
        size_t namePos = query.find("name=");
        if (namePos != std::string::npos) {
            fileParam = query.substr(namePos + 5);
        } else {
            size_t pathPos = query.find("path=");
            if (pathPos != std::string::npos) {
                fileParam = query.substr(pathPos + 5);
            }
        }
    }

    size_t amp = fileParam.find('&');
    if (amp != std::string::npos) fileParam = fileParam.substr(0, amp);

    if (!fileParam.empty()) {
        fileParam = decodeUrlString(fileParam);
        std::string fullPath;
        if (fileParam.rfind("/vol/", 0) == 0) {
            fullPath = fileParam;
        } else {
            if (fileParam.rfind(".sky") == std::string::npos && fileParam.rfind(".bin") == std::string::npos) {
                fileParam += ".sky";
            }
            fullPath = "/vol/external01/wiiu/re_nsyshid/Skylanders/" + sanitizeFileName(fileParam);
        }

        std::array<uint8_t, 1024> fileData;
        int readBytes = FSUtils::ReadFromFile(fullPath.c_str(), fileData.data(), fileData.size());
        if (readBytes > 0) {
            if (asJson) {
                miniJson::Json::_object res;
                res["success"]  = true;
                res["filename"] = sanitizeFileName(fileParam);
                res["data"]     = base64_encode_bytes(fileData.data(), readBytes);
                return HttpResponse{200, res};
            } else {
                std::string body((char *) fileData.data(), readBytes);
                HttpResponse resp{200, "application/octet-stream", body};
                resp["Content-Disposition"]        = "attachment; filename=\"" + sanitizeFileName(fileParam) + "\"";
                resp["Access-Control-Allow-Origin"] = "*";
                return resp;
            }
        } else {
            miniJson::Json::_object res;
            res["success"] = false;
            res["message"] = "File not found: " + fullPath;
            return HttpResponse{404, res};
        }
    }

    miniJson::Json::_object res;
    res["success"] = false;
    res["message"] = "Missing 'slot' or 'filename' query parameter";
    return HttpResponse{400, res};
}

static HttpResponse handleUpload(const HttpRequest &req) {
    ensureSkylandersDirectory();
    miniJson::Json::_object res;

    std::string filename = "uploaded_figure.sky";
    std::vector<uint8_t> fileBytes;
    int targetSlot = -1;
    bool shouldLoad = false;

    // 1. Check for JSON body
    auto bodyJson = req.json();
    if (bodyJson.isObject()) {
        auto obj = bodyJson.toObject();
        if (obj.count("filename") && obj["filename"].isString()) {
            filename = obj["filename"].toString();
        }
        if (obj.count("slot") && obj["slot"].isNumber()) {
            targetSlot = (int) obj["slot"].toDouble();
        }
        if (obj.count("load") && obj["load"].isBool()) {
            shouldLoad = obj["load"].toBool();
        } else if (targetSlot >= 0) {
            shouldLoad = true;
        }

        if (obj.count("data") && obj["data"].isString()) {
            std::string b64 = obj["data"].toString();
            fileBytes       = base64_decode_bytes(b64);
        }
    }

    // 2. Check for Raw binary body if no JSON data
    if (fileBytes.empty() && !req.content().empty()) {
        const std::string &raw = req.content();
        fileBytes.assign(raw.begin(), raw.end());

        std::string query = req.getQuery();
        size_t fnPos = query.find("filename=");
        if (fnPos != std::string::npos) {
            filename = decodeUrlString(query.substr(fnPos + 9));
            size_t amp = filename.find('&');
            if (amp != std::string::npos) filename = filename.substr(0, amp);
        } else if (!req["X-Filename"].empty()) {
            filename = req["X-Filename"];
        }

        size_t slotPos = query.find("slot=");
        if (slotPos != std::string::npos) {
            targetSlot = std::atoi(query.substr(slotPos + 5).c_str());
            shouldLoad = true;
        }
    }

    if (fileBytes.empty()) {
        res["success"] = false;
        res["message"] = "No file data received";
        return HttpResponse{400, res};
    }

    if (filename.rfind(".sky") == std::string::npos && filename.rfind(".bin") == std::string::npos) {
        filename += ".sky";
    }
    std::string safeName = sanitizeFileName(filename);
    std::string fullPath = "/vol/external01/wiiu/re_nsyshid/Skylanders/" + safeName;

    // Pad with zeros to 1024 bytes if needed
    if (fileBytes.size() < 1024) {
        fileBytes.resize(1024, 0);
    }

    int result = FSUtils::WriteToFile(fullPath.c_str(), fileBytes.data(), fileBytes.size());
    if (result <= 0 && result != (int) fileBytes.size()) {
        res["success"] = false;
        res["message"] = "Failed to write file to SD card";
        return HttpResponse{500, res};
    }

    res["success"]  = true;
    res["filename"] = safeName;
    res["path"]     = fullPath;
    res["size"]     = (double) fileBytes.size();

    if (shouldLoad && targetSlot >= 0 && targetSlot < MAX_SKYLANDERS) {
        bool loaded       = g_skyportal.LoadSkylander(fileBytes.data(), fullPath, (uint8_t) targetSlot);
        res["loaded"]     = loaded;
        res["slot"]       = (double) targetSlot;
        res["portalSlot"] = (double) g_skyportal.GetPortalSlotFromUISlot((uint8_t) targetSlot);
        res["figureName"] = g_skyportal.GetSkylanderFromUISlot((uint8_t) targetSlot);
    }

    res["message"] = "File uploaded successfully: " + safeName;
    return HttpResponse{200, res};
}

static HttpResponse handleVault(const HttpRequest &req) {
    ensureSkylandersDirectory();
    std::string dirPath = "/vol/external01/wiiu/re_nsyshid/Skylanders";

    miniJson::Json::_object ret;
    miniJson::Json::_array figures;

    DIR *dir = opendir(dirPath.c_str());
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != nullptr) {
            std::string filename = ent->d_name;
            if (filename == "." || filename == "..") continue;

            if (filename.rfind(".sky") == std::string::npos && filename.rfind(".bin") == std::string::npos) {
                continue;
            }

            std::string fullPath = dirPath + "/" + filename;
            std::array<uint8_t, 1024> fileData{};
            int readBytes = FSUtils::ReadFromFile(fullPath.c_str(), fileData.data(), fileData.size());
            if (readBytes >= 32) {
                uint16_t skyId = 0, skyVar = 0;
                std::string name, game, element, type;
                uint32_t level = 1, money = 0;

                SkylanderPortal::ParseTagMetadata(fileData.data(), skyId, skyVar, name, game, element, type, level, money);

                int8_t loadedSlot = g_skyportal.GetUISlotForFilePath(fullPath);

                miniJson::Json::_object fig;
                fig["filename"] = filename;
                fig["path"]     = fullPath;
                fig["size"]     = (double) readBytes;
                fig["id"]       = (double) skyId;
                fig["variant"]  = (double) skyVar;
                fig["name"]     = name;
                fig["game"]     = game;
                fig["element"]  = element;
                fig["type"]     = type;
                fig["level"]    = (double) level;
                fig["money"]    = (double) money;
                fig["loaded"]   = (loadedSlot >= 0);
                if (loadedSlot >= 0) {
                    fig["slot"] = (double) loadedSlot;
                }

                figures.push_back(fig);
            }
        }
        closedir(dir);
    }

    ret["success"] = true;
    ret["count"]   = (double) figures.size();
    ret["figures"] = figures;
    return HttpResponse{200, ret};
}

static HttpResponse handleDelete(const HttpRequest &req) {
    miniJson::Json::_object res;
    std::string filename;

    auto bodyJson = req.json();
    if (bodyJson.isObject()) {
        auto obj = bodyJson.toObject();
        if (obj.count("filename") && obj["filename"].isString()) {
            filename = obj["filename"].toString();
        } else if (obj.count("path") && obj["path"].isString()) {
            filename = obj["path"].toString();
        }
    }

    if (filename.empty()) {
        std::string query = req.getQuery();
        size_t fnPos = query.find("filename=");
        if (fnPos != std::string::npos) {
            filename = decodeUrlString(query.substr(fnPos + 9));
            size_t amp = filename.find('&');
            if (amp != std::string::npos) filename = filename.substr(0, amp);
        } else {
            size_t pPos = query.find("path=");
            if (pPos != std::string::npos) {
                filename = decodeUrlString(query.substr(pPos + 5));
                size_t amp = filename.find('&');
                if (amp != std::string::npos) filename = filename.substr(0, amp);
            }
        }
    }

    if (filename.empty()) {
        res["success"] = false;
        res["message"] = "Missing 'filename' parameter";
        return HttpResponse{400, res};
    }

    std::string fullPath;
    if (filename.rfind("/vol/", 0) == 0) {
        fullPath = filename;
    } else {
        if (filename.rfind(".sky") == std::string::npos && filename.rfind(".bin") == std::string::npos) {
            filename += ".sky";
        }
        fullPath = "/vol/external01/wiiu/re_nsyshid/Skylanders/" + sanitizeFileName(filename);
    }

    // If loaded on portal, remove it first
    int8_t loadedSlot = g_skyportal.GetUISlotForFilePath(fullPath);
    if (loadedSlot >= 0) {
        g_skyportal.RemoveSkylander((uint8_t) loadedSlot);
    }

    int rc = unlink(fullPath.c_str());
    if (rc == 0) {
        res["success"] = true;
        res["message"] = "Figure file deleted successfully: " + filename;
        return HttpResponse{200, res};
    } else {
        res["success"] = false;
        res["message"] = "Failed to delete file or file not found: " + fullPath;
        return HttpResponse{404, res};
    }
}

static HttpResponse handleRename(const HttpRequest &req) {
    miniJson::Json::_object res;
    std::string oldName, newName;

    auto bodyJson = req.json();
    if (bodyJson.isObject()) {
        auto obj = bodyJson.toObject();
        if (obj.count("oldFilename") && obj["oldFilename"].isString()) {
            oldName = obj["oldFilename"].toString();
        } else if (obj.count("from") && obj["from"].isString()) {
            oldName = obj["from"].toString();
        }
        if (obj.count("newFilename") && obj["newFilename"].isString()) {
            newName = obj["newFilename"].toString();
        } else if (obj.count("to") && obj["to"].isString()) {
            newName = obj["to"].toString();
        }
    }

    if (oldName.empty() || newName.empty()) {
        res["success"] = false;
        res["message"] = "Missing 'oldFilename' or 'newFilename' parameter";
        return HttpResponse{400, res};
    }

    if (oldName.rfind(".sky") == std::string::npos && oldName.rfind(".bin") == std::string::npos) oldName += ".sky";
    if (newName.rfind(".sky") == std::string::npos && newName.rfind(".bin") == std::string::npos) newName += ".sky";

    std::string oldPath = "/vol/external01/wiiu/re_nsyshid/Skylanders/" + sanitizeFileName(oldName);
    std::string newPath = "/vol/external01/wiiu/re_nsyshid/Skylanders/" + sanitizeFileName(newName);

    int rc = rename(oldPath.c_str(), newPath.c_str());
    if (rc == 0) {
        res["success"]  = true;
        res["oldPath"]  = oldPath;
        res["newPath"]  = newPath;
        res["filename"] = sanitizeFileName(newName);
        res["message"]  = "File renamed successfully";
        return HttpResponse{200, res};
    } else {
        res["success"] = false;
        res["message"] = "Failed to rename file or file not found";
        return HttpResponse{400, res};
    }
}

static HttpResponse handleDuplicate(const HttpRequest &req) {
    miniJson::Json::_object res;
    std::string srcName, dstName;

    auto bodyJson = req.json();
    if (bodyJson.isObject()) {
        auto obj = bodyJson.toObject();
        if (obj.count("filename") && obj["filename"].isString()) {
            srcName = obj["filename"].toString();
        } else if (obj.count("src") && obj["src"].isString()) {
            srcName = obj["src"].toString();
        }
        if (obj.count("newFilename") && obj["newFilename"].isString()) {
            dstName = obj["newFilename"].toString();
        } else if (obj.count("dst") && obj["dst"].isString()) {
            dstName = obj["dst"].toString();
        }
    }

    if (srcName.empty()) {
        res["success"] = false;
        res["message"] = "Missing source 'filename' parameter";
        return HttpResponse{400, res};
    }

    if (srcName.rfind(".sky") == std::string::npos && srcName.rfind(".bin") == std::string::npos) srcName += ".sky";
    if (dstName.empty()) {
        dstName = "copy_" + srcName;
    } else {
        if (dstName.rfind(".sky") == std::string::npos && dstName.rfind(".bin") == std::string::npos) dstName += ".sky";
    }

    std::string srcPath = "/vol/external01/wiiu/re_nsyshid/Skylanders/" + sanitizeFileName(srcName);
    std::string dstPath = "/vol/external01/wiiu/re_nsyshid/Skylanders/" + sanitizeFileName(dstName);

    std::array<uint8_t, 1024> fileData{};
    int readBytes = FSUtils::ReadFromFile(srcPath.c_str(), fileData.data(), fileData.size());
    if (readBytes > 0) {
        int writeBytes = FSUtils::WriteToFile(dstPath.c_str(), fileData.data(), readBytes);
        if (writeBytes > 0) {
            res["success"]     = true;
            res["srcFilename"] = sanitizeFileName(srcName);
            res["dstFilename"] = sanitizeFileName(dstName);
            res["path"]        = dstPath;
            res["message"]     = "Figure duplicated successfully";
            return HttpResponse{200, res};
        }
    }

    res["success"] = false;
    res["message"] = "Failed to duplicate file: source not found or SD write error";
    return HttpResponse{400, res};
}

static HttpResponse corsOptionsHandler(const HttpRequest &req) {
    HttpResponse res(200);
    res["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS, DELETE";
    res["Access-Control-Allow-Headers"] = "Content-Type, Accept, X-Filename, Content-Disposition";
    res["Access-Control-Max-Age"]       = "86400";
    return res;
}

void registerSkylanderEndpoints(HttpServer &server) {

    // ==========================================
    // 🌐 Web UI Endpoints
    // ==========================================
    server.when("/")->requested([](const HttpRequest &req) {
        return HttpResponse{200, "text/html", WEB_UI_HTML};
    });

    server.when("/web")->requested([](const HttpRequest &req) {
        return HttpResponse{200, "text/html", WEB_UI_HTML};
    });

    // ==========================================
    // 📡 Status Endpoints (/api/skylanders/status & /api/status)
    // ==========================================
    server.when("/api/skylanders/status")
            ->options(corsOptionsHandler)
            ->requested(handleGetStatus);

    server.when("/api/status")
            ->options(corsOptionsHandler)
            ->requested(handleGetStatus);

    // ==========================================
    // 📖 Catalog Endpoint (/api/skylanders)
    // ==========================================
    server.when("/api/skylanders")
            ->options(corsOptionsHandler)
            ->requested([](const HttpRequest &req) {
                miniJson::Json::_array list;
                for (const auto &sky : SkylanderPortal::GetAllSkylandersDetailed()) {
                    miniJson::Json::_object item;
                    item["id"]      = (double) sky.id;
                    item["variant"] = (double) sky.variant;
                    item["name"]    = std::string(sky.name);
                    item["game"]    = sky.game;
                    item["element"] = sky.element;
                    item["type"]    = sky.type;
                    list.push_back(item);
                }
                return HttpResponse{200, list};
            });

    // ==========================================
    // ⚡ Load Endpoints (/api/skylanders/load & /api/load)
    // ==========================================
    server.when("/api/skylanders/load")
            ->options(corsOptionsHandler)
            ->posted(handleLoad);

    server.when("/api/load")
            ->options(corsOptionsHandler)
            ->posted(handleLoad);

    // ==========================================
    // 🗑️ Remove Endpoints (/api/skylanders/remove & /api/remove)
    // ==========================================
    server.when("/api/skylanders/remove")
            ->options(corsOptionsHandler)
            ->posted(handleRemove);

    server.when("/api/remove")
            ->options(corsOptionsHandler)
            ->posted(handleRemove);

    // ==========================================
    // 🧹 Clear Endpoints (/api/skylanders/clear & /api/clear)
    // ==========================================
    server.when("/api/skylanders/clear")
            ->options(corsOptionsHandler)
            ->posted(handleClear);

    server.when("/api/clear")
            ->options(corsOptionsHandler)
            ->posted(handleClear);

    // ==========================================
    // 💾 Download Endpoints (/api/skylanders/download & /api/download)
    // ==========================================
    server.when("/api/skylanders/download")
            ->options(corsOptionsHandler)
            ->requested(handleDownload);

    server.when("/api/download")
            ->options(corsOptionsHandler)
            ->requested(handleDownload);

    // ==========================================
    // 📤 Upload Endpoints (/api/skylanders/upload & /api/upload)
    // ==========================================
    server.when("/api/skylanders/upload")
            ->options(corsOptionsHandler)
            ->posted(handleUpload);

    server.when("/api/upload")
            ->options(corsOptionsHandler)
            ->posted(handleUpload);

    // ==========================================
    // 🏦 Vault / Storage Endpoints (/api/skylanders/vault, /api/skylanders/files, /api/vault)
    // ==========================================
    server.when("/api/skylanders/vault")
            ->options(corsOptionsHandler)
            ->requested(handleVault);

    server.when("/api/skylanders/files")
            ->options(corsOptionsHandler)
            ->requested(handleVault);

    server.when("/api/vault")
            ->options(corsOptionsHandler)
            ->requested(handleVault);

    // ==========================================
    // 🗑️ Delete / Release Endpoints (/api/skylanders/delete & /api/delete)
    // ==========================================
    server.when("/api/skylanders/delete")
            ->options(corsOptionsHandler)
            ->posted(handleDelete);

    server.when("/api/delete")
            ->options(corsOptionsHandler)
            ->posted(handleDelete);

    // ==========================================
    // ✏️ Rename Endpoints (/api/skylanders/rename & /api/rename)
    // ==========================================
    server.when("/api/skylanders/rename")
            ->options(corsOptionsHandler)
            ->posted(handleRename);

    server.when("/api/rename")
            ->options(corsOptionsHandler)
            ->posted(handleRename);

    // ==========================================
    // 📑 Duplicate / Clone Endpoints (/api/skylanders/duplicate, /api/skylanders/clone, /api/duplicate)
    // ==========================================
    server.when("/api/skylanders/duplicate")
            ->options(corsOptionsHandler)
            ->posted(handleDuplicate);

    server.when("/api/skylanders/clone")
            ->options(corsOptionsHandler)
            ->posted(handleDuplicate);

    server.when("/api/duplicate")
            ->options(corsOptionsHandler)
            ->posted(handleDuplicate);

    // ==========================================
    // 📦 re_nsyshid Legacy / Dashboard Endpoints
    // ==========================================
    server.when("/device/skylander")->requested([](const HttpRequest &req) {
        miniJson::Json::_object ret;
        for (uint8_t i = 0; i < MAX_SKYLANDERS; i++) {
            miniJson::Json::_object skylander;
            const auto idvar       = g_skyportal.GetSkylanderIdFromUISlot(i);
            const auto skyName     = g_skyportal.GetSkylanderFromUISlot(i);
            skylander["id"]        = idvar.first;
            skylander["var"]       = idvar.second;
            skylander["name"]      = skyName;
            ret[std::to_string(i)] = skylander;
        }
        return HttpResponse{200, ret};
    });

    server.when("/device/skylander/remove")
            ->options(corsOptionsHandler)
            ->posted([](const HttpRequest &req) {
                miniJson::Json::_object res;
                auto body = req.json();

                if (!body.isObject()) {
                    res["error"] = "INVALID_BODY";
                    return HttpResponse{200, res};
                }
                auto removeRequest    = body.toObject();
                const auto portalSlot = removeRequest["slot"];
                if (!portalSlot.isNumber()) {
                    res["error"] = "INVALID_SLOT_PARAM";
                    return HttpResponse{400, res};
                }
                uint8_t slot = uint8_t(portalSlot.toDouble());
                if (slot >= MAX_SKYLANDERS || slot < 0) {
                    res["error"] = "INVALID_SLOT";
                    return HttpResponse{400, res};
                }
                if (g_skyportal.RemoveSkylander(slot)) {
                    res["message"] = "Skylander removed";
                    return HttpResponse{200, res};
                } else {
                    res["message"] = "NO_SKYLANDER_IN_SLOT";
                    return HttpResponse{404, res};
                }
            });

    server.when("/device/skylander/load")
            ->options(corsOptionsHandler)
            ->posted([](const HttpRequest &req) {
                miniJson::Json::_object res;
                auto body = req.json();

                if (!body.isObject()) {
                    res["error"] = "INVALID_BODY";
                    return HttpResponse{200, res};
                }
                auto loadRequest      = body.toObject();
                std::string file      = "/vol/external01/wiiu/re_nsyshid/" + loadRequest["file"].toString();
                const auto portalSlot = loadRequest["slot"];
                if (!portalSlot.isNumber()) {
                    res["error"] = "INVALID_SLOT_PARAM";
                    return HttpResponse{400, res};
                }
                uint8_t slot = uint8_t(portalSlot.toDouble());
                if (slot >= MAX_SKYLANDERS || slot < 0) {
                    res["error"] = "INVALID_SLOT";
                    return HttpResponse{400, res};
                }
                std::array<uint8_t, 0x10 * 0x40> fileData;
                int ret_code = FSUtils::ReadFromFile(file.c_str(), fileData.data(), fileData.size());
                if (ret_code == (int) fileData.size()) {
                    if (!g_skyportal.LoadSkylander(fileData.data(), file, slot)) {
                        res["error"] = "FAILED_TO_LOAD_SKYLANDER";
                        return HttpResponse{404, res};
                    }
                    res["message"] = "Skylander loaded";
                    return HttpResponse{200, res};
                } else {
                    res["error"] = "SKYLANDER_FILE_TOO_SMALL";
                    return HttpResponse{400, res};
                }
            });

    server.when("/device/skylander/create")
            ->options(corsOptionsHandler)
            ->posted([](const HttpRequest &req) {
                miniJson::Json::_object res;
                auto body = req.json();

                if (!body.isObject()) {
                    res["error"] = "INVALID_BODY";
                    return HttpResponse{200, res};
                }
                auto createRequest = body.toObject();
                const auto skyId   = createRequest["id"];
                const auto skyVar  = createRequest["var"];
                if (!skyId.isNumber() || !skyVar.isNumber()) {
                    res["error"] = "INVALID_ID_OR_VAR_PARAM";
                    return HttpResponse{400, res};
                }
                uint16_t id      = uint16_t(skyId.toDouble());
                uint16_t var     = uint16_t(skyVar.toDouble());
                std::string name = g_skyportal.FindSkylander(id, var);
                ensureSkylandersDirectory();
                std::string safeName = sanitizeFileName(name);
                if (g_skyportal.CreateSkylander("/vol/external01/wiiu/re_nsyshid/Skylanders/" + safeName + ".sky", id, var)) {
                    res["message"] = "Skylander created";
                    res["file"]    = "/Skylanders/" + safeName + ".sky";
                    return HttpResponse{200, res};
                } else {
                    res["error"] = "FAILED_TO_CREATE_SKYLANDER";
                    return HttpResponse{400, res};
                }
            });
}
