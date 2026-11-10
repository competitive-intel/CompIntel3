#include "core/Game.h"
#include "runtime/PlayerProcess.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace compintel;

namespace
{
void Require(bool condition, std::string const& message)
{
    if (!condition) throw std::runtime_error(message);
}

void Match(std::filesystem::path const& executable, GameKind kind, int seat, bool useWalls)
{
    Game game(kind);
    runtime::PlayerProcess process(executable);
    std::string error;
    Require(process.Start(&error), "Cannot start example: " + error);
    std::string input = game.Initialization(seat);
    size_t ownTurns = 0;
    size_t opponentTurns = 0;
    size_t wallsPlaced = 0;
    while (!game.Finished())
    {
        if (game.NextSeat() == seat)
        {
            auto turn = process.Turn(input, std::chrono::milliseconds(1000));
            input.clear();
            Require(turn.success, "Example failed to flush a valid turn: " + turn.diagnostics);
            Move move;
            Require(game.ParseMove(turn.line, move, &error), "Malformed example output: " + turn.line + " " + error);
            Require(game.Play(move, &error), "Illegal example output: " + turn.line + " " + error);
            ++ownTurns;
        }
        else
        {
            Move move = game.SuggestMove();
            // Exercise the sample's persistent wall state as well as pawn updates.
            if (useWalls && opponentTurns % 3 == 0 && game.WallsRemaining(1 - seat) > 0)
            {
                bool found = false;
                auto target = game.Pawn(seat);
                for (int y = 1; y <= 8 && !found; ++y)
                    for (int x = 1; x <= 8 && !found; ++x)
                    {
                        Move wall{x, y, true, 1};
                        if (std::abs(y - target.y) <= 2 && game.IsLegal(wall))
                        {
                            move = wall;
                            found = true;
                            ++wallsPlaced;
                        }
                    }
            }
            Require(game.Play(move, &error), "Internal opponent move failed: " + error);
            input += game.FormatMove(move);
            ++opponentTurns;
        }
    }
    process.Stop();
    Require(!process.Running(), "Example process must terminate with the match");
    Require(ownTurns > 0, "Example must play at least one turn");
    if (useWalls) Require(wallsPlaced > 0, "Wall test must exercise opponent wall updates");
    Require(game.Result() == "win" || game.Result() == "draw" || game.Result() == "move_limit", "Match ends normally");
    std::cout << "PASS " << (kind == GameKind::Gomoku ? "gomoku" : "quoridor")
              << " seat " << seat << (useWalls ? " with opponent walls" : "")
              << ": " << game.History().size() << " legal moves, " << game.Result() << '\n';
}
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 3)
    {
        std::cerr << "Expected Gomoku and Quoridor example executable paths\n";
        return 1;
    }
    try
    {
        for (int seat = 0; seat < 2; ++seat)
        {
            Match(argv[1], GameKind::Gomoku, seat, false);
            Match(argv[2], GameKind::Quoridor, seat, false);
            Match(argv[2], GameKind::Quoridor, seat, true);
        }
        return 0;
    }
    catch (std::exception const& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
