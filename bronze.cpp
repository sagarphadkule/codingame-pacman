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
    vector<Tile*> neighbours;

    bool visible = false;

    Pacman* pacOnTile = nullptr;  // If there exists any pacman on this tile.

    Tile() {}
    Tile(int x, int y) :
        x(x), y(y)
    {}

    string toStr() const {
        stringstream ss;
        ss << "(" << x << "," << y << ")";
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

    Board() {}

    bool isInBounds(int x, int y) {
        if (x >= 0 && x < width && y >=0 && y < height) return true;
        return false;
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
    double totalValue = -9999;       
    double additionalPathValue = -9999;

    Tile* firstStep() {
        return fullPath.size()==0 ? nullptr : fullPath[0];
    }

    Tile* secondStep() {
        return fullPath.size()==1 ? nullptr : fullPath[1];
    }

    bool empty() {
        return fullPath.empty();
    }
    
};


class Pacman {
public:
    int pacId; // pac number (unique within a team)
    bool mine; // true if this pac is yours
    Tile* pos;
    string typeId; // unused in wood leagues
    int speedTurnsLeft; // unused in wood leagues
    int abilityCooldown; // unused in wood leagues    
    const Board& board;

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
    int myScore;
    int opponentScore;
    int visiblePacCount; // all your pacs and enemy pacs in sight
    
    map<int, Pacman> myPacs;
    map<int, Pacman> theirPacs;
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

    void clearPacPositionsOnTiles() {
        for (auto& [id, pac] : myPacs) {
            pac.pos->pacOnTile = nullptr;
        }
        for (auto& [id, pac] : theirPacs) {
            pac.pos->pacOnTile = nullptr;
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
    void updateGonePellets() {        
        for(auto& [id, pac] : myPacs) {
            auto visibleTiles = pac.getVisibleTiles();
            for (Tile* tile : visibleTiles) {
                if (tile->pelletValue == -1) {
                    tile->pelletValue = 0;
                }
            }
        }
    }

    void estimateTheirDestinations(PacDestinationT& theirPacDestinations) {
        for (auto& [id, pac] : theirPacs) {

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

        for (auto& [id, pac] : myPacs) {
            cerr << "Pac" << id << ". Pos: " << pac.pos->x << "," << pac.pos->y << " STL: " << pac.speedTurnsLeft << " AC: " << pac.abilityCooldown << endl;
            
            auto switchCommand = step_switch(pac);
            if (switchCommand) {
                addCmd(cmd, *switchCommand);
            }
            else {
                auto speedCommand = false; // step_speedUp(pac);
                if (speedCommand) {
                    // addCmd(cmd, *speedCommand);
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
    }

    void floodFillFromPacmansAndCache() {        
        this->pacDistances.clear();

        vector<Pacman*> allPacs;
        for (auto& [_, pac] : myPacs) {            
            allPacs.push_back(&pac);
        }   

        for (auto pac : allPacs) {            
            
            queue<Tile*> q;
            unordered_map<Tile*, int> discoveredDistances;

            q.push(pac->pos);
            discoveredDistances.emplace(pac->pos, 0);

            while(!q.empty()) {
                Tile* currTile = q.front(); q.pop();
                
                for (Tile* n : currTile->neighbours) {
                    if (discoveredDistances.count(n) == 0) {
                        discoveredDistances[n] = discoveredDistances.at(currTile) + 1;
                        q.push(n);
                    }
                }
            }
            
            this->pacDistances.emplace(pac, discoveredDistances);
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


    optional<string> step_switch(Pacman& myPac) {
        // cerr << " Step switch check for Pac" << myPac.pacId << endl;
        int thresholdForSwitch = 3;

        if (!theirPacs.empty() && myPac.abilityCooldown == 0) {
            using DistT = pair<int, Pacman*>; // <dist, theirpac>           
            priority_queue<DistT, vector<DistT>, std::greater<DistT>> distances;        

            for (auto& [theirId, theirPac] : theirPacs) {
                int dist = pacDistances.at(&myPac).at(theirPac.pos);
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
        
        // cerr << " Step Speed check for Pac" << myPac.pacId << endl;
        int thresholdForSpeed = 10;

        if(myPac.abilityCooldown == 0 && myPac.speedTurnsLeft == 0 && !theirPacs.empty()) {

            using DistT = pair<int, Pacman*>; // <dist, theirpac>           
            priority_queue<DistT, vector<DistT>, std::greater<DistT>> distances;        

            for (auto& [theirId, theirPac] : theirPacs) {
                int dist = pacDistances.at(&myPac).at(theirPac.pos);
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
                    
                    if (typeWeakAgainst.at(myPac.typeId) == closestPac->typeId) {
                        // Don't speed up.
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
        //cerr << " Step Move check for Pac" << mypac.pacId << endl;

        Tile* source = &board.tiles.at({mypac.pos->x, mypac.pos->y});

        // // Route to closest pellet
        // vector<Tile*> route = routeToClosestPellet(mypac, myPacDestinations, theirPacDestinations);
        // string destType = "VisPel";

        // // Find best route by DFS of n steps
        // vector<Tile*> route = findBestRouteDFS(mypac, myPacDestinations, theirPacDestinations);
        // string destType = "VisPelDFS";

        // Find best route by DFS of n steps
        // vector<Tile*> route = findBestRoute(mypac, myPacDestinations, theirPacDestinations, 12);
        // string destType = "BestRt";

        ///------ New Logic: ----///
        int N = 4;  // Total length of path to have.

        // 1. Get path to closest pellet that is not beyond boundary and is not claimed by other mypacs.
        vector<Tile*> path = pathToClosestVisiblePellet(mypac, myPacDestinations, theirPacDestinations);
        string destType = "VisPel";
        
        // 2. Also get path to closestPotentialPellet. (TODO)
        if (path.empty()) {
            path = routeToClosestPotentialPellet(mypac, myPacDestinations, theirPacDestinations);   // old function
            destType = "PotPel";
        }

        // 3. Also get path to closest Super Pellet.
        
        // 4. Extend each path upto N (or until deadend) if not already >= N. (Convert Path into Route. Route contains more information.)
        Route route = extendPathIntoRouteOfN(path, N, mypac, myPacDestinations, theirPacDestinations);
        
        
        // 5. Evaluate each routes's total rewards - this is where we'll give incentive to kill or flee if opponent is next to us. Can also make potential pellet values probabilistic.
        
        // 6. Pick the best of these routes, and set that as the final route for this pac.

        
        if (!route.empty()) {
            // Set this as the final Route
            mypac.route = route;
            
            // Move to first tile in route:
            
            Tile* dest1 = nullptr;
            if (!mypac.isSpeedActive()) {
                dest1 = route.firstStep();
            }
            else {
                dest1 = route.secondStep();
            }

            cerr << " "<< destType << " Dest1: " << *dest1 << endl;
            printPath(route.fullPath); cerr << endl;
            myPacDestinations[dest1] = &mypac;

            stringstream cmd;
            cmd << "MOVE " << mypac.pacId << " " << dest1->x << " " << dest1->y; // MOVE <pacId> <x> <y>
            return cmd.str();
        }
        else {
            cerr << " Couldn't determine destination for Pac: " << mypac.pacId << endl;
            return nullopt;
        }
    
    }




    /// Route to closest visible pellet by BFS on the graph.
    /// If that pellet is claimed by other mypac, then choose another BUT IMP choose one that is on the boundary. (not beyond the first accessible pellet on any path).
    /// Goal Criteria: Pellet Visible to this pac; my other pac on a tile is a blocking tile.
    vector<Tile*> pathToClosestVisiblePellet(Pacman& pac, PacDestinationT& myPacDestinations, PacDestinationT& theirPacDestinations) {
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
            if (visibleTilesToThisPac.count(currTile) == 0) {
                // Don't go beyond this node if this tile is not visible.
                continue;
            } 
            if (currTile->pacOnTile && currTile->pacOnTile != &pac && currTile->pacOnTile->mine) {
                // Don't go beyond this node if this tile has another Pac of mine on it.
                continue;
            }

            // If this is the first step (after source), and if some pac's destination is source, 
            // then that pac's current position cannot be reached; unless I am strong against them.
            if (pathLength[currTile] == 1) {
                Pacman* otherPac = currTile->pacOnTile;
                if (otherPac != nullptr) {     
                    if (!otherPac->mine) {
                        if (typeWeakAgainst.at(otherPac->typeId) == pac.typeId) { // I'm weak against them. So dont go to/beyond this node.
                            continue;
                        }
                        else if (typeStrongAgainst.at(otherPac->typeId) == pac.typeId) {
                            // It's fine; we may eat them. nothing to do in this if-case.
                        }
                        else if (findValue(theirPacDestinations, otherPac)->first == source) {
                            continue;   // Probable collision.
                        }
                        else {
                            // Probably no collision; so it's probably fine. nothing to do in this if-case.
                        }
                    }
                }                    
            }      


            // GOAL:
            if(currTile->pelletValue > 0 && myPacDestinations.count(currTile) == 0) {    
                vector<Tile*> path;
                while (currTile != source) {
                    path.push_back(currTile);
                    currTile = discoveredBy.at(currTile);
                }
                reverse(path.begin(), path.end());
                return path;
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
        return {};
    }

    /// Extend given path upto total N nodes by using BFS from the end of given path.
    /// Select best path beyond end of given path some reward system accumulated value in M nodes. Use discounted rewards.
    /// Goal Criteria: m additional steps or dead end.    
    Route extendPathIntoRouteOfN(const Path& path, int N, Pacman& mypac, PacDestinationT& myPacDestinations, PacDestinationT& theirPacDestinations) {
        Route route;

        if (path.empty()) return route;

        route.pathUptoFirstPellet = path;
        route.fullPath = path;
        route.firstPelletTile = path.back();
        
        int M = N - path.size();    // M tiles more to add to the path beyond firstPelletTile.
        if (M <= 0) {
            return route;
        }

        PathsValsT paths; // vec<{double, Path}>        
        {
            // BFS:
            
            unordered_set<Tile*> alreadyVisited;
            for (Tile* t : route.pathUptoFirstPellet) {
                alreadyVisited.insert(t);
            }
            
            Tile* source = route.firstPelletTile;
            queue<Tile*> q;
            unordered_map<Tile*, Tile*> discoveredBy;    // aka visited + parent combined. <child, parent>
            unordered_map<Tile*, int> pathLength;
            unordered_map<Tile*, double> discountedRewards;
            q.push(source);
            discoveredBy.emplace(source, nullptr);
            pathLength.emplace(source, 0);        
            double discount = 0.88;

            while (!q.empty()) {

                Tile* currTile = q.front(); q.pop();
                double reward = (currTile->pelletValue == -1) ? 0.5 : currTile->pelletValue;             
                double discountedReward = 1.0 * pow(discount, pathLength[currTile]) * reward;
                discountedRewards.emplace(currTile, discountedReward);

                // if (currTile->pacOnTile && currTile->pacOnTile->mine) {
                //     Pacman* otherPac = currTile->pacOnTile;
                //     // if (otherPac->route.firstStep() == discoveredBy[currTile]) { // Means that pac is coming towards me.
                //     // 
                //     // }
                // }

                // GOAL CRITERION:
                bool goalCondition = pathLength[currTile] == M || (currTile->neighbours.size() == 1 && discoveredBy[currTile] == currTile->neighbours[0]);
                if(goalCondition) {    
                    Path path;
                    double totalReward=0;
                    while (currTile != source) {
                        path.push_back(currTile);
                        totalReward += discountedRewards[currTile];
                        currTile = discoveredBy.at(currTile);
                    }
                    reverse(path.begin(), path.end());
                    paths.push_back({totalReward, path});
                }
                else {
                    // REPEAT FOR NEIGHBOURS:
                    for (Tile* neighbour : currTile->neighbours) {         
                        if (alreadyVisited.count(neighbour)==0 && discoveredBy.count(neighbour)==0) {
                            pathLength[neighbour] = pathLength.at(currTile) + 1;
                            discoveredBy.emplace(neighbour, currTile);
                            q.push(neighbour);
                        }
                    }
                }
            }
        
        }  // End BFS

        if (paths.empty()) {
            return route;
        }

        sort(paths.begin(), paths.end(), greater<pair<double, Path> >() ); // Sort in descending order.
        auto& [additionalPathValue, bestAdditionalPath] = paths[0];        
        
        // Now append this bestAdditionalPath to the Route to form the final route.
        route.additionalPathValue = additionalPathValue;
        route.additionalPath = bestAdditionalPath;
        route.fullPath.insert(route.fullPath.end(), bestAdditionalPath.begin(), bestAdditionalPath.end());

        return route;

    }


    /// Find best route by BFS based on accumulated value in n steps. Use discounted rewards.
    /// Goal Criteria: n steps or dead end.
    Path findBestRoute(Pacman& pac, PacDestinationT& myPacDestinations, PacDestinationT& theirPacDestinations, int n) {
        
        PathsValsT paths; // vec<{double, Path}>
        
        {
            // BFS:
            double discount = 0.88;
            Tile* source = pac.pos;
            queue<Tile*> q;
            unordered_map<Tile*, Tile*> discoveredBy;    // aka visited + parent combined. <child, parent>
            unordered_map<Tile*, int> pathLength;
            unordered_map<Tile*, double> discountedRewards;
            q.push(source);
            discoveredBy.emplace(source, nullptr);
            pathLength.emplace(source, 0);        

            while (!q.empty()) {

                Tile* currTile = q.front(); q.pop();
                double reward = (currTile->pelletValue == -1) ? 0.5 : currTile->pelletValue;            

                // If this is the first step (after source), and if some pac's destination is source, 
                // then that pac's current position cannot be reached; unless I am strong against them.
                if (pathLength[currTile] == 1) {
                    Pacman* otherPac = currTile->pacOnTile;
                    if (otherPac != nullptr) {     // TODO include conditoin for my pacs.
                        if (!otherPac->mine) {
                            if (typeStrongAgainst.at(otherPac->typeId) == pac.typeId) {
                                reward += 10;   // Extra reward to incentivize killing opponent!
                            }
                            else if (typeWeakAgainst.at(otherPac->typeId) == pac.typeId) {
                                reward += -100;  // Big penalty for potential death!
                            }    
                            else if (findValue(theirPacDestinations, otherPac)->first == source) {
                                reward -= 10;   // Probable collision.
                            }
                            else {
                                // Probably no collision.
                            }
                        }                        
                        else if(otherPac->mine && findValue(theirPacDestinations, otherPac)->first == source) {
                            reward += -100;
                        }
                    }                    
                }                                

                double discountedReward = 1.0 * pow(discount, pathLength[currTile]) * reward;
                discountedRewards.emplace(currTile, discountedReward);

                // GOAL CRITERION:
                bool goalCondition = pathLength[currTile] == n || (currTile->neighbours.size() == 1 && discoveredBy[currTile] == currTile->neighbours[0]);
                if(goalCondition) {    
                    Path path;
                    double totalReward=0;
                    while (currTile != source) {
                        path.push_back(currTile);
                        totalReward += discountedRewards[currTile];
                        currTile = discoveredBy.at(currTile);
                    }
                    reverse(path.begin(), path.end());
                    paths.push_back({totalReward, path});
                }
                else {
                    // REPEAT FOR NEIGHBOURS:
                    for (Tile* neighbour : currTile->neighbours) {                    
                    
                        if (discoveredBy.count(neighbour)==0) {
                            pathLength[neighbour] = pathLength.at(currTile) + 1;
                            discoveredBy.emplace(neighbour, currTile);
                            q.push(neighbour);
                        }
                    }
                }
            }
        
        }  // End BFS

        sort(paths.begin(), paths.end(), greater<pair<double, Path> >() ); // Sort in descending order.

        {   // Debugging:
            if (true && (pac.pacId == 2 || pac.pacId == 3)) { 
                for (auto& path : paths) {
                    cerr << "  Val: " << path.first << " ";
                    printPath(path.second);
                    cerr << endl;
                }
            }            
        }

        Path bestPath;
        for (auto& [value, path] : paths) {
            if (path.empty()) {
                return {};
            }

            Tile* movePosition = (pac.speedTurnsLeft > 0 && path.size() > 1) ? path[1] : path[0];
            if (myPacDestinations.count(movePosition) == 0) {
                bestPath = path;
                break;
            }
        }
        return bestPath;     

    }



    /// helper dfs 
    void dfs(Tile* tile, Path currPath, double totalValue, PathsValsT& paths, int i, const int n, unordered_set<Tile*>& visited, Pacman& pac, PacDestinationT& myPacDestinations, PacDestinationT& theirPacDestinations) {        
        double discount = 0.88;
        double value = pow(discount, i) * ((tile->pelletValue == -1) ? 0.5 : tile->pelletValue);

        totalValue += value;
        if (tile != pac.pos) {
            currPath.push_back(tile);
        }
        

        if (i == 0) {
            if(theirPacDestinations.count(tile) >= 1) {
                // If this is the first step in the path and if the opponent is also taking that step, then skip it (blocking tile); unless I am strong against them.
                Pacman* oppPac = theirPacDestinations.at(tile);
                if (typeStrongAgainst.at(oppPac->typeId) == pac.typeId) {
                    totalValue += 10;   // Extra reward to incentivize killing opponent!
                    currPath.push_back(tile);   // Also, add current tile which is pac.pos so that we stay there to kill them.
                }
                else {
                    return; // drop this path.
                }                        
            }
            else if (myPacDestinations.count(tile) >= 1) {
                return; // drop this path.
            }
        }

        if (i == n || tile->neighbours.size() == 1 && visited.count(tile->neighbours[0])==1) {   // Base case. This was last node or dead end.
            paths.push_back({totalValue, currPath});
        }
        else {
            for (Tile* neighbour : tile->neighbours) {
                if (visited.count(neighbour) == 0) {
                    visited.insert(neighbour);
                    dfs(neighbour, currPath, totalValue, paths, i+1, n, visited, pac, myPacDestinations, theirPacDestinations);
                }                
            }
        }
    }

    /// Find best route by DFS based on accumulated value in n steps.
    /// Goal Criteria: n steps.
    vector<Tile*> findBestRouteDFS(Pacman& pac, PacDestinationT& myPacDestinations, PacDestinationT& theirPacDestinations, int n =6) {
        Tile* source = pac.pos;
                
        PathsValsT paths;
        Path currPath;

        unordered_set<Tile*> visited;
        visited.insert(pac.pos);
        dfs(pac.pos, currPath, 0.0, paths, 0, n, visited, pac, myPacDestinations, theirPacDestinations);           

        sort(paths.begin(), paths.end(), greater<pair<double, Path> >() ); // Sort in descending order.

        if (pac.pacId == 2) {
            for (auto& [value, path] : paths) {
                cerr << "  Val: " << value << " Path:";
                printPath(path);
                cerr << endl;
            }
        }

        Path bestPath;
        for (auto& [value, path] : paths) {
            if (path.empty()) {
                return {};
            }

            Tile* movePosition = (pac.speedTurnsLeft > 0 && path.size() > 1) ? path[1] : path[0];
            if (myPacDestinations.count(movePosition) == 0) {
                bestPath = path;
                break;
            }
        }

        return bestPath;                
    }


    ///// -- Original: -- ////
    /// Route to closest pellet by BFS on the graph.
    /// Goal Criteria is: Pellet Visible to this pac; other pac's immediate dest is a blocking tile.
    vector<Tile*> routeToClosestPellet(Pacman& pac, PacDestinationT& myPacDestinations, PacDestinationT& theirPacDestinations) {        // Q: Would rather return a Tile& but std::optional<> cannot hold references. What's the recommendation here?    
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
            if(currTile->pelletValue > 0 && myPacDestinations.count(currTile) == 0) {    
                vector<Tile*> path;
                while (currTile != source) {
                    path.push_back(currTile);
                    currTile = discoveredBy.at(currTile);
                }
                reverse(path.begin(), path.end());
                return path;
            }

            for (Tile* neighbour : currTile->neighbours) {
                if (discoveredBy.count(neighbour)==0 && visibleTilesToThisPac.count(neighbour) != 0 && myPacDestinations.count(neighbour) == 0) {
                    pathLength[neighbour] = pathLength.at(currTile) + 1;
                    discoveredBy.emplace(neighbour, currTile);                    
                    
                    if (pathLength[neighbour] == 1) {
                        if(theirPacDestinations.count(neighbour) >= 1) {
                            // If this is the first step in the path and if the opponent is also taking that step, then skip it. (blocking tile).
                            Pacman* oppPac = theirPacDestinations.at(neighbour);
                            if (typeWeakAgainst.at(pac.typeId) != oppPac->typeId) {
                                continue;   
                            }
                            else {
                                return {currTile};  // Kill opponent!
                            }                        
                        }
                        else if (myPacDestinations.count(neighbour) >= 1) {
                            continue;
                        }
                    }
                    q.push(neighbour);                    
                }
            }
        }
        return {};
    }

    /// Find closest potential pellet by BFS on the graph.
    /// Goal Criteria is: Tile with unknown pellet value (-1) and no other my pac going for it.
    vector<Tile*> routeToClosestPotentialPellet(Pacman& pac, PacDestinationT& myPacDestinations, PacDestinationT& theirPacDestinations) {
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
            if(currTile->pelletValue == -1 && myPacDestinations.count(currTile) == 0) {    
                vector<Tile*> path;
                while (currTile != source) {
                    path.push_back(currTile);
                    currTile = discoveredBy.at(currTile);
                }
                reverse(path.begin(), path.end());
                return path;
            }

            for (Tile* neighbour : currTile->neighbours) {
                if (discoveredBy.count(neighbour)==0) {
                    pathLength[neighbour] = pathLength.at(currTile) + 1;
                    discoveredBy.emplace(neighbour, currTile);       
                    
                    if (pathLength[neighbour] == 1) {
                        if(theirPacDestinations.count(neighbour) >= 1) {
                            // If this is the first step in the path and if the opponent is also taking that step, then skip it. (blocking tile).
                            Pacman* oppPac = theirPacDestinations.at(neighbour);
                            if (typeWeakAgainst.at(pac.typeId) != oppPac->typeId) {
                                continue;   
                            }
                            else {
                                return {currTile};  // Kill opponent!
                            }                        
                        }
                        else if (myPacDestinations.count(neighbour) >= 1) {
                            continue;
                        }
                    }
                    q.push(neighbour);                    
                }
            }
        }
        return {};
    }

    void addCmd(stringstream& cmd, const string& command) {
        // Write an action using cout. DON'T FORGET THE "<< endl" 
        if (!cmd.str().empty()) {
            cmd << "|";
        }
        cmd << command;
    }
    
    // SWITCH pacId pacType
};


int main()
{    
    cerr << "Starting" << endl;
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
        cin >> game.myScore >> game.opponentScore; cin.ignore();
        cin >> game.visiblePacCount; cin.ignore();
        
        game.clearPacPositionsOnTiles();
        game.theirPacs.clear(); // So that we keep only visible pacs in the list.

        for (int i = 0; i < game.visiblePacCount; i++) {
            int pacId; // pac number (unique within a team)
            bool mine; // true if this pac is yours
            int x; // position in the grid
            int y; // position in the grid
            string typeId; // unused in wood leagues
            int speedTurnsLeft; // unused in wood leagues
            int abilityCooldown; // unused in wood leagues

            cin >> pacId >> mine >> x >> y >> typeId >> speedTurnsLeft >> abilityCooldown; cin.ignore();
            Tile* pos = &game.board.tiles.at({x,y});            
            auto& pacsContainer = (mine) ? game.myPacs : game.theirPacs;        

            if (pacsContainer.count(pacId) == 0) {
                pacsContainer.emplace(pacId, Pacman(pacId, mine, pos, typeId, speedTurnsLeft, abilityCooldown, game.board));
            }
            
            Pacman& pac = pacsContainer.at(pacId);
            pac.pos = pos;
            pac.typeId = typeId;
            pac.speedTurnsLeft = speedTurnsLeft;
            pac.abilityCooldown = abilityCooldown;
                                
            pos->pacOnTile = &pac;
        }
        
        game.clearPellets();

        // Input Pellets:       
        cin >> game.visiblePelletCount; cin.ignore();
        for (int i = 0; i < game.visiblePelletCount; i++) {         
            int x, y, value;   
            cin >> x >> y >> value; cin.ignore();
            // cerr << "Input Pellet: " << x << " " << y << " " << value << endl;
            game.board.tiles.at({x,y}).pelletValue = value;
        }
        
        // If no visible pellets were received in input for visible tiles, then mark them to be 0 (gone).
        game.updateGonePellets();

        // TAKE ACTION
        game.step();
    }
}