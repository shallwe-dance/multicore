#include <stdlib.h>
#include <cmath>
#include <vector>
#include <limits>
#include <cstdio>
#include <iostream>
#include <omp.h>

#define POINTS_MIN  1.0
#define POINTS_MAX  1000.0

struct Point {
    double x, y;
};

//Euclidean distance
double getDistance(Point const& p1, Point const& p2) {
    return std::pow(p2.x-p1.x, 2) + std::pow(p2.y-p1.y, 2);
}

/* Return the distance between the closest two points in the vector points.

   Example:
      input: [{2, 3}, {12, 30}, {40, 50}, {5, 1}, {12, 10}, {3, 4}]
      output: 1.41421
*/
double closestPair(std::vector<Point> const& points) {
    const int n = points.size();
    // The polygon needs to have at least two points
    if (n < 2)   {
        return 0;
    }
    
    //init with max value of double
    double min_dist = std::numeric_limits<double>::max();
    #pragma omp parallel for reduction(min:min_dist) schedule(static)
    for (int i = 0; i < n-1; i++) { //점 개수만큼 iter
        for (int j = i+1; j < n; j++) { //(1,2), (1,3), (1, 4), (2,3), (2, 4), (3,4)
            double dist = getDistance(points[i], points[j]);
            if (dist < min_dist) {
                min_dist = dist;
            }
        }
    }
    return std::sqrt(min_dist);
}

int main(int argc, char **argv) {
    int N = 8192;
    int seed = 17;

    if (argc == 2) {
        N = std::stoi(argv[1]);
    }
    if (argc == 3) {
	    N = std::stoi(argv[1]);
	    seed = std::stoi(argv[2]);
    }
    //점 개수, 시드 지정 가능

    std::vector<Point> points(N);
    srand(seed);

    for (size_t i = 0; i < points.size(); i++) {
        points[i].x = (rand() / (double) RAND_MAX) * (POINTS_MAX - POINTS_MIN) + POINTS_MIN;
        points[i].y = (rand() / (double) RAND_MAX) * (POINTS_MAX - POINTS_MIN) + POINTS_MIN;
    }
    //random point 생성

    double totalTime = 0.0;
    double start = omp_get_wtime();

    double dist = closestPair(points);
    printf("Distance: %.5f\n", dist);

    totalTime = omp_get_wtime() - start;
    printf("Time: %.5f\n", totalTime);
}
