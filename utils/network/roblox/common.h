#pragma once

#include <imgui.h>
#include <random>
#include <string>


static ImVec4 getStatusColor(std::string statusCode) {
        if (statusCode == "Online") {
                return ImVec4(0.6f, 0.8f, 0.95f, 1.0f);
        }
        if (statusCode == "InGame") {
                return ImVec4(0.6f, 0.9f, 0.7f, 1.0f);
        }
        if (statusCode == "InStudio") {
                return ImVec4(1.0f, 0.85f, 0.7f, 1.0f);
        }
        if (statusCode == "Invisible") {
                return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        }
        if (statusCode == "Banned") {
                return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        }
        return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
}

static std::string generateSessionId() {
        static auto hex = "0123456789abcdef";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);

        std::string uuid(36, ' ');
        for (int i = 0; i < 36; i++) {
                switch (i) {
                        case 8:
                        case 13:
                        case 18:
                        case 23:
                                uuid[i] = '-';
                                break;
                        case 14:
                                uuid[i] = '4';
                                break;
                        case 19:
                                uuid[i] = hex[(dis(gen) & 0x3) | 0x8];
                                break;
                        default:
                                uuid[i] = hex[dis(gen)];
                }
        }
        return uuid;
}

static std::string presenceTypeToString(int type) {
        switch (type) {
                case 1:
                        return "Online";
                case 2:
                        return "InGame";
                case 3:
                        return "InStudio";
                case 4:
                        return "Invisible";
                default:
                        return "Offline";
        }
}

