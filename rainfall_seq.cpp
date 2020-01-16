#include <fstream>
#include <iostream>
#include <vector>
#include <utility>
#include <string>
#include <limits.h>
#include <algorithm>
#include <time.h>
#include <sys/time.h>
#include <iomanip>

using namespace std;

double calc_time(struct timespec start, struct timespec end) {
    double start_sec = (double)start.tv_sec*1000000000.0 + (double)start.tv_nsec;
    double end_sec = (double)end.tv_sec*1000000000.0 + (double)end.tv_nsec;
    if (end_sec < start_sec) {
      return 0;
    } else {
      return end_sec - start_sec;
    }
  }

struct rain{
  float water;
  float absorb_water;
  float trickle_water;
  vector<pair<int, int> > low_direction;
  rain():water(0), absorb_water(0), trickle_water(0){}
};

void find_low_direction(vector<vector<rain> > & raindrop, string file, int dimension) {
  //read landscape
  vector<vector<int> > landscape(dimension, vector<int>(dimension, 0));
  ifstream fin(file);
  for (int i=0; i<dimension; i++) {
    for (int j=0; j<dimension; j++) {
      fin >> landscape[i][j];
    }
  }

  //find minimum height
  vector<vector<int> > dir = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
  for(int i = 0; i < dimension; i++) {
    for(int j = 0; j < dimension; j++) {
      int min_height = landscape[i][j];
      for (int d = 0; d < 4; d++) {
        int x = i + dir[d][0];
        int y = j + dir[d][1];
        if(x >= 0 && x < dimension && y >= 0 && y < dimension) {
          min_height = min(min_height, landscape[x][y]);
        }
      }

      //add lowest neighbors in low_direnction of each point
      if(min_height < landscape[i][j]) {
        for (int d = 0; d < 4; d++) {
          int x = i + dir[d][0];
          int y = j + dir[d][1];
          if(x >= 0 && x < dimension && y >= 0 && y < dimension && landscape[x][y] == min_height) {
            raindrop[i][j].low_direction.push_back({x, y});
          }
        }
      }

    }
  }
}

int simulation(vector<vector<rain> > & raindrop, int time_steps, float absorb_water, int dimension) {
  int step =1;
  //  bool finished = true;
  while(true) {
    //first traverse
    for(int i = 0; i < dimension; i++) {
      for(int j = 0; j < dimension; j++) {

        //receive a new raindrop
        if(step <= time_steps) {
          raindrop[i][j].water += 1;
        }

        //absorb water into the point
        if(raindrop[i][j].water > absorb_water) {
          raindrop[i][j].absorb_water += absorb_water;
          raindrop[i][j].water -= absorb_water;
        } else  {
          raindrop[i][j].absorb_water += raindrop[i][j].water;
          raindrop[i][j].water = 0;
        }

        //raindrops that will next trickle to the lowest neighbor(s)
        float trickle = 0;
        if(raindrop[i][j].low_direction.size() > 0) {
          if(raindrop[i][j].water > 1) {
            trickle = 1;
          } else {
            trickle = raindrop[i][j].water;
          }
          raindrop[i][j].water -= trickle;

          trickle /= raindrop[i][j].low_direction.size();
          for(auto neighbor: raindrop[i][j].low_direction) {
            raindrop[neighbor.first][neighbor.second].trickle_water += trickle;
          }
        }
      }
    }
bool finished = true; 
    //second traverse
    for(int i = 0; i < dimension; i++) {
      for(int j = 0; j < dimension; j++) {
        raindrop[i][j].water += raindrop[i][j].trickle_water;
        raindrop[i][j].trickle_water = 0;
        if (raindrop[i][j].water > 0) finished = false;
      }
    }

    if(step > time_steps && finished) break;
    step++;
  }

  return step;
}

int main(int argc, char ** argv) {
  if(argc != 6) {
    cerr << "Wrong Argument!" << endl;
    return EXIT_FAILURE;
  }

  int threads = stoi(argv[1]);
  int time_steps = stoi(argv[2]);
  float absorb_water = stof(argv[3]);
  int dimension = stoi(argv[4]);
  string elevation_file = argv[5];
  //create a new raindrop 
  vector<vector<rain> > raindrop(dimension, vector<rain>(dimension));
  find_low_direction(raindrop, elevation_file, dimension);
 
  //simulate the rain
  struct timespec start_time, end_time;
  clock_gettime(CLOCK_MONOTONIC, &start_time);
  int steps = simulation(raindrop, time_steps, absorb_water, dimension);
  clock_gettime(CLOCK_MONOTONIC, &end_time);
  double elapsed_s = calc_time(start_time, end_time) / 1000000000.0;

  //print result
  cout << "Rainfall simulation completed in "  << steps << " time steps" << endl;
  cout << "Runtime = " << elapsed_s << " seconds" << endl;
  cout << endl;
  cout << "The following grid shows the number of raindrops absorbed at each point: " << endl;
  for (auto i=0; i < dimension; i++) {
    for (auto j=0; j < dimension; j++) {
      cout << setw(8) << setprecision(6) << raindrop[i][j].absorb_water;
    }                                                                                               
    cout << endl;
  }
}
