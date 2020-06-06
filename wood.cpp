#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <queue>
#include <map>

using namespace std;

template<class T>
T mod(T a, T b) {
    T z = a % b;
    if (a < 0) {
        return z + b;
    }
    else {
        return z;
    }
}

class Pacman {
public:
    int pacId; // pac number (unique within a team)
    bool mine; // true if this pac is yours
    int x; // position in the grid
    int y; // position in the grid
    string typeId; // unused in wood leagues
    int speedTurnsLeft; // unused in wood leagues
    int abilityCooldown; // unused in wood leagues    

    friend ostream& operator<<(ostream& out, const Pacman& pac);
};
ostream& operator<<(ostream& out, const Pacman& pac) {
    out << "Pacman: " << "Id:"<< pac.pacId << " x:"<< pac.x << " y:" << pac.y << " mine:" << pac.mine;
    return out;
}


class Pellet {
public:
    int x;
    int y;
    int value; // amount of points this pellet is worth
    
    friend ostream& operator<<(ostream& out, const Pellet& pellet);
};
ostream& operator<<(ostream& out, const Pellet& pellet) {
    out << "Pellet: " << "x:" << pellet.x << " y:"<< pellet.y << " value:" << pellet.value;
    return out;
}

class Tile {
public:
    int x=-1, y=-1;
    int pelletValue;
    vector<Tile*> neighbours;
    
    Tile() {}
    Tile(int x, int y) :
        x(x), y(y)
    {}

    friend ostream& operator<<(ostream& out, const Tile& tile);
};
ostream& operator<<(ostream& out, const Tile& tile) {
    out << "Tile: " << "x:" << tile.x << " y:"<< tile.y << " pelletValue:" << tile.pelletValue << " neighbours: ";
    for (Tile* neighbour : tile.neighbours) {
        out << "(" << neighbour->x << "," << neighbour->y << ") ";
    }
    return out;
}


using Coord = pair<int, int>;   // (x, y) which is equivalent to (col, row). x & y must be positive.

class Board {
public:
    map<Coord, Tile> tiles; // Like an adjacency list
    int width, height;

    Board() {}

    bool isInBounds(int x, int y) {
        if (x >= 0 && x < width && y >=0 && y < height) return true;
        return false;
    }

    void clearPellets() {
        for (auto& [_, tile] : tiles) {
            tile.pelletValue = 0;
        }
    }
};


class Game {    // Main class, like the Solution class.
public:
    int myScore;
    int opponentScore;
    int visiblePacCount; // all your pacs and enemy pacs in sight
    
    Pacman mypac;
    Pacman theirPac;
    int visiblePelletCount; // all pellets in sight

    Board board;

    Game() {}

    void buildBoard(vector<string>& grid) {
        board.height = grid.size();
        board.width = grid[0].size();

        // Add Nodes to graph
        for (int y = 0; y < board.height; y++) {
            for (int x = 0; x < board.width; x++) {
                if (grid[y][x] == ' ') {
                    board.tiles.at({x, y}) = Tile(x, y);
                }
            }
        }

        // Connect the tiles/nodes:
        for (int y = 0; y < board.height; y++) {
            for (int x = 0; x < board.width; x++) {
                if (grid[y][x] == ' ') {                    
                    for (int adjX : {x+1, x-1}) {
                        for (int adjY : {y-1, y+1}) {
                            if (adjX < 0) x = mod(adjX, board.width);
                            if (adjY < 0) x = mod(adjY, board.height);
                            if (board.isInBounds(adjX, adjY) && grid[adjY][adjY] == ' ') {
                                board.tiles.at({x,y}).neighbours.push_back(&board.tiles.at({adjX, adjY}));
                            }
                        }
                    }
                }
            }
        }
    }


    void step() {
        cerr << "Taking step" << endl;
        
        // Find closest pellet
        Tile* closestPelletTile = findClosestPellet(mypac.x, mypac.y);
        
        if (closestPelletTile) {
            // Move to it
            cerr << "Closest Pellet: " << *closestPelletTile << endl;
            outputMove(mypac.pacId, closestPelletTile->x, closestPelletTile->y);        
        }
        else {
            cerr << "No tile found with pellet" << endl;
            // Do nothing.
        }        
    }

    // Pellet& findClosestPelletManhattan(int fromX, int fromY) {
    //     auto minPelletIter = min_element(pellets.begin(), pellets.end(), [&](const Pellet& p1, const Pellet& p2) {
    //         int p1dist = abs(p1.x - fromX) + abs(p1.y - fromY);
    //         int p2dist = abs(p2.x - fromX) + abs(p2.y - fromY);
    //         return p1dist < p2dist;
    //     });
    //     return *minPelletIter;
    // }

    Tile* findClosestPellet(int fromX, int fromY) {        // Q: Would rather return a Tile& but std::optional<> cannot hold references. What's the recommendation here?
        /// Find closest pellet by BFS on the graph.
        queue<Tile*> q;
        bool visited[board.width][board.height];    // discovered.
        q.push(&board.tiles.at({fromX, fromY}));
        visited[fromX][fromY] = true;

        while (!q.empty()) {
            Tile* currTile = q.front(); q.pop();
            if(currTile->pelletValue > 0) {
                return currTile;
            }

            for (Tile* neighbour : currTile->neighbours) {
                if (!visited[neighbour->x][neighbour->y]) {
                    q.push(neighbour);
                    visited[neighbour->x][neighbour->y] = true;
                }
            }
        }
        return nullptr;
    }

    void outputMove(int pacId, int x, int y) {
        // Write an action using cout. DON'T FORGET THE "<< endl"
        // To debug: cerr << "Debug messages..." << endl;        
        cout << "MOVE " << pacId << " " << x << " " << y << endl; // MOVE <pacId> <x> <y>
    }

};


int main()
{    
    Game game;

    int width; // size of the grid
    int height; // top left corner is (x=0, y=0)
    cin >> width >> height; cin.ignore();
    vector<string> grid;
    for (int i = 0; i < height; i++) {
        getline(cin, grid[i]); // one line of the grid: space " " is floor, pound "#" is wall
    }    
    game.buildBoard(grid);

    // game loop
    while (1) {
        // INPUTS:
        cin >> game.myScore >> game.opponentScore; cin.ignore();
        cin >> game.visiblePacCount; cin.ignore();

        for (int i = 0; i < game.visiblePacCount; i++) {
            int pacId; // pac number (unique within a team)
            bool mine; // true if this pac is yours
            int x; // position in the grid
            int y; // position in the grid
            string typeId; // unused in wood leagues
            int speedTurnsLeft; // unused in wood leagues
            int abilityCooldown; // unused in wood leagues

            Pacman temp;
            // cin >> pacId >> mine >> x >> y >> typeId >> speedTurnsLeft >> abilityCooldown; cin.ignore();
            cin >> temp.pacId >> temp.mine >> temp.x >> temp.y >> temp.typeId >> temp.speedTurnsLeft >> temp.abilityCooldown; cin.ignore();        
            if(temp.mine) {
                game.mypac = temp;
            }
            else {
                game.theirPac = temp;
            }
        }
        
        // Input Pellets:
        cin >> game.visiblePelletCount; cin.ignore();
        for (int i = 0; i < game.visiblePelletCount; i++) {         
            int x, y, value;   
            cin >> x >> y >> value; cin.ignore();
            game.board.tiles.at({x,y}).pelletValue = value;
        }
        

        // TAKE ACTION
        game.step();
    }
}