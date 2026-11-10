#include "Game.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>

namespace compintel
{
namespace
{
constexpr std::array<Position, 4> Cardinal{Position{0, -1}, Position{1, 0}, Position{0, 1}, Position{-1, 0}};
constexpr std::array<Position, 4> Lines{Position{1, 0}, Position{0, 1}, Position{1, 1}, Position{1, -1}};
constexpr int Unreachable = 10000;

bool Fail(std::string* error, char const* message)
{
    if (error) *error = message;
    return false;
}

bool Whitespace(char ch) noexcept
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f' || ch == '\v';
}

bool Conflicts(Move a, Move b) noexcept
{
    if (a.orientation != b.orientation) return a.x == b.x && a.y == b.y;
    return a.orientation == 0 ? a.x == b.x && std::abs(a.y - b.y) <= 1
                              : a.y == b.y && std::abs(a.x - b.x) <= 1;
}
}

Game::Game(GameKind kind) : m_kind(kind)
{
    m_board.fill(-1);
}

bool Game::Inside(Position p) const noexcept
{
    return p.x >= 0 && p.y >= 0 && p.x < Size() && p.y < Size();
}

int Game::At(int x, int y) const noexcept
{
    if (!Inside({x, y})) return -1;
    if (m_kind == GameKind::Gomoku) return m_board[static_cast<size_t>(y * 15 + x)];
    for (int seat = 0; seat < 2; ++seat)
        if (m_pawns[static_cast<size_t>(seat)] == Position{x, y}) return seat;
    return -1;
}

Position Game::Pawn(int seat) const noexcept
{
    return seat >= 0 && seat < 2 ? m_pawns[static_cast<size_t>(seat)] : Position{-1, -1};
}

int Game::WallsRemaining(int seat) const noexcept
{
    return seat >= 0 && seat < 2 ? m_wallsRemaining[static_cast<size_t>(seat)] : 0;
}

bool Game::Blocked(Position from, Position to) const noexcept
{
    for (auto const& wall : m_walls)
    {
        if (from.x != to.x)
        {
            if (wall.orientation == 0 && wall.x == std::max(from.x, to.x) &&
                (wall.y == from.y || wall.y == from.y + 1)) return true;
        }
        else if (wall.orientation == 1 && wall.y == std::max(from.y, to.y) &&
                 (wall.x == from.x || wall.x == from.x + 1)) return true;
    }
    return false;
}

std::vector<Move> Game::LegalPawnMoves(int seat) const
{
    std::vector<Move> moves;
    if (seat < 0 || seat > 1 || Finished()) return moves;
    if (m_kind == GameKind::Gomoku)
    {
        for (int y = 0; y < 15; ++y)
            for (int x = 0; x < 15; ++x)
                if (At(x, y) == -1) moves.push_back({x, y});
        return moves;
    }
    auto origin = Pawn(seat);
    auto opponent = Pawn(1 - seat);
    for (auto d : Cardinal)
    {
        Position adjacent{origin.x + d.x, origin.y + d.y};
        if (!Inside(adjacent) || Blocked(origin, adjacent)) continue;
        if (adjacent != opponent)
        {
            moves.push_back({adjacent.x, adjacent.y});
            continue;
        }
        Position behind{opponent.x + d.x, opponent.y + d.y};
        if (Inside(behind) && !Blocked(opponent, behind))
        {
            moves.push_back({behind.x, behind.y});
            continue;
        }
        for (int sign : {-1, 1})
        {
            Position side{opponent.x + (d.x == 0 ? sign : 0), opponent.y + (d.y == 0 ? sign : 0)};
            if (Inside(side) && !Blocked(opponent, side)) moves.push_back({side.x, side.y});
        }
    }
    return moves;
}

int Game::DistanceToGoal(int seat, Position start) const
{
    std::array<int, 81> distance;
    distance.fill(-1);
    std::queue<Position> queue;
    queue.push(start);
    distance[static_cast<size_t>(start.y * 9 + start.x)] = 0;
    while (!queue.empty())
    {
        auto current = queue.front();
        queue.pop();
        auto steps = distance[static_cast<size_t>(current.y * 9 + current.x)];
        if (current.y == (seat == 0 ? 8 : 0)) return steps;
        for (auto d : Cardinal)
        {
            Position next{current.x + d.x, current.y + d.y};
            if (!Inside(next) || Blocked(current, next)) continue;
            auto index = static_cast<size_t>(next.y * 9 + next.x);
            if (distance[index] >= 0) continue;
            distance[index] = steps + 1;
            queue.push(next);
        }
    }
    return Unreachable;
}

bool Game::Validate(Move move, std::string* error) const
{
    if (Finished()) return Fail(error, "The game has already finished.");
    if (m_kind == GameKind::Gomoku)
    {
        if (move.wall) return Fail(error, "Gomoku does not support walls.");
        if (!Inside({move.x, move.y})) return Fail(error, "Coordinates must be inside 0..14.");
        if (At(move.x, move.y) >= 0) return Fail(error, "That intersection is occupied.");
        return true;
    }
    if (!move.wall)
    {
        auto candidates = LegalPawnMoves(m_nextSeat);
        if (std::find(candidates.begin(), candidates.end(), move) == candidates.end())
            return Fail(error, "Illegal pawn move: check walls, adjacency and jump rules.");
        return true;
    }
    if (WallsRemaining(m_nextSeat) == 0) return Fail(error, "No walls remaining.");
    if (move.x < 1 || move.y < 1 || move.x > 8 || move.y > 8)
        return Fail(error, "Wall centers must be inside 1..8.");
    if (move.orientation != 0 && move.orientation != 1)
        return Fail(error, "Wall orientation must be 0 (vertical) or 1 (horizontal).");
    for (auto const& wall : m_walls)
        if (Conflicts(wall, move)) return Fail(error, "The wall overlaps or crosses an existing wall.");
    auto probe = *this;
    probe.m_walls.push_back(move);
    if (probe.DistanceToGoal(0, Pawn(0)) == Unreachable || probe.DistanceToGoal(1, Pawn(1)) == Unreachable)
        return Fail(error, "The wall would block every path to a goal edge.");
    return true;
}

bool Game::IsLegal(Move move) const
{
    return Validate(move, nullptr);
}

bool Game::HasFive(int seat, Move move) const noexcept
{
    for (auto d : Lines)
    {
        int count = 1;
        for (int sign : {-1, 1})
        {
            Position p{move.x + d.x * sign, move.y + d.y * sign};
            while (Inside(p) && At(p.x, p.y) == seat)
            {
                ++count;
                p.x += d.x * sign;
                p.y += d.y * sign;
            }
        }
        if (count >= 5) return true;
    }
    return false;
}

bool Game::Play(Move move, std::string* error)
{
    if (!Validate(move, error)) return false;
    if (error) error->clear();
    int seat = m_nextSeat;
    if (m_kind == GameKind::Gomoku) m_board[static_cast<size_t>(move.y * 15 + move.x)] = seat;
    else if (move.wall)
    {
        m_walls.push_back(move);
        --m_wallsRemaining[static_cast<size_t>(seat)];
    }
    else m_pawns[static_cast<size_t>(seat)] = {move.x, move.y};
    m_history.push_back({move, seat});
    bool win = m_kind == GameKind::Gomoku ? HasFive(seat, move) : Pawn(seat).y == (seat == 0 ? 8 : 0);
    if (win)
    {
        m_result = "win";
        m_winner = seat;
        m_finishReason = m_kind == GameKind::Gomoku ? "five_in_a_row" : "goal_reached";
    }
    else if (m_kind == GameKind::Gomoku && m_history.size() == 225)
    {
        m_result = "draw";
        m_finishReason = "board_full";
    }
    else if (m_kind == GameKind::Quoridor && m_history.size() >= 200)
    {
        m_result = "move_limit";
        m_finishReason = "move_limit";
    }
    else m_nextSeat = 1 - seat;
    return true;
}

bool Game::Undo()
{
    if (m_history.empty())
    {
        if (m_result != "forfeit") return false;
        *this = Game(m_kind);
        return true;
    }
    auto history = m_history;
    history.pop_back();
    *this = Game(m_kind);
    for (auto const& entry : history) Play(entry.move);
    return true;
}

void Game::Forfeit(int seat, std::string reason)
{
    if (Finished() || seat < 0 || seat > 1) return;
    m_result = "forfeit";
    m_winner = 1 - seat;
    m_finishReason = std::move(reason);
}

int Game::GomokuScore(int seat, Move move) const noexcept
{
    int total = 0;
    for (auto d : Lines)
    {
        int count = 1;
        int open = 0;
        for (int sign : {-1, 1})
        {
            Position p{move.x + d.x * sign, move.y + d.y * sign};
            while (Inside(p) && At(p.x, p.y) == seat)
            {
                ++count;
                p.x += d.x * sign;
                p.y += d.y * sign;
            }
            if (Inside(p) && At(p.x, p.y) == -1) ++open;
        }
        if (count >= 5) return 10000000;
        if (open == 0) continue;
        constexpr std::array<int, 5> score{0, 2, 30, 600, 20000};
        total += score[static_cast<size_t>(count)] * (open == 2 ? 6 : 1);
    }
    return total;
}

Move Game::SuggestMove() const
{
    if (Finished()) return {-1, -1};
    auto candidates = LegalPawnMoves(m_nextSeat);
    if (candidates.empty()) return {-1, -1};
    Move best = candidates.front();
    if (m_kind == GameKind::Gomoku)
    {
        long long bestScore = std::numeric_limits<long long>::min();
        for (auto move : candidates)
        {
            int offense = GomokuScore(m_nextSeat, move);
            int defense = GomokuScore(1 - m_nextSeat, move);
            long long score;
            if (offense >= 10000000) score = 1000000000;
            else if (defense >= 10000000) score = 100000000;
            else score = static_cast<long long>(offense) * 10 + static_cast<long long>(defense) * 9;
            score = score * 100 - std::abs(move.x - 7) - std::abs(move.y - 7);
            if (score > bestScore)
            {
                best = move;
                bestScore = score;
            }
        }
        return best;
    }
    int distance = Unreachable;
    for (auto move : candidates)
    {
        int candidateDistance = DistanceToGoal(m_nextSeat, {move.x, move.y});
        if (candidateDistance < distance)
        {
            distance = candidateDistance;
            best = move;
        }
    }
    return best;
}

std::string Game::Initialization(int seat) const
{
    if (seat < 0 || seat > 1) throw std::invalid_argument("Seat must be 0 or 1.");
    return m_kind == GameKind::Gomoku ? "1\n15 15\n" + std::to_string(seat) + "\n"
                                    : "1 " + std::to_string(seat) + "\n";
}

std::string Game::FormatMove(Move move) const
{
    std::string coordinates = std::to_string(move.x) + " " + std::to_string(move.y);
    if (m_kind == GameKind::Gomoku) return coordinates + "\n";
    return move.wall ? "1 " + coordinates + " " + std::to_string(move.orientation) + "\n"
                     : "0 " + coordinates + "\n";
}

bool Game::ParseMove(std::string_view output, Move& move, std::string* error) const
{
    std::vector<int> values;
    size_t at = 0;
    while (at < output.size())
    {
        while (at < output.size() && Whitespace(output[at])) ++at;
        if (at == output.size()) break;
        auto first = at;
        while (at < output.size() && !Whitespace(output[at])) ++at;
        auto token = output.substr(first, at - first);
        if (token.empty() || token[0] < '0' || token[0] > '9' || (token.size() > 1 && token[0] == '0'))
            return Fail(error, "Move fields must be canonical non-negative decimal integers.");
        int number = 0;
        auto parsed = std::from_chars(token.data(), token.data() + token.size(), number);
        if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size())
            return Fail(error, "Move fields must be non-negative decimal integers.");
        values.push_back(number);
        if (values.size() > 4) return Fail(error, "Unexpected extra move fields.");
    }
    Move candidate;
    if (m_kind == GameKind::Gomoku)
    {
        if (values.size() != 2) return Fail(error, "Expected exactly two integers: x y.");
        candidate = {values[0], values[1]};
        if (!Inside({candidate.x, candidate.y})) return Fail(error, "Gomoku coordinates must be inside 0..14.");
    }
    else
    {
        if (values.size() == 3 && values[0] == 0) candidate = {values[1], values[2]};
        else if (values.size() == 4 && values[0] == 1 && values[3] <= 1)
            candidate = {values[1], values[2], true, values[3]};
        else return Fail(error, "Expected 0 x y or 1 x y orientation (0 or 1).");
        if (candidate.wall ? candidate.x < 1 || candidate.x > 8 || candidate.y < 1 || candidate.y > 8
                           : !Inside({candidate.x, candidate.y}))
            return Fail(error, "Quoridor move coordinates are outside the board.");
    }
    move = candidate;
    if (error) error->clear();
    return true;
}
}
