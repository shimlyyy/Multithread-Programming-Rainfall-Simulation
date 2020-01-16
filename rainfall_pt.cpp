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
#include <pthread.h>

using namespace std;
pthread_mutex_t* locks;
pthread_barrier_t barrier;
int dimension;
int time_steps;
float absorb_water;
int threads;
int * thread_end;
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

struct ThreadArg {
    vector<vector<rain>> * const raindrop;//only one raindrop graph
    int start_row;
    int end_row;
    int steps;
    int fd;
    ThreadArg(vector<vector<rain>> * _raindrop, int start, int end, int _steps, int _fd): raindrop(_raindrop), start_row(start), end_row(end), steps(_steps), fd(_fd){}
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
            raindrop[i][j].low_direction.push_back({dir[d][0], dir[d][1]});
          }
        }
      }

    }
  }
}
bool checkAllTread() {
    for (int i = 0; i < threads; ++i) {
        if (thread_end[i] == 0) return true;
    }
    return false; // all threads finished
}
void * simulation(void * varg) {
    ThreadArg * arg = (ThreadArg *) varg;
    while (checkAllTread()) {
         pthread_barrier_wait(&barrier);
        //first traverse
        for(int i = arg->start_row; i < arg->end_row; ++i) {
            for (int j = 0; j < dimension; ++j) {
                //receive a new raindrop
                if(arg->steps < time_steps) {
                  (*(arg->raindrop))[i][j].water += 1;
                }
                //absorb water into the point
                if((*(arg->raindrop))[i][j].water> absorb_water) {
                  (*(arg->raindrop))[i][j].absorb_water += absorb_water;
                  (*arg->raindrop)[i][j].water -= absorb_water;
                } else  {
                  (*arg->raindrop)[i][j].absorb_water += (*arg->raindrop)[i][j].water;
                  (*arg->raindrop)[i][j].water = 0;
                }
                //raindrops that will next trickle to the lowest neighbor(s)
                float trickle = 0;
                if((*arg->raindrop)[i][j].low_direction.size() > 0) {
                  if((*arg->raindrop)[i][j].water > 1) {
                    trickle = 1;
                  } else {
                    trickle = (*arg->raindrop)[i][j].water;
                  }
                  (*arg->raindrop)[i][j].water -= trickle;

                  trickle /= (*arg->raindrop)[i][j].low_direction.size();
 
                        for(auto neighbor: (*arg->raindrop)[i][j].low_direction) {
                            //race conditon there
                            //(*arg->raindrop)[neighbor.first][neighbor.second].trickle_water += trickle;
                            //top
                            if (neighbor.first == -1 && neighbor.second == 0) {
                                if (i == arg->start_row || i == arg->start_row + 1) {
                                    pthread_mutex_lock(&locks[i - 1]);
                                }
                                    (*arg->raindrop)[i + neighbor.first][j +neighbor.second].trickle_water += trickle;
                                 if (i == arg->start_row || i == arg->start_row + 1) {
                                    pthread_mutex_unlock(&locks[i - 1]);
                                 }
                            }
                            //buttom
                            if (neighbor.first == 1 && neighbor.second == 0) {
                                if (i == arg->end_row - 1 || i == arg->end_row - 2) pthread_mutex_lock(&locks[i + 1]);
                                
                                (*arg->raindrop)[i + neighbor.first][j +neighbor.second].trickle_water += trickle;
                                
                                if (i == arg->end_row - 1 || i == arg->end_row - 2) pthread_mutex_unlock(&locks[i + 1]);
                                
                            }
                            //left
                            if (neighbor.first == 0 && neighbor.second == -1) {
                                if (i == arg->start_row || i == arg->end_row - 1) pthread_mutex_lock(&locks[i]);
                                    (*arg->raindrop)[i + neighbor.first][j +neighbor.second].trickle_water += trickle;
                                if (i == arg->start_row || i == arg->end_row - 1) pthread_mutex_unlock(&locks[i]);
                                
                            }
                            //right
                            if (neighbor.first == 0 && neighbor.second == 1) {
                                if (i == arg->start_row || i == arg->end_row - 1) pthread_mutex_lock(&locks[i]);
                                    (*arg->raindrop)[i + neighbor.first][j +neighbor.second].trickle_water += trickle;
                                if (i == arg->start_row || i == arg->end_row - 1) pthread_mutex_unlock(&locks[i]);
                                
                            }
                        }
                    
                }
            }
        }
         //before second traverse, ensure first traverse is finished
         pthread_barrier_wait(&barrier);
         bool finished = true;
        //second traverse
        for (int i = arg->start_row; i < arg->end_row; ++i) {
            for (int j = 0; j < dimension; ++j) {
                (*arg->raindrop)[i][j].water += (*arg->raindrop)[i][j].trickle_water;
                (*arg->raindrop)[i][j].trickle_water = 0;
                if ((*arg->raindrop)[i][j].water > 0) finished = false;
            }
        }
        
        if(arg->steps > time_steps && finished) thread_end[arg->fd] = 1;
        arg->steps++;
        pthread_barrier_wait(&barrier);
    }
    return NULL;
    
}
int main(int argc, char ** argv) {
  if(argc != 6) {
    cerr << "Wrong Argument!" << endl;
    return EXIT_FAILURE;
  }

  threads = stoi(argv[1]);
  time_steps = stoi(argv[2]);
  absorb_water = stof(argv[3]);
  dimension = stoi(argv[4]);
  string elevation_file = argv[5];

    //create a new raindrop
    vector<vector<rain> > raindrop(dimension, vector<rain>(dimension));
    find_low_direction(raindrop, elevation_file, dimension);
    
    //initialize locks
    locks =  new pthread_mutex_t[dimension + 2];
    for (int i = 0; i < dimension + 2; ++i) {
       pthread_mutex_init(&locks[i], NULL);
    }
    //initalize barrier
    pthread_barrier_init(&barrier, NULL, threads);
    
    //initalize the struct for every threads
    ThreadArg ** arg = new ThreadArg* [threads];
    pthread_t * threads_fd = new pthread_t[threads];
    int perThread = dimension / threads + 1;
    int extras = dimension % threads;
    int curr = 0;
    int step = 0;
    thread_end = new int[threads];
    for (int i = 0; i < threads; ++i) {
        thread_end[i] = 0;
    }
    for (int i = 0; i < threads; ++i) {
        if (i == extras) {
            perThread--;
        }
        arg[i] = new ThreadArg(&raindrop, curr, curr + perThread, step, i);
        curr += perThread;
    }
    //simulate
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    for (int i = 0; i < threads; ++i) {
        pthread_create(&threads_fd[i], NULL, &simulation, arg[i]);
    }
    for (int i = 0; i < threads; ++i) {
        pthread_join(threads_fd[i], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double elapsed_s = calc_time(start_time, end_time) / 1000000000.0;
    
  //print result
  cout << "Rainfall simulation completed in "  << arg[0] -> steps<< " time steps" << endl;
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
