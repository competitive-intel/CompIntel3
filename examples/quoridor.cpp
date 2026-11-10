// CompIntel3 / Quoridor v1. A standalone shortest-path player.
// Walls use internal intersection coordinates 1..8; 0=vertical, 1=horizontal.
#include <iostream>
#include <queue>
#include <vector>
#include <utility>
#include <algorithm>

using Pos = std::pair<int,int>;
struct Wall { int x, y, orientation; };
std::vector<Wall> walls;
Pos pawn[2] = {{4,0},{4,8}};
constexpr int directions[4][2] = {{0,1},{1,0},{0,-1},{-1,0}};
bool inside(Pos p) { return p.first >= 0 && p.first < 9 && p.second >= 0 && p.second < 9; }
Pos add(Pos p, int x, int y) { return {p.first+x, p.second+y}; }
bool open(Pos a, Pos b) {
    if (!inside(b)) return false;
    for (auto w : walls) {
        if (a.first != b.first && w.orientation == 0 &&
            w.x == std::max(a.first, b.first) && (a.second == w.y-1 || a.second == w.y)) return false;
        if (a.second != b.second && w.orientation == 1 &&
            w.y == std::max(a.second, b.second) && (a.first == w.x-1 || a.first == w.x)) return false;
    }
    return true;
}
std::vector<Pos> legal(int seat) {
    std::vector<Pos> moves;
    auto p = pawn[seat], opponent = pawn[1-seat];
    for (auto const& d : directions) {
        auto q = add(p,d[0],d[1]);
        if (!open(p,q)) continue;
        if (q != opponent) { moves.push_back(q); continue; }
        auto jump = add(q,d[0],d[1]);
        if (open(q,jump)) moves.push_back(jump);
        else for (int sign : {-1,1}) {
            auto side = add(q,sign*d[1],sign*d[0]);
            if (open(q,side)) moves.push_back(side);
        }
    }
    return moves;
}
int distance(Pos p, int goal) {
    int dist[9][9];
    for (auto& row : dist) for (auto& cell : row) cell = -1;
    std::queue<Pos> queue; queue.push(p); dist[p.first][p.second] = 0;
    while (!queue.empty()) {
        auto a = queue.front(); queue.pop();
        if (a.second == goal) return dist[a.first][a.second];
        for (auto const& d : directions) {
            auto b = add(a,d[0],d[1]);
            if (open(a,b) && dist[b.first][b.second] == -1) {
                dist[b.first][b.second] = dist[a.first][a.second]+1;
                queue.push(b);
            }
        }
    }
    return 999;
}
int main() {
    int version, seat;
    if (!(std::cin >> version >> seat) || version != 1 || seat < 0 || seat > 1) return 0;
    bool first = seat == 0;
    for (;;) {
        if (!first) {
            int type, x, y, orientation;
            if (!(std::cin >> type >> x >> y)) break;
            if (type == 0) pawn[1-seat] = {x,y};
            else if (type == 1 && std::cin >> orientation) walls.push_back({x,y,orientation});
            else break;
        }
        first = false;
        auto moves = legal(seat);
        if (moves.empty()) break;
        auto best = *std::min_element(moves.begin(), moves.end(), [=](Pos a, Pos b) {
            return distance(a,seat == 0 ? 8 : 0) < distance(b,seat == 0 ? 8 : 0);
        });
        pawn[seat] = best;
        std::cout << "0 " << best.first << ' ' << best.second << std::endl;
    }
}
