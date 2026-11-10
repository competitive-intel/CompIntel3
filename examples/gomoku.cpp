// CompIntel3 / Gomoku v1. Seat 0 is WHITE and moves first.
// Compile with C++20. Keep the process alive and flush after every move.
#include <iostream>
#include <utility>
#include <algorithm>
#include <cstdlib>

int board[15][15]; // 0: empty, 1: first, 2: second
constexpr int dirs[4][2] = {{1,0},{0,1},{1,1},{1,-1}};
bool inside(int x, int y) { return x >= 0 && x < 15 && y >= 0 && y < 15; }

int evaluate(int x, int y, int player) {
    int score = 0;
    for (auto const& d : dirs) {
        int count = 1, open = 0;
        for (int sign : {-1, 1}) {
            int nx = x + sign*d[0], ny = y + sign*d[1];
            while (inside(nx, ny) && board[nx][ny] == player) {
                ++count; nx += sign*d[0]; ny += sign*d[1];
            }
            if (inside(nx, ny) && board[nx][ny] == 0) ++open;
        }
        if (count >= 5) return 1000000;
        static const int weight[] = {0, 1, 12, 160, 4000};
        score += weight[std::min(count, 4)] * (open == 2 ? 5 : open);
    }
    return score;
}

int main() {
    int version, height, width, seat;
    if (!(std::cin >> version >> height >> width >> seat)) return 0;
    if (version != 1 || height != 15 || width != 15 || seat < 0 || seat > 1) return 1;
    int mine = seat + 1, other = 2 - seat;
    bool first = seat == 0;
    for (;;) {
        if (!first) {
            int x, y;
            if (!(std::cin >> x >> y)) break;
            if (!inside(x,y) || board[x][y] != 0) return 1;
            board[x][y] = other;
        }
        first = false;
        int best = -1, bx = -1, by = -1;
        for (int x = 0; x < 15; ++x) for (int y = 0; y < 15; ++y) {
            if (board[x][y]) continue;
            int attack = evaluate(x, y, mine), defense = evaluate(x, y, other);
            int score = attack >= 1000000 ? 200000000 :
                        defense >= 1000000 ? 100000000 :
                        attack * 2 + defense * 3 + 28 - std::abs(x-7) - std::abs(y-7);
            if (score > best) { best = score; bx = x; by = y; }
        }
        if (bx < 0) break;
        board[bx][by] = mine;
        std::cout << bx << ' ' << by << std::endl;
    }
}
