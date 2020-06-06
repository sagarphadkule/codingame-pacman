#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <queue>
#include <map>
#include <assert.h>
#include <sstream>
#include <unordered_set>
#include <set>
#include <math.h>
#include <iomanip>      // std::setprecision

using namespace std;

// GLOBAL HELPERS:
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

template<class K, class V>
auto findValue(std::map<K,V>& someMap, V value) {
    auto it1 = someMap.begin();
    auto it2 = someMap.end();
    while (it1 != it2) {
        if (it1->second == value) return it1;
        ++it1;
    }
    return it2;
}

template<class K, class V>
auto findValue(std::unordered_map<K,V>& someMap, V value) {
    auto it1 = someMap.begin();
    auto it2 = someMap.end();
    while (it1 != it2) {
        if (it1->second == value) return it1;
        ++it1;
    }
    return it2;
}



void printGrid(const vector<string>& grid) {
    for (auto& s : grid) {
        cerr << s << endl;
    }
}



class Pacman;

class Tile {
public:
    int x=-1, y=-1;
    int pelletValue = -1;  // -1 means we don't know if pellet exists there. 0 means pellet does not exist for sure. If >0 means pellet exists of that value.
    double pelletValueAdjusted = 1.0;   // Must be updated each step;
    vector<Tile*> neighbours;

    bool visible = false;

    Pacman* pacOnTile = nullptr;  // If there exists any pacman on this tile.

    Tile() {}
    Tile(int x, int y) :
        x(x), y(y)
    {}

    double getPelletValueAdjusted() const {
        return pelletValueAdjusted;
    }

    string toStr() const {
        stringstream ss;
        ss << "(" << x << "," << y << ")" << setprecision(1) << getPelletValueAdjusted() << setprecision(3);
        return ss.str();
    }

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
using PacDestinationT = map<Tile*, Pacman*>;
using Path = vector<Tile*>;
using PathsValsT = vector<pair<double, Path>>;
using PacRoutesT = map<Pacman*, Path>;

class Board {
public:
    std::map<Coord, Tile> tiles; // Like an adjacency list
    int width, height;
    set<Tile*> superPelletTiles;    // NOTE: These are tiles where Super Pellets WERE present in the beginning of game. The super pellets may not be on the tiles anymore.
    int numUnknownPellets;  // Must be updated each step.
    Board() {}

    bool isInBounds(int x, int y) {
        if (x >= 0 && x < width && y >=0 && y < height) return true;
        return false;
    }
    void sortTileNeighboursByPelletValues() {
        // Sort neighbours of all tiles by their value.
        // This will ensure we choose a Super Pellet before normal Pellet during graph search.

        for (auto& [coord, tile] : tiles) {
            auto& neighbours = tile.neighbours; // reference important.
            auto comp = [](const Tile* t1, const Tile* t2) {
                return t1->pelletValue > t2->pelletValue;
            };
            sort(neighbours.begin(), neighbours.end(), comp);
        }
    }
    void updateNumUnknownPellets() {
        numUnknownPellets = count_if(tiles.begin(), tiles.end(), [](decltype(tiles)::value_type& element) {
            return element.second.pelletValue == -1;
        });
    }
    void updatePelletAdjustValues(int numOpponentAliveInvisiblePacs) {
        // If we have N tiles of unknown pellets and m opponent pacmans, then after n game steps,
        // the expectated value of an unknown pellet being there is roughly of the form:
        //      E = (N - m*n) / N
        //
        // Ideally, n should be number of steps taken by opponent pacmans when they were out of sight.
        // Also, this is an optimistic estimate but the optimism should be compensated by the fact that there
        // are fake game steps that we can't count.
        //
        // Since m can change due to pacmans dying, use incremental update:
        //      E' = E - m/N
        //  Now, if we make m = number of alive opponent pacs that are not visible in this turn (ideally last turn), then
        //  this formula is much more accurate.
        //
        // Also, whenever we see this tile again, we reset it.

        for (auto& [_, tile] : tiles) {
            if (tile.pelletValue == -1) {
                tile.pelletValueAdjusted = tile.pelletValueAdjusted - 1.0*numOpponentAliveInvisiblePacs / numUnknownPellets;

                if (tile.pelletValueAdjusted <= 0.0) {
                    tile.pelletValueAdjusted = 0.1;     // Keep a minimum expected value.
                }
            }
            else {
                // This resets the values of previously unknown tiles.
                tile.pelletValueAdjusted = tile.pelletValue;
            }

        }
    }

};
ostream& operator<<(ostream& out, const Board& board) {
    // for (auto& [key, tile] : board.tiles) {
    //     out << "(" << key.first << "," << key.second << ") : " << tile << endl;
    // }
    unordered_map<int, char> valMap = {
        {-1, '?'},
        {0, '0'},
        {1, '1'},
        {10, '*'},
    };
    vector<string> grid(board.height, string(board.width, '#'));
    for (auto& [key, tile] : board.tiles) {
        grid[tile.y][tile.x] = valMap[tile.pelletValue];
    }
    for (auto& row : grid) {
        out << row << endl;
    }
    return out;
}


class Route {
public:
    Path fullPath;
    Path pathUptoFirstPellet;
    Path additionalPath;
    Tile* firstPelletTile = nullptr;
    double totalReward = 0;
    double additionalPathValue = -9999;
    int horizon = -1;
    vector<Tile*> superPellets;

    Tile* firstStep() {
        return fullPath.size() < 1 ? nullptr : fullPath[0];
    }
    Tile* secondStep() {
        return fullPath.size() < 2 ? nullptr : fullPath[1];
    }
    bool empty() {
        return fullPath.empty();
    }
    bool hasSuperPellets() {
        return !superPellets.empty();
    }
    friend ostream& operator<<(ostream& out, const Route& route);
};
ostream& operator<<(ostream& out, const Route& route) {
    out << "Rt:" << setprecision(3) << route.totalReward << " ";
    for (auto t : route.fullPath) {
        if (t == route.firstPelletTile) out << "[";
        if (t->getPelletValueAdjusted() == 10) out << "<";
        out << t->toStr();
        if (t->getPelletValueAdjusted() == 10) out << ">";
        if (t == route.firstPelletTile) out << "]";
        out << " ";
    }
    return out;
}


class Pacman {
public:
    int pacId; // pac number (unique within a team)
    bool mine; // true if this pac is yours
    Tile* pos;
    string typeId; // unused in wood leagues
    int speedTurnsLeft; // unused in wood leagues
    int abilityCooldown; // unused in wood leagues

    const Board& board;
    bool visible = true;    // Can be false for enemy pacs
    unordered_map<Tile*, int> distanceToTile;

    Route route;    // Will change each step.

    Pacman(int pacId, int mine, Tile* pos, string typeId, int speedTurnsLeft, int abilityCooldown, const Board& b)
     : pacId(pacId), mine(mine), pos(pos), typeId(typeId), speedTurnsLeft(speedTurnsLeft), abilityCooldown(abilityCooldown), board(b)
     {}

    unordered_set<Tile*> getVisibleTiles() {
        unordered_set<Tile*> visibleTiles;
        dfs(pos, visibleTiles);
        return visibleTiles;
    }

    bool operator<(const Pacman& other) {
        return pacId < other.pacId;
    }

    bool isSpeedActive() {
        return speedTurnsLeft > 0;
    }

    Tile* getClaimedTile() {    // Return the pellet tile claimed by this pac on its boundary of pellets
        return route.firstPelletTile;
    }

    Tile* getMoveDestination() {
        if (isSpeedActive()) {
            auto ret = route.secondStep();
            if (!ret) ret = route.firstStep();
            return ret;
        }
        else {
            return route.firstStep();
        }
    }

    Tile* getClosestSuperPellet() {
        int minDist = 9999;
        Tile* closest = nullptr;
        for (auto t : board.superPelletTiles) {
            if (t->pelletValue == 10) {
                if (distanceToTile[t] < minDist) {
                    minDist = distanceToTile[t];
                    closest = t;
                }
            }
        }
        return closest;
    }

    string routeToStr() {
        stringstream out;
        out << "Rt:" << setprecision(3) << route.totalReward << "";
        for (auto t : route.fullPath) {
            if (t == getClaimedTile()) out << "[";
            if (t == getMoveDestination()) out << "{";
            if (t->getPelletValueAdjusted() == 10) out << "<";
            out << t->toStr();
            if (t->getPelletValueAdjusted() == 10) out << ">";
            if (t == getMoveDestination()) out << "}";
            if (t == getClaimedTile()) out << "]";
            out << " ";
        }
        return out.str();
    }

private:
    void dfs(Tile* tile, unordered_set<Tile*>& visibleTiles) {
        if (tile->x == pos->x || tile->y == pos->y) {
            visibleTiles.insert(tile);

            for (auto n : tile->neighbours) {
                if(visibleTiles.count(n) == 0) {
                    dfs(n, visibleTiles);
                }
            }
        }
    }

    friend ostream& operator<<(ostream& out, const Pacman& pac);
};
ostream& operator<<(ostream& out, const Pacman& pac) {
    out << "Pacman: " << "Id:"<< pac.pacId << " x:"<< pac.pos->x << " y:" << pac.pos->y << " mine:" << pac.mine;
    return out;
}


class Game {    // Main class, like the Solution class.
public:
    int gameSteps = 0;
    int myScore;
    int opponentScore;
    int visiblePacCount; // all your pacs and enemy pacs in sight

    map<int, Pacman> myPacs;
    map<int, Pacman> theirPacs;
    map<int, Pacman> theirVisiblePacs;
    map<int, Pacman> myDeadPacs;
    map<int, Pacman> theirDeadPacs;
    int visiblePelletCount; // all pellets in sight

    Board board;

    map<string, string> typeWeakAgainst = {
        {"ROCK", "SCISSORS"},
        {"SCISSORS", "PAPER"},
        {"PAPER", "ROCK"}
    };
    map<string, string> typeStrongAgainst = {
        {"ROCK", "PAPER"},
        {"SCISSORS", "ROCK"},
        {"PAPER", "SCISSORS"}
    };

    map<Pacman*, unordered_map<Tile*, int>> pacDistances;   // Flood-fill distances from each pac in each step.

    Game() {}

    void buildBoard(vector<string>& grid) {
        board.height = grid.size();
        board.width = grid[0].size();

        // Add Nodes to graph
        for (int y = 0; y < board.height; y++) {
            for (int x = 0; x < board.width; x++) {
                if (grid[y][x] == ' ') {
                    board.tiles[{x, y}] = Tile(x, y);
                }
            }
        }

        // Connect the tiles/nodes:
        for (int y = 0; y < board.height; y++) {
            for (int x = 0; x < board.width; x++) {
                if (grid[y][x] == ' ') {
                    assert(board.tiles.find({x,y}) != board.tiles.end());

                    vector<int> nx = {x+1, x-1, x, x};
                    vector<int> ny = {y, y, y+1, y-1};
                                    // E, W, S, N
                    for (int i = 0; i < nx.size(); i++) {
                        int adjX = nx[i];
                        int adjY = ny[i];
                        adjX = mod(adjX, board.width);
                        adjY = mod(adjY, board.height);

                        if (board.isInBounds(adjX, adjY) && grid[adjY][adjX] == ' ') {
                            assert(board.tiles.find({adjX,adjY}) != board.tiles.end());
                            board.tiles.at({x,y}).neighbours.push_back(&board.tiles.at({adjX, adjY}));
                        }
                    }
                }
            }
        }
    }

    // Runs before input:
    void resetPacs() {
        for (auto& [id, pac] : myPacs) {
            pac.pos->pacOnTile = nullptr;
        }
        for (auto& [id, pac] : theirPacs) {
            if (pac.visible) {  // was a visible pac.
                pac.pos->pacOnTile = nullptr;
                pac.visible = false;    // reset for new input.
            }
        }

    }

    // Runs after input:
    void updateVisiblePacsList() {
        theirVisiblePacs.clear();
        for (auto& [id, pac] : theirPacs) {
            if (pac.visible) {  
                theirVisiblePacs.emplace(id, pac);
            }
        }
    }

    /// Marks pellets for all tiles to be unknown (-1) except for the ones that are known to be gone (0).
    /// This function must be run BEFORE receiving input of visible pellets.
    void clearPellets() {
        for (auto& [_, tile] : board.tiles) {
            if(tile.pelletValue != 0) {
                tile.pelletValue = -1;
            }
        }
    }

    /// For all Visible Tiles, if there is no pellet input received (-1), mark pelletValue=0 i.e. gone for sure.
    /// This function must be run AFTER receiving input of visible pellets.
    /// Do similar thing for Super Pellet Tiles.
    void updateGonePellets() {
        for(auto& [id, pac] : myPacs) {
            auto visibleTiles = pac.getVisibleTiles();
            for (Tile* tile : visibleTiles) {
                if (tile->pelletValue == -1) {
                    tile->pelletValue = 0;
                }
            }
        }

        for (Tile* tile: board.superPelletTiles) {
            if (tile->pelletValue == -1) {
                tile->pelletValue = 0;
            }
        }
    }

    void updateGonePelletsBasedOnEnemyPacLocation() {
        for (auto& [id, pac] : theirPacs) {
            if (pac.visible && gameSteps > 0) {
                // Count how many of this pac's neighbouring tiles have a pellet for sure.
                // They definitely did not come from there.
                // If remaining neighbours is only 1, then they came from there. Else we dont know, skip.
                vector<Tile*> potentialSources;
                for (auto t : pac.pos->neighbours) {
                    if (t->pelletValue == 1 || t->pelletValue == 10) {
                        continue;
                    }
                    potentialSources.push_back(t);
                }

                if (potentialSources.size() == 1) {
                    // They came from here. Trace back from here.
                    unordered_set<Tile*> visited = {pac.pos, potentialSources[0]};
                    queue<Tile*> q;
                    int steps = 0;

                    while(!q.empty() && steps < gameSteps) {
                        Tile* curr = q.front(); q.pop(); 
                        curr->pelletValue = 0;
                        
                        if (curr->neighbours.size() == 2) {
                            for (auto& neighbour : curr->neighbours) {
                                if (visited.count(neighbour) == 0) {
                                    q.push(neighbour);
                                    visited.insert(neighbour);
                                }
                            }
                        }
                        steps++;
                    }
                }

            }

        }
    }

    void estimateTheirDestinations(PacDestinationT& theirPacDestinations) {
        for (auto& [id, pac] : theirVisiblePacs) {
            
            bool noPelletNeighbours = all_of(pac.pos->neighbours.begin(), pac.pos->neighbours.end(),
                [](Tile* t) {
                    return t->pelletValue == 0;
                });

            if (noPelletNeighbours) {
                for (Tile* tile : pac.pos->neighbours) {
                    theirPacDestinations.emplace(tile, &pac);
                }
            }
            else {
                for (Tile* tile : pac.pos->neighbours) {
                    if(tile->pelletValue == 0) continue;
                    theirPacDestinations.emplace(tile, &pac);
                }
            }

        }
    }
    void printPacDestinations(PacDestinationT& pacDestinations) {
        multimap<Pacman*, Tile*> tempmap;
        for (auto [tile, pac] : pacDestinations) {
            tempmap.emplace(pac, tile);
        }
        Pacman* prev = nullptr;
        for (auto [pac, tile] : tempmap) {
            if (pac != prev) {
                cerr << "| " << pac->pacId << ": ";
                prev = pac;
            }
            cerr << "(" << tile->x << "," << tile->y << ") ";
        }
        cerr << endl;
    }


    void printPath(const Path& path) {
        cerr << "Path:";
        for (auto t : path) {
            cerr << t->toStr() << " ";
        }
    }


    void floodFillFromPacmansAndCache() {
        for (auto& [_, pac] : myPacs) {
            queue<Tile*> q;
            unordered_map<Tile*, int> discoveredDistances;

            q.push(pac.pos);
            discoveredDistances.emplace(pac.pos, 0);

            while(!q.empty()) {
                Tile* currTile = q.front(); q.pop();

                for (Tile* n : currTile->neighbours) {
                    if (discoveredDistances.count(n) == 0) {
                        discoveredDistances[n] = discoveredDistances.at(currTile) + 1;
                        q.push(n);
                    }
                }
            }

            pac.distanceToTile = move(discoveredDistances);
        }
    }

    int calcDistanceBetween(Tile* source, Tile* dest) {
        queue<Tile*> q;
        unordered_map<Tile*, int> discoveredDistances;

        q.push(source);
        discoveredDistances.emplace(source, 0);

        while(!q.empty()) {
            Tile* currTile = q.front(); q.pop();

            if (currTile == dest) {
                return discoveredDistances.at(currTile);
            }

            for (Tile* n : currTile->neighbours) {
                if (discoveredDistances.count(n) == 0) {
                    discoveredDistances[n] = discoveredDistances.at(currTile) + 1;
                    q.push(n);
                }
            }
        }
    }

    Pacman* tileClaimedByPac(Tile* tile) {
        for (auto& [_, pac] : myPacs) {
            if (tile == pac.getClaimedTile()) {
                return &pac;
            }
        }
        return nullptr;
    }

    bool isAnyPacsFirstStep(Tile* tile) {
        for (auto& [_, pac] : myPacs) {
            if (tile == pac.route.firstStep()) {
                return true;
            }
        }
        return false;
    }

    void step() {
        stringstream cmd;
        PacDestinationT myPacDestinations;
        PacDestinationT theirPacDestinations;

        // Estimate Opponent Destinations:
        estimateTheirDestinations(theirPacDestinations);
        cerr << "OppDests: ";
        printPacDestinations(theirPacDestinations);

        // Calculate distance to all tiles from each Pacman:
        floodFillFromPacmansAndCache();

        // Clear all Pac Routes:
        for (auto& [id, pac] : myPacs) {
            pac.route = Route();
        }

        for (auto& [id, pac] : myPacs) {
            cerr << "Pac" << id << ". Pos: " << pac.pos->x << "," << pac.pos->y << " STL: " << pac.speedTurnsLeft << " AC: " << pac.abilityCooldown << endl;

            auto switchCommand = step_switch(pac);
            if (switchCommand) {
                addCmd(cmd, *switchCommand);
            }
            else {
                auto speedCommand = step_speedUp(pac);
                if (speedCommand) {
                    addCmd(cmd, *speedCommand);
                }
                else {
                    auto moveCommand = step_move(pac, myPacDestinations, theirPacDestinations);
                    if (moveCommand) {
                        addCmd(cmd, *moveCommand);
                    }
                }
            }
        }

        // Output the final command:
        // cerr << cmd.str() << endl; cerr.flush();
        cout << cmd.str() << endl;

        gameSteps++;
    }


    optional<string> step_switch(Pacman& myPac) {
        //cerr << " Step switch check for Pac" << myPac.pacId << endl;
        int thresholdForSwitch = 3;

        if (!theirVisiblePacs.empty() && myPac.abilityCooldown == 0) {
            using DistT = pair<int, Pacman*>; // <dist, theirpac>
            priority_queue<DistT, vector<DistT>, std::greater<DistT>> distances;

            for (auto& [theirId, theirPac] : theirVisiblePacs) {
                int dist = myPac.distanceToTile[theirPac.pos];
                distances.push({dist, &theirPac});
                // cerr << "  Dist to theirPac" << theirPac.pacId << ": "<< dist << endl;
            }

            auto& closest = distances.top();
            if (closest.first <= thresholdForSwitch) {
                // Should switch if not already stronger type.
                Pacman* theirPac = closest.second;
                string toType = typeStrongAgainst.at(theirPac->typeId);

                if (myPac.typeId != toType) {
                    cerr << " Switching Pac" << myPac.pacId << " to " << toType << endl;
                    stringstream cmd;
                    cmd << "SWITCH " << myPac.pacId << " " << toType; // SWITCH pacId pacType
                    return cmd.str();
                }
            }
        }
        return nullopt;
    }

    optional<string> step_speedUp(Pacman& myPac) {
        // Speed Up only if:
        // - Opponents are more than threshold tiles away
        // - or if within threshold, then if they are weak to my type

        //cerr << " Step Speed check for Pac" << myPac.pacId << endl;
        int thresholdForSpeed = 10;

        if(myPac.abilityCooldown == 0 && myPac.speedTurnsLeft == 0) {

            using DistT = pair<int, Pacman*>; // <dist, theirpac>
            priority_queue<DistT, vector<DistT>, std::greater<DistT>> distances;

            for (auto& [theirId, theirPac] : theirVisiblePacs) {
                int dist = myPac.distanceToTile[theirPac.pos];
                distances.push({dist, &theirPac});
                // cerr << "  Dist to theirPac" << theirPac.pacId << ": "<< dist << endl;
            }

            // If any of the close opponent pacmans are strong against me, then dont speed up.
            if(!distances.empty()) {
                auto closest = distances.top();
                int closestDist = closest.first;
                Pacman* closestPac = closest.second;

                while (closestDist <= thresholdForSpeed && !distances.empty()) {
                    closest = distances.top(); distances.pop();
                    closestDist = closest.first;
                    closestPac = closest.second;

                    if (typeStrongAgainst.at(myPac.typeId) == closestPac->typeId) {
                        // Don't speed up.
                        cerr << " Skip speedup coz oppPac " << closestPac->pacId << " is close." << endl;
                        return nullopt;
                    }
                }
            }

            // Else we are safe to speed up:
            cerr << " Speeding Pac" << myPac.pacId << endl;
            stringstream cmd;
            cmd << "SPEED " << myPac.pacId; // SPEED pacId
            return cmd.str();
        }

        return nullopt;

    }


    optional<string> step_move(Pacman& mypac, PacDestinationT& myPacDestinations, PacDestinationT& theirPacDestinations) {
        // cerr << " Step Move check for Pac" << mypac.pacId << endl; cerr.flush();

        ///------ New Logic: ----///
        int N = 20;  // Total length of path to have.

        // 0. Reset / Clear previous route and other things:
        // QUESTION: Should I clear one by one for each pac, or should they be all cleared at once outside this?

        // 1. Get path to closest pellet that is not beyond boundary and is not claimed by other mypacs.
        vector<Path> paths = pathsToClosestPellets(mypac, theirPacDestinations);
        // cerr << " Ran pathsToClosestVisiblePellets. paths.size:" << paths.size() << endl;

        // 2. Also get path to closestPotentialPellet. (TODO)

        // 3. Also get path to closest Super Pellet.

        // 4. Extend each path upto N (or until deadend) if not already >= N. (Convert Path into Route. Route contains more information.)
        // 5. Evaluate each routes's total rewards - this is where we'll give incentive to kill or flee if opponent is next to us. Can also make potential pellet values probabilistic.
        //cerr << " ";
        vector<Route> routes;
        for (auto& path : paths) {
            vector<Route> routesForThisPath = extendPathIntoRouteOfN(path, N, mypac);
            // cerr << "R:"; cerr.flush();
            // for_each(routesForThisPath.begin(), routesForThisPath.end(), [this, &mypac](Route& rt) {
            //     calculateRouteReward3(rt, mypac);
            // });

            // cerr << route.totalReward << " ";cerr.flush();
            routes.insert(routes.end(), routesForThisPath.begin(), routesForThisPath.end());
        }
        // cerr << endl;
        // cerr << " Ran extendPathIntoRouteOfN. route.size:" << route.fullPath.size() << endl;

        // { // Debugging:
        //     if(mypac.pacId == 2) {
        //         // cerr << "  Routes: " << routes.size() << " | ";
        //         // for (auto& route : routes) {
        //         //     cerr << route.totalReward << ", ";
        //         // }
        //         // cerr << endl;
        //         for (auto& route : routes) {
        //             if (route.hasSuperPellets()) cerr << "  " << route << endl;
        //         }
        //         for (auto& route : routes) {
        //             Tile* t = &board.tiles.at({9,3});
        //             if (find(route.fullPath.begin(), route.fullPath.end(), t) != route.fullPath.end()) {
        //                 cerr << "  Rt w/ (9,3):" << route << endl;
        //             }
        //         }
        //         cerr << "  ...";
        //         for (auto& route : routes) {
        //             cerr << "  " << route << endl;
        //         }
        //     }
        // }

        // 6. Pick the best of these routes, and set that as the final route for this pac.
        sort(routes.begin(), routes.end(), [](const Route& rt1, const Route& rt2) {
            return rt1.totalReward > rt2.totalReward;
        });
        // pair<double, double> rewardRange = (routes.empty())? {0.0, 0.0} : {routes.front().totalReward, routes.back().totalReward}; // This syntax doesnt work :(
        pair<double, double> rewardRange = (routes.empty())? pair<double, double>(0.0, 0.0) : pair<double, double>(routes.front().totalReward, routes.back().totalReward);

        cerr << " pathsToClosestPellets: " << paths.size() << " Total Routes: " << routes.size() << " RewardRange: [" << rewardRange.first << ", " << rewardRange.second << "]" << endl;


        if (routes.empty()) {
            // This can happen when there are more Pacs than pellets remaining towards the end.
            // For now just choose the first Pac's claimed pellet.
            Tile* pelletTile = nullptr;
            for(auto& [otherId, otherPac] : myPacs) {
                pelletTile = otherPac.getClaimedTile();
                if(pelletTile) {
                    cerr << " Moving to otherPac" << otherId << "'s destination" << " {[" << pelletTile->x << "," << pelletTile->y <<  "]} " << endl;
                    break;
                }
            }

            stringstream cmd;
            cmd << "MOVE " << mypac.pacId << " " << pelletTile->x << " " << pelletTile->y;
            cmd << " {[" << pelletTile->x << "," << pelletTile->y <<  "]} ";
            return cmd.str();

            // TODO: Make this ^ better by creating path & route and then storing the route on pac.
        }

        if (!routes.empty()) {
            // Set this as the final Route
            mypac.route = routes[0];
            cerr << " " << mypac.routeToStr() << endl;
            cerr.flush();

            // Move to first tile in route:
            Tile* pelletTile = mypac.getClaimedTile();
            Tile* dest = mypac.getMoveDestination();
            myPacDestinations[dest] = &mypac;

            // MOVE <pacId> <x> <y> <extraInfo>
            stringstream cmd;
            cmd << "MOVE " << mypac.pacId << " " << dest->x << " " << dest->y;

            if (pelletTile == dest) {
                cmd << " {[" << pelletTile->x << "," << pelletTile->y <<  "]} ";
            }
            else {
                cmd << " [" << pelletTile->x << "," << pelletTile->y <<  "] {" << dest->x << "," << dest->y <<  "}";
            }

            return cmd.str();
        }
        else {
            cerr << " Couldn't determine destination for Pac: " << mypac.pacId << endl;
            return nullopt;
        }

    }


    /// Route to closest pellet (visible or not) by BFS on the graph.
    /// If that pellet is claimed by other mypac, then choose another BUT IMP choose one that is on the boundary. (not beyond the first accessible pellet on any path).
    /// Goal Criteria: Pellet to this pac; my other pac on a tile is a blocking tile.
    vector<Path> pathsToClosestPellets(Pacman& pac, PacDestinationT& theirPacDestinations) {
        vector<Path> pathsToAllClosestPellets;

        Tile* source = pac.pos;
        auto visibleTilesToThisPac = pac.getVisibleTiles();

        queue<Tile*> q;
        unordered_map<Tile*, Tile*> discoveredBy;    // aka visited + parent combined. <child, parent>
        unordered_map<Tile*, int> pathLength;

        q.push(source);
        discoveredBy.emplace(source, nullptr);
        pathLength.emplace(source, 0);

        while (!q.empty()) {
            Tile* currTile = q.front(); q.pop();

            // MAIN PROCESSING:
            if (currTile->pacOnTile && currTile->pacOnTile != &pac && currTile->pacOnTile->mine) {
                // Don't go beyond this node if this tile has another Pac of mine on it. only if this tile is close to mypac.
                //if (pathLength[currTile] <= 3) {
                    continue;
                //}
            }

            if (pathLength[currTile] == 1 || pathLength[currTile] == 2) {

                // If this is the first step (after source), and if some opponent pac is on this tile, AND if that pac's destination is source,
                // then that is coming towards me. So, that pac's current position cannot be reached; unless I am strong against them.
                if (currTile->pacOnTile) {
                    Pacman* otherPac = currTile->pacOnTile;
                    if (!otherPac->mine) {
                        if (typeWeakAgainst.at(otherPac->typeId) == pac.typeId) { // I'm weak against them. So dont go to/beyond this node.
                            continue;
                        }
                        else if (typeStrongAgainst.at(otherPac->typeId) == pac.typeId) {
                            // It's fine; we may eat them. nothing to do in this if-case.
                            // Actually, no. They can transform in that turn and we'll be dead. It depends:
                            if (otherPac->abilityCooldown == 0) {
                                continue;   // skip.
                            }
                            else {
                                // Even if we can eat them, they will probably get the pellets, so why bother?
                                continue;
                            }
                        }
                        else if (findValue(theirPacDestinations, otherPac)->first == source) {
                            continue;   // Probable collision.
                        }
                        else {
                            // Probably no collision; so it's probably fine. nothing to do in this if-case.
                            // Actually Not fine, atleast in this case:
                            // if (currTile->neighbours.size() == 2) {
                                continue;
                            // }
                        }
                    }
                    else {  // If it's mine...
                        continue;    // TODO. Uncomment.
                    }
                }

                // If this is the first step, and if it is also the first step of any other mypacs, then drop this path.
                // NOTE: This also equally applicable whether this pac is sped up or other pacs is sped up or both.
                if (isAnyPacsFirstStep(currTile)) {
                    continue;
                }
            }


            // GOAL:
            if(currTile->getPelletValueAdjusted() > 0 && tileClaimedByPac(currTile) == nullptr) {
                Path path;
                while (currTile != source) {
                    path.push_back(currTile);
                    currTile = discoveredBy.at(currTile);
                }
                reverse(path.begin(), path.end());
                pathsToAllClosestPellets.push_back(move(path));
            }

            // Only add neighbours of tiles without pellets. This ensures that we don't go beyond the boundary of first pellets.
            if (currTile->pelletValue == 0) {

                for (Tile* neighbour : currTile->neighbours) {
                    if (discoveredBy.count(neighbour)==0) {
                        pathLength[neighbour] = pathLength.at(currTile) + 1;
                        discoveredBy.emplace(neighbour, currTile);
                        q.push(neighbour);
                    }
                }
            }

        }

        return pathsToAllClosestPellets;
    }


    /// Extend given path upto total N nodes by using BFS from the end of given path.
    /// Select best path beyond end of given path some reward system accumulated value in M nodes. Use discounted rewards.
    /// Goal Criteria: m additional steps or dead end.
    vector<Route> extendPathIntoRouteOfN(const Path& startingPath, int N, Pacman& mypac) {

        vector<Route> routes;
        if (startingPath.empty()) return routes;

        int M = N - startingPath.size();    // M tiles more to add to the path beyond firstPelletTile.

        if (M <= 0) {   // Given path is already long enough. No need to extend it more. Just convert it to route.
            Route route;
            route.pathUptoFirstPellet = startingPath;
            route.fullPath = startingPath;
            route.firstPelletTile = startingPath.back();
            route.horizon = N; // Later, total route reward will be calculated till cutoff.

            routes.push_back(move(route));
            return routes;
        }


        {
            // Exhaustive Search:
            double gamma = 0.88;    // Discount factor

            Route startingRoute;
            startingRoute.pathUptoFirstPellet = startingPath;
            startingRoute.fullPath = startingPath;
            startingRoute.fullPath.insert(startingRoute.fullPath.begin(), mypac.pos);   // Since my Path does not include it.
            startingRoute.firstPelletTile = startingPath.back();
            startingRoute.horizon = N;

            queue<Route> q;
            q.push(startingRoute);

            while (!q.empty()) {

                Route currRoute = q.front(); q.pop();
                Path& currPath = currRoute.fullPath;
                int pathSize = currPath.size();
                Tile* currTile = currPath.back();
                Tile* prevTile = currPath[currPath.size()-2];
                bool goalCondition = false;

                //// PROCESS THIS TILE and decide reward & termination condition.
                double reward = 0.0;

                /// Pellet Reward is gonna be:
                //  (2*pelletValue - 1) discounted
                // Problem with this reward calculation:
                //  - It prefers longer routes of pellets over shorter routes,.. so when N is high like 20, it will deliberately take a longer path through a super pellet
                //  - There is not enough reward for the close-ness of a super pellet on the path.
                //  See https://www.codingame.com/share-replay/465045001
                if (currTile->pelletValue == 10 && currRoute.hasSuperPellets()) {
                    reward = 0; //  To address above problem. Fixed: https://www.codingame.com/share-replay/465070869
                }
                else {
                    // If some other pacman has this SuperPellet on its route, then reduce reward for this.
                    bool otherPacGoingForIt = false;
                    for (auto& [_, pac] : myPacs) {
                        if (!pac.route.empty()) {
                            auto& vec = pac.route.superPellets;
                            if (find(vec.begin(), vec.end(), currTile) != vec.end()) {
                                otherPacGoingForIt = true;
                            }
                        }
                    }
                    if (otherPacGoingForIt) reward = 0.5*currTile->getPelletValueAdjusted();
                    else reward = currTile->getPelletValueAdjusted();
                }

                ///
                // if(i == 0 && tile->pacOnTile) {   // TODO: Think about this more.
                //     reward += 10;   // Reward for kill.
                // }

                currRoute.totalReward += reward * pow(gamma, pathSize-1);

                if (currTile->getPelletValueAdjusted() == 10) {
                    currRoute.superPellets.push_back(currTile);
                }

                /// TODO: In exhaustive search,.. If a tile has pac on it, end path there if that tile has only 2 neighbours;
                // regardless of whether it’s my other pac or enemy pac.
                // If it has 3 neighbours then somehow half all subsequent rewards if enemy pac. If it’s my pac and it has decided a route already,
                // check its next step and cut off path at its next step.
                if (currTile->pacOnTile && currTile->pacOnTile != &mypac && currTile->neighbours.size() == 2) {
                    goalCondition = goalCondition || true;
                }

                // GOAL CRITERION:
                goalCondition = goalCondition || currPath.size() == N+1 || ( currTile->neighbours.size() == 1 && currTile->neighbours[0] == prevTile);
                if(goalCondition) {
                    currRoute.fullPath.erase(currRoute.fullPath.begin());   // Remove the mypac.pos tile; since that's how I've structured other code.
                    routes.push_back(move(currRoute));
                }
                else { // currTile on path has neighbours other than the parent.
                    for (Tile* neighbour : currTile->neighbours) {
                        if (find(currPath.begin(), currPath.end(), neighbour) == currPath.end()) {  // Add it if it's not already on the current path
                            Route tempRoute = currRoute;
                            tempRoute.fullPath.push_back(neighbour);
                            q.push(move(tempRoute));
                        }
                    }
                }

            }

        }  // End Search.

        return routes;
    }

    void calculateRouteReward1(Route& route) {
        double totalReward = 0;
        double gamma = 0.88;
        int N = route.horizon;

        bool isDeadend = route.fullPath.size() < N;

        // Reward is gonna be:
        // N - turnsWasted

        // Before that, if this is a deadend, trim away the ending cells with no pellet on them.
        int last = route.fullPath.size() - 1;
        if (isDeadend) {
            while (route.fullPath[last]->pelletValue == 0) {
                last--;
            }
        }

        int i = 0;
        int turnsWasted = 0;
        while (i <= last && i < N) {
            Tile* tile = route.fullPath[i];
            if (tile->getPelletValueAdjusted() == 0) {
                turnsWasted++;
            }
            i++;
        }

        if (i < N) {    // Means we ran into a deadend.
            // We gotta walk back to starting position now.
            // So:
            turnsWasted += i;
        }

        totalReward = N - turnsWasted;
        route.totalReward = totalReward;
    }


    // This is for a low N, of about 4 - 8 because this kinda penalizes deadends.
    // For high N like 20, nearly everything would be considered as a dead end; so not good.
    void calculateRouteReward2(Route& route, Pacman& pac) {
        double totalReward = 0;
        double gamma = 0.88;
        int N = route.horizon;

        bool isDeadend = route.fullPath.size() < N;

        // Reward is gonna be:
        // For each step:
        //  (2*pelletValue - 1) discounted

        // Before that, if this is a deadend, trim away the ending cells with no pellet on them.
        int last = route.fullPath.size() - 1;
        if (isDeadend) {
            while (last > 0 && route.fullPath[last]->pelletValue == 0) {
                last--;
            }
        }

        int i = 0;
        int stepCount = 0;

        while (i <= last && i < N) {
            Tile* tile = route.fullPath[i];
            double reward = (2 * tile->getPelletValueAdjusted() - 1);

            // if(i == 0 && tile->pacOnTile) {   // A route will never have an otherPac towards whom I am not strong. That gets filtered out in BFS.
            //     reward += 10;   // Reward for kill.
            //      Commented out coz this is a bad idea. We end up losing pellets.
            // }

            if (isDeadend) {
                totalReward += reward;      // TUNE: Dont discount rewards if going into deadend ??
            }
            else {
                totalReward += reward * pow(gamma, stepCount);
            }
            i++; stepCount++;
        }

        if (i < N) {    // Means we ran into a deadend.
            // We gotta walk back to starting position now.
            // So:
            while (i > 0) {
                totalReward  += (-1) * pow(gamma, stepCount);
                i--; stepCount++;
            }
        }

        while (stepCount < N) {  // Means we came out of a deadend, and dont know what will happen for the remaining steps.
            totalReward += 0;    // (2 * ExpectedPelletValue - 1) * gamme^stepCount;
            stepCount++;
        }

        // // Add heuristic reward for getting closer to the closest super pellet if it's not in the path already.
        // Tile* closestSuperPellet = pac.getClosestSuperPellet();
        // if (find(route.fullPath.begin(), route.fullPath.end(), closestSuperPellet) == route.fullPath.end()) {
        //     int distToSuperPellet = pac.distanceToTile[closestSuperPellet];
        //     double reward = (2 * closestSuperPellet->getPelletValueAdjusted() - 1) * pow(gamma, distToSuperPellet);
        //     totalReward += reward;
        // }

        route.totalReward = totalReward;
    }

    void calculateRouteReward3(Route& route, Pacman& pac) {
        double totalReward = 0;
        double gamma = 0.88;
        int N = route.horizon;

        // Reward is gonna be:
        // For each step:
        //  (2*pelletValue - 1) discounted

        // Before that, if this is a deadend, trim away the ending cells with no pellet on them.
        int last = route.fullPath.size() - 1;
        int i = 0;
        int stepCount = 0;

        while (i <= last && i < N) {
            Tile* tile = route.fullPath[i];
            double reward = (2 * tile->getPelletValueAdjusted() - 1);

            // if(i == 0 && tile->pacOnTile) {   // A route will never have an otherPac towards whom I am not strong. That gets filtered out in BFS.
            //     reward += 10;   // Reward for kill.
            //      Commented out coz this is a bad idea. We end up losing pellets.
            // }

            totalReward += reward * pow(gamma, stepCount);
            i++; stepCount++;
        }

        while (stepCount < N) {  // Means we came out of a deadend, and dont know what will happen for the remaining steps.
            totalReward += 0;    // (2 * ExpectedPelletValue - 1) * gamma^stepCount;
            stepCount++;
        }

        // // Add heuristic reward for getting closer to the closest super pellet if it's not in the path already.
        // Tile* closestSuperPellet = pac.getClosestSuperPellet();
        // if (find(route.fullPath.begin(), route.fullPath.end(), closestSuperPellet) == route.fullPath.end()) {
        //     int distToSuperPellet = pac.distanceToTile[closestSuperPellet];
        //     double reward = (2 * closestSuperPellet->getPelletValueAdjusted() - 1) * pow(gamma, distToSuperPellet);
        //     totalReward += reward;
        // }

        route.totalReward = totalReward;
    }

    void addCmd(stringstream& cmd, const string& command) {
        // Write an action using cout. DON'T FORGET THE "<< endl"
        if (!cmd.str().empty()) {
            cmd << "|";
        }
        cmd << command;
    }

    void input() {

        cin >> this->myScore >> this->opponentScore; cin.ignore();
        cin >> this->visiblePacCount; cin.ignore();

        this->resetPacs();

        for (int i = 0; i < this->visiblePacCount; i++) {
            int pacId; // pac number (unique within a team)
            bool mine; // true if this pac is yours
            int x; // position in the grid
            int y; // position in the grid
            string typeId; // unused in wood leagues
            int speedTurnsLeft; // unused in wood leagues
            int abilityCooldown; // unused in wood leagues

            cin >> pacId >> mine >> x >> y >> typeId >> speedTurnsLeft >> abilityCooldown; cin.ignore();
            Tile* pos = &this->board.tiles.at({x,y});

            auto& pacsContainer = (mine) ? ( (typeId != "DEAD")? this->myPacs : this->myDeadPacs ) : ( (typeId != "DEAD")? this->theirPacs : this->theirDeadPacs );

            if (pacsContainer.count(pacId) == 0) {
                pacsContainer.emplace(pacId, Pacman(pacId, mine, pos, typeId, speedTurnsLeft, abilityCooldown, this->board));
            }

            Pacman& pac = pacsContainer.at(pacId);
            pac.pos = pos;
            pac.typeId = typeId;
            pac.speedTurnsLeft = speedTurnsLeft;
            pac.abilityCooldown = abilityCooldown;
            pac.visible = true;

            if (typeId != "DEAD") { // Link only Alive pacs to tiles.
                pos->pacOnTile = &pac;
            }
            else {
                // Delete dead pacs from alive container.
                auto& alivePacsContainer = (mine) ? this->myPacs : this->theirPacs;
                alivePacsContainer.erase(pacId);
            }
        }

        this->clearPellets();

        // Input Pellets:
        cin >> this->visiblePelletCount; cin.ignore();
        cerr << "Input SuperPellets: ";
        for (int i = 0; i < this->visiblePelletCount; i++) {
            int x, y, value;
            cin >> x >> y >> value; cin.ignore();
            // cerr << "Input Pellet: " << x << " " << y << " " << value << endl;
            this->board.tiles.at({x,y}).pelletValue = value;
            if (value == 10) {
                board.superPelletTiles.insert(&this->board.tiles.at({x,y}));
                cerr << "<" << x << "," << y << "> ";
            }
        }
        cerr << endl;
    }

    void update() {
        
        this->updateVisiblePacsList();

        // If no visible pellets were received in input for visible tiles, then mark them to be 0 (gone).
        this->updateGonePellets();

        this->updateGonePelletsBasedOnEnemyPacLocation();

        // Update things related to unknown pellets:
        this->board.updateNumUnknownPellets();

        int numMyPacsTotal = myPacs.size() + myDeadPacs.size();
        int numOpponentAliveInvisiblePacs = numMyPacsTotal - theirDeadPacs.size() - theirVisiblePacs.size();
        this->board.updatePelletAdjustValues(numOpponentAliveInvisiblePacs);

        this->board.sortTileNeighboursByPelletValues();

    }
};


int main()
{
    cerr << "Starting" << setprecision(2) << endl;
    Game game;

    // Input Board Grid:
    int width; // size of the grid
    int height; // top left corner is (x=0, y=0)
    cin >> width >> height; cin.ignore();
    vector<string> grid(height);
    for (int i = 0; i < height; i++) {
        getline(cin, grid[i]); // one line of the grid: space " " is floor, pound "#" is wall
    }
    cerr << "Building board..." << endl;
    game.buildBoard(grid);
    cerr << "Built board" << endl;


    // game loop
    while (1) {
        // INPUTS:
        game.input();

        // UPDATE THINGS BASED ON INPUT:
        game.update();

        // TAKE ACTION
        game.step();
    }
}