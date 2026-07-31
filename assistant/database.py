"""
Module 6 - Persistence layer (SQLite).

This file was referenced by app.py (`import database`) but never actually
existed in the project, which is why `python app.py` failed immediately
with `ModuleNotFoundError: No module named 'database'`.

Storage: a single SQLite file, chess_coach.db, created next to this file
on first run. No external DB server required - just drop this file in
next to app.py and it works.

Schema:
  players       - one row per player, tracks Elo
  profiles      - one row per player, the Module 5 weakness_profile (P) +
                  consecutive_counts + current homework motif list
  game_records  - one row PER GAME (append-only), used to build the
                  progress timeline / dashboard. Also stores the full
                  per-move breakdown (moves_detail) so a future "game
                  history" screen can show move-by-move quality/cp_loss/
                  motifs without needing a schema change later.
"""

import os
import json
import math
import sqlite3

DB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "chess_coach.db")

NUM_MOTIFS = 10
MOTIF_NAMES = [
    "hanging_piece", "missed_fork", "king_safety_collapse", "back_rank_weakness",
    "missed_pin_skewer", "time_pressure_blunder", "defensive_inaccuracy",
    "offensive_miss", "tactical_blindness", "positional_misjudgment",
]

STARTING_ELO = 1200.0
K_FACTOR = 32.0


def _conn():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA foreign_keys = ON")
    return conn


def init_db():
    conn = _conn()
    conn.execute("""
        CREATE TABLE IF NOT EXISTS players (
            player_id INTEGER PRIMARY KEY,
            elo REAL NOT NULL DEFAULT 1200
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS profiles (
            player_id INTEGER PRIMARY KEY,
            weakness_profile TEXT NOT NULL,
            consecutive_counts TEXT NOT NULL,
            homework TEXT NOT NULL DEFAULT '[]',
            sliders TEXT,
            engine_config TEXT
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS game_records (
            game_id INTEGER PRIMARY KEY AUTOINCREMENT,
            player_id INTEGER NOT NULL,
            played_at TEXT DEFAULT CURRENT_TIMESTAMP,
            moves TEXT NOT NULL,
            motif_vector TEXT NOT NULL,
            accuracy_pct REAL,
            acpl REAL,
            phase_acpl TEXT,
            elo_after REAL,
            moves_detail TEXT
        )
    """)
    conn.commit()

    # safety net: if an older chess_coach.db already exists without these
    # columns (e.g. from a partial earlier run), add them rather than crash
    existing_cols = {row["name"] for row in conn.execute("PRAGMA table_info(profiles)")}
    for col in ("sliders", "engine_config"):
        if col not in existing_cols:
            conn.execute(f"ALTER TABLE profiles ADD COLUMN {col} TEXT")
    conn.commit()
    conn.close()


def get_or_create_player(player_id: int) -> dict:
    conn = _conn()
    row = conn.execute("SELECT * FROM players WHERE player_id=?", (player_id,)).fetchone()
    if row is None:
        conn.execute("INSERT INTO players (player_id, elo) VALUES (?, ?)", (player_id, STARTING_ELO))
        conn.commit()
        result = {"player_id": player_id, "elo": STARTING_ELO}
    else:
        result = dict(row)
    conn.close()
    return result


def get_profile(player_id: int):
    """Returns (weakness_profile: list[float] len 10, counts: list[int] len 10)."""
    conn = _conn()
    row = conn.execute("SELECT * FROM profiles WHERE player_id=?", (player_id,)).fetchone()
    conn.close()
    if row is None:
        return [0.0] * NUM_MOTIFS, [0] * NUM_MOTIFS
    return json.loads(row["weakness_profile"]), json.loads(row["consecutive_counts"])


def save_profile(player_id: int, new_profile, updated_counts):
    conn = _conn()
    conn.execute("""
        INSERT INTO profiles (player_id, weakness_profile, consecutive_counts, homework)
        VALUES (?, ?, ?, '[]')
        ON CONFLICT(player_id) DO UPDATE SET
            weakness_profile = excluded.weakness_profile,
            consecutive_counts = excluded.consecutive_counts
    """, (player_id, json.dumps(list(new_profile)), json.dumps(list(updated_counts))))
    conn.commit()
    conn.close()


def save_study_plan(player_id: int, homework_motifs):
    conn = _conn()
    conn.execute("""
        INSERT INTO profiles (player_id, weakness_profile, consecutive_counts, homework)
        VALUES (?, ?, ?, ?)
        ON CONFLICT(player_id) DO UPDATE SET homework = excluded.homework
    """, (player_id, json.dumps([0.0] * NUM_MOTIFS), json.dumps([0] * NUM_MOTIFS),
          json.dumps(list(homework_motifs))))
    conn.commit()
    conn.close()


def save_engine_state(player_id: int, sliders, engine_config):
    """Persists the Module 5 output sliders + the EngineConfig they map to,
    so the Engine Personality panel has something to show even before the
    modal/dashboard has been opened this session."""
    conn = _conn()
    conn.execute("""
        INSERT INTO profiles (player_id, weakness_profile, consecutive_counts, sliders, engine_config)
        VALUES (?, ?, ?, ?, ?)
        ON CONFLICT(player_id) DO UPDATE SET
            sliders = excluded.sliders,
            engine_config = excluded.engine_config
    """, (player_id, json.dumps([0.0] * NUM_MOTIFS), json.dumps([0] * NUM_MOTIFS),
          json.dumps(list(sliders)), json.dumps(engine_config)))
    conn.commit()
    conn.close()


def compute_elo(old_elo: float, result: float, engine_elo: float, k: float = K_FACTOR) -> float:
    """Standard Elo update. result: 1.0 win / 0.5 draw / 0.0 loss (player's side)."""
    expected = 1.0 / (1.0 + math.pow(10, (engine_elo - old_elo) / 400.0))
    return round(old_elo + k * (result - expected), 1)


def update_elo(player_id: int, new_elo: float):
    conn = _conn()
    conn.execute("UPDATE players SET elo=? WHERE player_id=?", (new_elo, player_id))
    conn.commit()
    conn.close()


def save_game_record(player_id: int, moves, x_vec, accuracy_pct, acpl, phase_acpl,
                      elo_after=None, moves_detail=None):
    """moves_detail is optional (list of per-move dicts from the engine's
    GameReport, e.g. report['moves'] in the --endgame output) - stored now
    so a future game-history screen can show move-by-move breakdowns
    without another migration."""
    conn = _conn()
    conn.execute("""
        INSERT INTO game_records
            (player_id, moves, motif_vector, accuracy_pct, acpl, phase_acpl, elo_after, moves_detail)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    """, (
        player_id, json.dumps(list(moves)), json.dumps(list(x_vec)),
        accuracy_pct, acpl, json.dumps(list(phase_acpl)), elo_after,
        json.dumps(moves_detail) if moves_detail is not None else None,
    ))
    conn.commit()
    conn.close()


def get_player_dashboard(player_id: int) -> dict:
    conn = _conn()
    player_row = conn.execute("SELECT * FROM players WHERE player_id=?", (player_id,)).fetchone()
    elo = player_row["elo"] if player_row else STARTING_ELO

    games = conn.execute(
        "SELECT game_id, played_at, accuracy_pct, acpl, elo_after FROM game_records "
        "WHERE player_id=? ORDER BY played_at ASC", (player_id,)
    ).fetchall()
    timeline = [dict(g) for g in games]

    prof_row = conn.execute("SELECT * FROM profiles WHERE player_id=?", (player_id,)).fetchone()
    if prof_row:
        weakness = json.loads(prof_row["weakness_profile"])
        homework = json.loads(prof_row["homework"]) if prof_row["homework"] else []
        sliders = json.loads(prof_row["sliders"]) if prof_row["sliders"] else [0.0] * 5
        engine_config = json.loads(prof_row["engine_config"]) if prof_row["engine_config"] else None
    else:
        weakness = [0.0] * NUM_MOTIFS
        homework = []
        sliders = [0.0] * 5
        engine_config = None
    heatmap = [{"motif": MOTIF_NAMES[i], "score": weakness[i]} for i in range(NUM_MOTIFS)]

    conn.close()
    return {
        "elo": elo, "timeline": timeline, "heatmap": heatmap, "homework": homework,
        "sliders": sliders, "engine_config": engine_config,
    }


def list_games(player_id: int):
    conn = _conn()
    rows = conn.execute(
        "SELECT game_id, played_at, accuracy_pct, acpl, elo_after FROM game_records "
        "WHERE player_id=? ORDER BY played_at DESC", (player_id,)
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


def get_game(game_id: int):
    conn = _conn()
    row = conn.execute("SELECT * FROM game_records WHERE game_id=?", (game_id,)).fetchone()
    conn.close()
    if row is None:
        return None
    result = dict(row)
    result["moves"] = json.loads(result["moves"])
    result["motif_vector"] = json.loads(result["motif_vector"])
    result["phase_acpl"] = json.loads(result["phase_acpl"])
    result["moves_detail"] = json.loads(result["moves_detail"]) if result.get("moves_detail") else None
    return result
