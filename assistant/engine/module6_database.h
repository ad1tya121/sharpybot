#ifndef MODULE6_DATABASE_H
#define MODULE6_DATABASE_H

// Database lifecycle:
//   New player  -> fetchProfile returns false -> caller uses zero profile
//                  -> saveProfile does an INSERT (first time)
//   Existing player -> fetchProfile returns true -> loads P_old + counts
//                   -> after game: saveProfile does an UPDATE (UPSERT handles both)
// One row per player in `profiles`. Gets overwritten after each game.
// Game history (game_records) is a separate append-only table - one row per game.
//
// Required table:
//   CREATE TABLE profiles (
//       player_id          INTEGER PRIMARY KEY,
//       weakness_profile   REAL[]    NOT NULL,
//       consecutive_counts INTEGER[] NOT NULL
//   );

#include "chess_types.h"
#include <vector>
#include <string>
#include <libpq-fe.h>

class Database {
private:
    PGconn* connection;

    std::vector<float> parseFloatArray(const std::string& text) {
        std::vector<float> result;
        size_t start = text.find('{');
        size_t end = text.find('}');
        if (start == std::string::npos || end == std::string::npos) return result;
        std::string inner = text.substr(start + 1, end - start - 1);
        std::string cur;
        for (char c : inner) {
            if (c == ',') { result.push_back(std::stof(cur)); cur = ""; }
            else cur += c;
        }
        if (cur.size() > 0) result.push_back(std::stof(cur));
        return result;
    }

    std::vector<int> parseIntArray(const std::string& text) {
        std::vector<int> result;
        size_t start = text.find('{');
        size_t end = text.find('}');
        if (start == std::string::npos || end == std::string::npos) return result;
        std::string inner = text.substr(start + 1, end - start - 1);
        std::string cur;
        for (char c : inner) {
            if (c == ',') { result.push_back(std::stoi(cur)); cur = ""; }
            else cur += c;
        }
        if (cur.size() > 0) result.push_back(std::stoi(cur));
        return result;
    }

    std::string toFloatArrayText(const std::vector<float>& v) {
        std::string t = "{";
        for (int i = 0; i < (int)v.size(); i++) {
            t += std::to_string(v[i]);
            if (i < (int)v.size() - 1) t += ",";
        }
        return t + "}";
    }

    std::string toIntArrayText(const std::vector<int>& v) {
        std::string t = "{";
        for (int i = 0; i < (int)v.size(); i++) {
            t += std::to_string(v[i]);
            if (i < (int)v.size() - 1) t += ",";
        }
        return t + "}";
    }

public:
    Database(const std::string& connectionString) {
        connection = PQconnectdb(connectionString.c_str());
        if (PQstatus(connection) != CONNECTION_OK) {
            printf("DB connection failed: %s\n", PQerrorMessage(connection));
        }
    }

    ~Database() {
        PQfinish(connection);
    }

    bool isConnected() {
        return PQstatus(connection) == CONNECTION_OK;
    }

    bool fetchProfile(int playerId, std::vector<float>& outProfile, std::vector<int>& outCounts) {
        std::string idStr = std::to_string(playerId);
        const char* paramValues[1] = { idStr.c_str() };
        PGresult* res = PQexecParams(connection,
            "SELECT weakness_profile, consecutive_counts FROM profiles WHERE player_id = $1",
            1, NULL, paramValues, NULL, NULL, 0);

        if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
            PQclear(res);
            return false;
        }

        outProfile = parseFloatArray(PQgetvalue(res, 0, 0));
        outCounts  = parseIntArray(PQgetvalue(res, 0, 1));
        PQclear(res);
        return true;
    }

    bool saveProfile(int playerId, const std::vector<float>& profile, const std::vector<int>& counts) {
        std::string idStr      = std::to_string(playerId);
        std::string profileStr = toFloatArrayText(profile);
        std::string countsStr  = toIntArrayText(counts);
        const char* params[3]  = { idStr.c_str(), profileStr.c_str(), countsStr.c_str() };

        PGresult* res = PQexecParams(connection,
            "INSERT INTO profiles (player_id, weakness_profile, consecutive_counts) "
            "VALUES ($1, $2, $3) "
            "ON CONFLICT (player_id) DO UPDATE "
            "SET weakness_profile = $2, consecutive_counts = $3",
            3, NULL, params, NULL, NULL, 0);

        bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
        if (!ok) printf("Save failed: %s\n", PQerrorMessage(connection));
        PQclear(res);
        return ok;
    }
};

#endif
