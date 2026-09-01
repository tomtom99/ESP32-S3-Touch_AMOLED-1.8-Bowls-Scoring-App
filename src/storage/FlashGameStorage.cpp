#include "storage/FlashGameStorage.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

namespace bowls {

FlashGameStorage::FlashGameStorage(const char* path) : path_(path) {}

bool FlashGameStorage::begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("FlashGameStorage: failed to mount LittleFS");
        return false;
    }
    return true;
}

bool FlashGameStorage::load(GameHistory& history) {
    if (!LittleFS.exists(path_)) {
        return true;  // Nothing saved yet - not an error.
    }

    File file = LittleFS.open(path_, "r");
    if (!file) {
        Serial.println("FlashGameStorage: failed to open history file");
        return false;
    }

    // Sized generously enough for a reasonably long history; JsonDocument
    // grows as needed but this avoids repeated small reallocations.
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) {
        Serial.printf("FlashGameStorage: failed to parse history: %s\n", err.c_str());
        return false;
    }

    JsonArray games = doc["games"].as<JsonArray>();
    for (JsonObject g : games) {
        GameType type = static_cast<int>(g["type"] | 0) == 1 ? GameType::Doubles
                                                               : GameType::Singles;
        std::vector<std::string> team1Names;
        std::vector<std::string> team2Names;
        for (JsonVariant n : g["team1"]["players"].as<JsonArray>()) {
            team1Names.push_back(n.as<const char*>());
        }
        for (JsonVariant n : g["team2"]["players"].as<JsonArray>()) {
            team2Names.push_back(n.as<const char*>());
        }

        BowlsGame game(type, team1Names, team2Names,
                       g["startTimestamp"] | 0);
        for (JsonObject end : g["ends"].as<JsonArray>()) {
            game.recordEnd(end["t1"] | 0, end["t2"] | 0);
        }
        game.finish(g["endTimestamp"] | 0);
        history.addGame(game);
    }

    return true;
}

bool FlashGameStorage::save(const GameHistory& history) {
    JsonDocument doc;
    JsonArray games = doc["games"].to<JsonArray>();

    for (const BowlsGame& game : history.games()) {
        JsonObject g = games.add<JsonObject>();
        g["type"] = game.type() == GameType::Doubles ? 1 : 0;
        g["startTimestamp"] = game.startTimestamp();
        g["endTimestamp"] = game.endTimestamp();

        JsonObject team1 = g["team1"].to<JsonObject>();
        JsonArray team1Players = team1["players"].to<JsonArray>();
        for (const std::string& name : game.team1().playerNames) {
            team1Players.add(name);
        }

        JsonObject team2 = g["team2"].to<JsonObject>();
        JsonArray team2Players = team2["players"].to<JsonArray>();
        for (const std::string& name : game.team2().playerNames) {
            team2Players.add(name);
        }

        JsonArray ends = g["ends"].to<JsonArray>();
        for (const EndResult& end : game.ends()) {
            JsonObject e = ends.add<JsonObject>();
            e["t1"] = end.team1Score;
            e["t2"] = end.team2Score;
        }
    }

    File file = LittleFS.open(path_, "w");
    if (!file) {
        Serial.println("FlashGameStorage: failed to open history file for writing");
        return false;
    }
    serializeJson(doc, file);
    file.close();
    return true;
}

}  // namespace bowls
