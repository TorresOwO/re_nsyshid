#include "skylanderendpoints.h"
#include "../devices/Skylander.h"
#include "../utils/FSUtils.hpp"
#include "web_ui.h"

#include <algorithm>
#include <cctype>
#include <sys/stat.h>

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
    // 📡 Dolphin Skylanders REST API: /api/status
    // ==========================================
    server.when("/api/status")
            ->options([](const HttpRequest &req) {
                HttpResponse res(200);
                res["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
                res["Access-Control-Allow-Headers"] = "Content-Type";
                res["Access-Control-Max-Age"]       = "86400";
                return res;
            })
            ->requested([](const HttpRequest &req) {
                miniJson::Json::_object ret;
                ret["success"] = true;
                ret["running"] = true;

                miniJson::Json::_array slots;
                for (uint8_t i = 0; i < MAX_SKYLANDERS; i++) {
                    miniJson::Json::_object slotObj;
                    bool occupied     = g_skyportal.IsSlotOccupied(i);
                    int8_t portalSlot = g_skyportal.GetPortalSlotFromUISlot(i);
                    std::string name  = occupied ? g_skyportal.GetSkylanderFromUISlot(i) : ("Slot " + std::to_string(i + 1));

                    slotObj["slot"]       = (double) i;
                    slotObj["portalSlot"] = (double) portalSlot;
                    slotObj["name"]       = name;
                    slotObj["occupied"]   = occupied;
                    slots.push_back(slotObj);
                }
                ret["slots"] = slots;
                return HttpResponse{200, ret};
            });

    // ==========================================
    // 📖 Dolphin Skylanders REST API: /api/skylanders
    // ==========================================
    server.when("/api/skylanders")
            ->options([](const HttpRequest &req) {
                HttpResponse res(200);
                res["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
                res["Access-Control-Allow-Headers"] = "Content-Type";
                res["Access-Control-Max-Age"]       = "86400";
                return res;
            })
            ->requested([](const HttpRequest &req) {
                miniJson::Json::_array list;
                for (const auto &[idvar, name] : SkylanderPortal::GetListSkylanders()) {
                    miniJson::Json::_object item;
                    item["id"]      = (double) idvar.first;
                    item["variant"] = (double) idvar.second;
                    item["name"]    = std::string(name);
                    list.push_back(item);
                }
                return HttpResponse{200, list};
            });

    // ==========================================
    // ⚡ Dolphin Skylanders REST API: /api/load
    // ==========================================
    server.when("/api/load")
            ->options([](const HttpRequest &req) {
                HttpResponse res(200);
                res["Access-Control-Allow-Methods"] = "POST, OPTIONS";
                res["Access-Control-Allow-Headers"] = "Content-Type";
                res["Access-Control-Max-Age"]       = "86400";
                return res;
            })
            ->posted([](const HttpRequest &req) {
                miniJson::Json::_object res;
                auto body = req.json();

                if (!body.isObject()) {
                    res["success"] = false;
                    res["error"]   = "INVALID_BODY";
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
                        res["error"]   = "INVALID_SLOT";
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
                        res["error"]   = "SKYLANDER_NAME_NOT_FOUND";
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
                    res["error"]   = "MISSING_NAME_ID_OR_PATH";
                    return HttpResponse{400, res};
                }

                // Cargar archivo en la ranura del portal
                std::array<uint8_t, 0x10 * 0x40> fileData;
                int ret_code = FSUtils::ReadFromFile(filePath.c_str(), fileData.data(), fileData.size());
                if (ret_code == (int) fileData.size()) {
                    if (!g_skyportal.LoadSkylander(fileData.data(), filePath, slot)) {
                        res["success"] = false;
                        res["error"]   = "FAILED_TO_LOAD_SKYLANDER";
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
                    return HttpResponse{200, res};
                } else {
                    res["success"] = false;
                    res["error"]   = "FAILED_TO_READ_FILE";
                    return HttpResponse{400, res};
                }
            });

    // ==========================================
    // 🗑️ Dolphin Skylanders REST API: /api/remove
    // ==========================================
    server.when("/api/remove")
            ->options([](const HttpRequest &req) {
                HttpResponse res(200);
                res["Access-Control-Allow-Methods"] = "POST, OPTIONS";
                res["Access-Control-Allow-Headers"] = "Content-Type";
                res["Access-Control-Max-Age"]       = "86400";
                return res;
            })
            ->posted([](const HttpRequest &req) {
                miniJson::Json::_object res;
                auto body = req.json();

                if (!body.isObject()) {
                    res["success"] = false;
                    res["error"]   = "INVALID_BODY";
                    return HttpResponse{400, res};
                }
                auto removeRequest = body.toObject();
                uint8_t slot       = 0;
                if (removeRequest.count("slot") && removeRequest["slot"].isNumber()) {
                    slot = (uint8_t) removeRequest["slot"].toDouble();
                }

                if (slot >= MAX_SKYLANDERS) {
                    res["success"] = false;
                    res["error"]   = "INVALID_SLOT";
                    return HttpResponse{400, res};
                }

                g_skyportal.RemoveSkylander(slot);
                res["success"] = true;
                res["slot"]    = (double) slot;
                return HttpResponse{200, res};
            });

    // ==========================================
    // 🧹 Dolphin Skylanders REST API: /api/clear
    // ==========================================
    server.when("/api/clear")
            ->options([](const HttpRequest &req) {
                HttpResponse res(200);
                res["Access-Control-Allow-Methods"] = "POST, OPTIONS";
                res["Access-Control-Allow-Headers"] = "Content-Type";
                res["Access-Control-Max-Age"]       = "86400";
                return res;
            })
            ->posted([](const HttpRequest &req) {
                for (uint8_t i = 0; i < MAX_SKYLANDERS; i++) {
                    g_skyportal.RemoveSkylander(i);
                }
                miniJson::Json::_object res;
                res["success"] = true;
                res["message"] = "All figures removed";
                return HttpResponse{200, res};
            });

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
            ->options([](const HttpRequest &req) {
                HttpResponse res(200);
                res["Access-Control-Allow-Methods"] = "POST, OPTIONS";
                res["Access-Control-Allow-Headers"] = "Content-Type";
                res["Access-Control-Max-Age"]       = "86400";
                return res;
            })
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
            ->options([](const HttpRequest &req) {
                HttpResponse res(200);
                res["Access-Control-Allow-Methods"] = "POST, OPTIONS";
                res["Access-Control-Allow-Headers"] = "Content-Type";
                res["Access-Control-Max-Age"]       = "86400";
                return res;
            })
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
            ->options([](const HttpRequest &req) {
                HttpResponse res(200);
                res["Access-Control-Allow-Methods"] = "POST, OPTIONS";
                res["Access-Control-Allow-Headers"] = "Content-Type";
                res["Access-Control-Max-Age"]       = "86400";
                return res;
            })
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
