# Codingame Pacman Spring Challenge 2020
My code for the weeklong Codingame Spring 2020 Challenge: https://www.codingame.com/multiplayer/bot-programming/spring-challenge-2020   
Written in C++. Ranked in Gold League and #250 overall by final day.

# Game Description From Codingame:
This game is based on Pac-man, with a unique rock-paper-scissors twist.

STATEMENT:
Control a team of pacs and eat more pellets than your opponent. Beware, an enemy pac just might be waiting around the corner to take a bite out of you!

KEYWORDS:
Pathfinding, Multi-Agent, Distances, 2D Array

# My Approach
Since I didn't use git during the challenge, I ended up saving versions of my code from time to time for each league. You can see the evolution of the code in it. *`gold_curr.cpp` is the final version.*   
Note that the entire code is in a single file (ugly) because one file is all that codingame allows.  
Also included is a rough Document that I used while coding for braindumps.

The final solution has the following design

* Represent the entire grid as a Graph of Tiles
* For each time-step, there are 3 main moves for each Pac: SpeedUp, Switch, or Move.
* To decide how & where to move:
  * For each Pac (my pacman), run a BFS to find all the pellets* that are not beyond another pellet (on the boundary of empty tiles & filled tiles) and that which is not claimed by another Pac of mine.
  * For each of these, run an exhaustive search (by DFS) to extend the path upto N=20 tiles in all possible directions.
  * Assign a reward for these paths as they are built up.
  * Choose the path of best potential reward and claim the first pellet on the path.
  
* Since not all pellets are visible, I use an Expected value of a pellet on a tile, and decay it with time steps based on visibility of enemy pacs.



