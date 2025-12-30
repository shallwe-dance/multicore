all:
	g++ -O3 -o inputgen inputgen.cpp
	g++ -O3 -march=native driver.cpp -o sequential_skiplist -pthread

run:
	./sequential_skiplist workload.txt   

gen:
	./inputgen -n 1000000 -i40 -d20
