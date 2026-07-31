import os
import json
import subprocess
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from dotenv import load_dotenv
from google import genai

import database

load_dotenv()
client = genai.Client(api_key=os.getenv("GEMINI_API_KEY"))
database.init_db()

app = FastAPI()
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])

BASE_DIR = os.path.dirname(__file__)
ENGINE_DIR = os.path.join(BASE_DIR, "engine")
EXE_PATH = os.path.join(ENGINE_DIR, "chess_bridge.exe")


class AnalyzeRequest(BaseModel):
    moves: list[str]                 # full UCI move list played so far, e.g. ["e2e4","e7e5"]
    time_spent_seconds: float = 0.0  # how long the player spent on the LAST move


class EndgameRequest(BaseModel):
    moves: list[str]
    player_id: int = 1
    result: float | None = None      # 1.0 win / 0.5 draw / 0.0 loss, from the player's side. Omit to skip Elo update.


class ChatRequest(BaseModel):
    message: str
    fen: str


class CoachNoteRequest(BaseModel):
    coaching_prompt: str
    fallback_note: str = ""


def is_quota_error(exc: Exception) -> bool:
    """Gemini's free tier raises a 429 RESOURCE_EXHAUSTED when the daily
    request quota (currently as low as 20/day on some free-tier projects)
    is used up. Retrying immediately won't help - the quota resets daily,
    not per-minute - so we detect it and fail over to the engine-only
    fallback_note instead of retrying."""
    text = str(exc)
    return "RESOURCE_EXHAUSTED" in text or "429" in text or "quota" in text.lower()


def call_gemini(prompt: str) -> tuple[str | None, str]:
    """Returns (note, source). source is 'gemini', 'quota_exceeded', or 'error'."""
    try:
        response = client.models.generate_content(
            model="gemini-2.5-flash", contents=prompt
        )
        return response.text.strip(), "gemini"
    except Exception as e:
        if is_quota_error(e):
            print(f"[Gemini] Daily quota exhausted, using fallback: {e}")
            return None, "quota_exceeded"
        print(f"[Gemini] Call failed, using fallback: {e}")
        return None, "error"


def run_engine(args: list[str]) -> dict:
    """Runs chess_bridge.exe with the given args (cwd = engine/, so the
    binary's relative 'stockfish.exe' path resolves), parses the JSON
    it prints to stdout, and returns it as a dict."""
    result = subprocess.run(
        [EXE_PATH] + args,
        capture_output=True, text=True, universal_newlines=True, cwd=ENGINE_DIR,
    )
    if result.stderr:
        print(f"[engine stderr]\n{result.stderr}")

    raw = result.stdout
    start = raw.find('{')
    end = raw.rfind('}')
    if start == -1 or end == -1:
        print(f"NO JSON FOUND IN ENGINE OUTPUT. Raw stdout was:\n{raw!r}")
        return {"success": False, "error": "no_json"}
    try:
        report = json.loads(raw[start:end + 1])
    except Exception as e:
        print(f"JSON PARSE ERROR: {e}. Raw output was:\n{raw}")
        return {"success": False, "error": "bad_json"}

    if not report.get("success", True):
        print(f"Engine reported failure: {report}")
    return report


FAST_PATH_FIELDS = [
    "move_number", "san", "quality", "color", "cp_loss", "eval_cp",
    "mover_is_white", "motifs", "best_alternative", "best_alternative_pv",
    "coaching_prompt", "fallback_note", "engine_move",
]

ENDGAME_FIELDS = [
    "accuracy_pct", "acpl", "blunders", "mistakes", "inaccuracies", "phase_acpl",
    "sliders", "engine_config", "new_profile", "updated_counts", "homework_motifs",
    "motif_vector",
]


@app.post("/analyze")
async def analyze(data: AnalyzeRequest):
    """FAST PATH - called after every move the player makes. Analyzes ONLY
    that last move and gets the engine's reply move in the same call."""
    moves = data.moves
    print(f"\nAnalyzing move {len(moves)}: {moves[-1] if moves else '(none)'}")

    # main.cpp's fast path signature is: <time_spent_seconds> <move1> ... <moveN>
    # time_spent_seconds MUST come first or the C++ side misparses the move list.
    args = [str(data.time_spent_seconds)] + moves
    report = run_engine(args)

    if not report.get("success"):
        return {
            "success": False,
            "quality": "Error",
            "cp_loss": 0,
            "ai_note": "Engine communication failed - check the backend terminal for details.",
            "best_alternative": "-",
            "engine_move": None,
        }

    missing = [f for f in FAST_PATH_FIELDS if f not in report]
    if missing:
        print(f"ENGINE OUTPUT MISSING FIELDS {missing}. This almost always means "
              f"chess_bridge.exe was not rebuilt from the latest main.cpp / "
              f"module4_coach.h - rebuild it and try again.")
        return {
            "success": False,
            "quality": "Error",
            "cp_loss": 0,
            "ai_note": f"Engine output is missing fields {missing} - rebuild chess_bridge.exe.",
            "best_alternative": "-",
            "engine_move": None,
        }

    # IMPORTANT: we deliberately do NOT call Gemini here. That call can take
    # a few seconds (or hit the daily quota), and this endpoint needs to
    # return fast so the board/eval-bar/analysis panel update instantly and
    # the bot's reply move plays without delay. The frontend calls
    # /coach-note right after this resolves to fill in the AI note
    # asynchronously, using coaching_prompt/fallback_note below.
    return {
        "success": True,
        "move_number": report["move_number"],
        "san": report["san"],
        "quality": report["quality"],
        "color": report["color"],
        "cp_loss": report["cp_loss"],
        "eval_cp": report["eval_cp"],
        "mover_is_white": report["mover_is_white"],
        "motifs": report["motifs"],
        "is_top_engine_choice": report.get("is_top_engine_choice", False),
        "best_alternative": report["best_alternative"],
        "best_alternative_pv": report["best_alternative_pv"],
        "coaching_prompt": report["coaching_prompt"],
        "fallback_note": report["fallback_note"],
        "engine_move": report["engine_move"],
        "total_moves": len(moves),
    }


@app.post("/coach-note")
async def coach_note(data: CoachNoteRequest):
    """Called right after /analyze resolves. Does the (slow / quota-limited)
    Gemini call and returns just the coaching note, so it never blocks the
    board update or the engine's reply move."""
    note, source = call_gemini(data.coaching_prompt)
    if note is None:
        note = data.fallback_note or "No note available for this move."
    return {"ai_note": note, "source": source}


@app.post("/endgame")
async def endgame(data: EndgameRequest):
    """SLOW PATH - call ONCE when a game ends. Runs full-game analysis,
    updates personalization (Module 5), persists everything to the
    database (Module 6), and returns the new sliders/Elo/homework."""
    database.get_or_create_player(data.player_id)
    p_old, counts = database.get_profile(data.player_id)

    p_old_csv = ",".join(str(v) for v in p_old)
    counts_csv = ",".join(str(v) for v in counts)

    args = ["--endgame", p_old_csv, counts_csv] + data.moves
    report = run_engine(args)

    if not report.get("success", True):
        return {"success": False, "error": report.get("error", "engine_failed")}

    missing = [f for f in ENDGAME_FIELDS if f not in report]
    if missing:
        print(f"ENDGAME OUTPUT MISSING FIELDS {missing}. Rebuild chess_bridge.exe "
              f"from the latest main.cpp - the --endgame path needs new_profile/"
              f"homework_motifs/etc. that older builds don't emit.")
        return {"success": False, "error": f"missing_fields:{missing}"}

    database.save_profile(data.player_id, report["new_profile"], report["updated_counts"])
    database.save_study_plan(data.player_id, report["homework_motifs"])
    database.save_engine_state(data.player_id, report["sliders"], report["engine_config"])

    new_elo = None
    if data.result is not None:
        player = database.get_or_create_player(data.player_id)
        engine_elo = float(report["engine_config"]["uci_elo"])
        new_elo = database.compute_elo(player["elo"], data.result, engine_elo=engine_elo)
        database.update_elo(data.player_id, new_elo)

    database.save_game_record(
        player_id=data.player_id,
        moves=data.moves,
        x_vec=report["motif_vector"],
        accuracy_pct=report["accuracy_pct"],
        acpl=report["acpl"],
        phase_acpl=report["phase_acpl"],
        elo_after=new_elo,
        moves_detail=report.get("moves"),
    )

    return {
        "success": True,
        "accuracy_pct": report["accuracy_pct"],
        "acpl": report["acpl"],
        "blunders": report["blunders"],
        "mistakes": report["mistakes"],
        "inaccuracies": report["inaccuracies"],
        "phase_acpl": report["phase_acpl"],
        "sliders": report["sliders"],
        "engine_config": report["engine_config"],
        "homework_motifs": report["homework_motifs"],
        "elo": new_elo,
    }


@app.get("/profile/{player_id}")
async def profile(player_id: int):
    database.get_or_create_player(player_id)
    return database.get_player_dashboard(player_id)

@app.get("/games/{player_id}")
async def games_list(player_id: int):
    return {"games": database.list_games(player_id)}


@app.get("/game/{game_id}")
async def game_detail(game_id: int):
    game = database.get_game(game_id)
    if game is None:
        return {"success": False, "error": "not_found"}
    return {"success": True, "game": game}

@app.post("/chat")
async def chat(request: ChatRequest):
    prompt = (
        "You are a friendly chess coach chatting live with a student during a game. "
        f"Current board FEN: {request.fen}. Keep answers short (1-3 sentences) and "
        f"conversational.\n\nStudent asks: {request.message}"
    )
    note, source = call_gemini(prompt)
    if note is not None:
        return {"reply": note, "source": source}
    if source == "quota_exceeded":
        return {
            "reply": "I'm out of AI coaching quota for today (free tier limit reached). "
                     "The move-by-move engine analysis will keep working fine - live chat "
                     "will be back once the quota resets.",
            "source": source,
        }
    return {
        "reply": "Sorry, I couldn't reach the coaching AI just now. Try again in a moment.",
        "source": source,
    }


if __name__ == "__main__":
    import uvicorn
    uvicorn.run("app:app", host="0.0.0.0", port=8000, reload=True)
