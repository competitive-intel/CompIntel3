#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace compintel
{
enum class GameKind { Gomoku, Quoridor };

struct Position
{
    int x{};
    int y{};
    bool operator==(Position const&) const = default;
};

struct Move
{
    int x{};
    int y{};
    bool wall{false};
    int orientation{}; // Quoridor: 0 vertical; 1 horizontal.
    bool operator==(Move const&) const = default;
};

struct PlayedMove
{
    Move move;
    int seat{};
};

// Rules and protocol v1, independent from Windows and the UI.
// Seat 0 starts (white in Gomoku). Coordinates increase rightward/downward.
class Game
{
public:
    explicit Game(GameKind kind = GameKind::Gomoku);

    GameKind Kind() const noexcept { return m_kind; }
    int Size() const noexcept { return m_kind == GameKind::Gomoku ? 15 : 9; }
    int NextSeat() const noexcept { return m_nextSeat; }
    int Winner() const noexcept { return m_winner; } // -1 while playing or drawn.
    bool Finished() const noexcept { return m_result != "playing"; }
    std::string const& Result() const noexcept { return m_result; }
    std::string const& FinishReason() const noexcept { return m_finishReason; }

    int At(int x, int y) const noexcept; // -1 empty / outside; otherwise seat 0 or 1.
    Position Pawn(int seat) const noexcept;
    int WallsRemaining(int seat) const noexcept;
    std::vector<PlayedMove> const& History() const noexcept { return m_history; }
    std::vector<Move> const& Walls() const noexcept { return m_walls; }
    std::vector<Move> LegalPawnMoves(int seat) const;
    bool IsLegal(Move move) const;
    bool Play(Move move, std::string* error = nullptr);
    bool Undo();
    void Forfeit(int seat, std::string reason);
    Move SuggestMove() const;

    std::string Initialization(int seat) const;
    std::string FormatMove(Move move) const;
    bool ParseMove(std::string_view output, Move& move, std::string* error = nullptr) const;

private:
    bool Inside(Position p) const noexcept;
    bool Blocked(Position from, Position to) const noexcept;
    int DistanceToGoal(int seat, Position start) const;
    bool Validate(Move move, std::string* error) const;
    bool HasFive(int seat, Move move) const noexcept;
    int GomokuScore(int seat, Move move) const noexcept;

    GameKind m_kind;
    std::array<int, 225> m_board{};
    std::array<Position, 2> m_pawns{Position{4, 0}, Position{4, 8}};
    std::array<int, 2> m_wallsRemaining{10, 10};
    std::vector<PlayedMove> m_history;
    std::vector<Move> m_walls;
    int m_nextSeat{0};
    int m_winner{-1};
    std::string m_result{"playing"};
    std::string m_finishReason;
};
}
