/*
 * main.cpp
 *
 * Serial version
 *
 * Compile with -O3
 */

#include <limits.h> 
#include <stdbool.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <iostream> 
#include <vector>
#include <pthread.h>
#include "skiplist.h"
using namespace std;

#define BILLION 1000000000L
#define NT (4)

struct commandLine{
    char action;
    long num;
};

struct thread_params {
    int thread_id;
    skiplist<int, int>* list;
    std::vector<commandLine>* commandLines;
    std::vector<commandLine>* workload; //duty of this thread
};

void* worker_thread(void* params) {
    thread_params* t_params = (thread_params*)params;
    for (const auto& cmd : *(t_params->workload)) {
	if (cmd.action=='i') {
	    t_params->list->insert(cmd.num, cmd.num);
	}
	else if (cmd.action=='q') {
	    int val;
	    t_params->list->find(cmd.num, val);
	}
	else if (cmd.action=='d') {
	    t_params->list->erase(cmd.num);
	}
    }
    return NULL;
};


int main(int argc, char* argv[])
{
    struct timespec start, stop;
    bool printFlag = false;  // -p option: whether to print or not

    int opt;
    extern char* optarg;
    while ((opt = getopt(argc, argv, "p")) != -1) {
        switch (opt) {
            case 'p':
                printFlag = true;
                break;
            default:
                fprintf(stderr, "Usage: %s [-p] <infile>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Usage: %s [-p] <infile>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    FILE* fin = fopen(argv[optind], "r");
    if (!fin) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
//##################################################################################
    // count the number of lines and buffer the input file in the page cache.       
    std::vector<commandLine> allCommands; //vector to store the whole input
    commandLine cur_cmd; //현재 읽고 있는 커맨드 저장할 변수
    while(fscanf(fin, " %c %ld", &cur_cmd.action, &cur_cmd.num)==2) {
	allCommands.push_back(cur_cmd);
    }
    fclose(fin);

    //distribute workload with mod result
    std::vector<std::vector<commandLine>> t_workloads(NT); //workload of each thread
    for (const auto& cmd : allCommands) {
	int cur_thread = cmd.num%NT;
	t_workloads[cur_thread].push_back(cmd);
    }

    //init
    skiplist<int, int> list(0, INT_MAX);
    pthread_t threads[NT];
    thread_params t_params[NT];

    clock_gettime(CLOCK_REALTIME, &start);//line 다 읽고 스타트

    //create thread, allocate
    for (int i=0 ; i<NT ; ++i) {
	t_params[i].thread_id=i;
	t_params[i].list=&list;
	t_params[i].workload=&t_workloads[i];
	pthread_create(&threads[i], NULL, worker_thread, &t_params[i]); //params 설정 하고 이대로 쓰레드 생성
    }
    
    //join all threads
    for (int i=0;i<NT; ++i) {
	pthread_join(threads[i],NULL);
    }
    
/*
    int lineNo = 0;
    while (fscanf(fin, "%c %ld\n", &action, &num) > 0) {
        lineNo++;

        if (action == 'i') {
            list.insert(num, num);
        } else if (action == 'q') {
            int val;
            if (!list.find(num, val))
                cout << "ERROR: Not Found: " << num << endl;
        } else if (action == 'd') {
            list.erase(num);
        } else {
            printf("ERROR: Unrecognized action: '%c'\n", action);
            exit(EXIT_FAILURE);
        }

        count++;

        // ANSI progress rate     
        if (printFlag && (lineNo % (totalLines / 100) == 0 || lineNo == totalLines)) {
            int percent = (lineNo * 100) / totalLines;
            printf("\r\033[KLine %d / %d, Progress: %d%%", lineNo, totalLines, percent);
            fflush(stdout);
        }
    }
    fclose(fin);
    printf("\n");  
*/

    clock_gettime(CLOCK_REALTIME, &stop);

    cout << "Final skiplist keys: " << list.printList() << endl;

    double elapsed_time = (stop.tv_sec - start.tv_sec) +
                          ((double)(stop.tv_nsec - start.tv_nsec)) / BILLION;

    cout << "Elapsed time: " << elapsed_time << " sec" << endl;
    cout << "Throughput: " << (double) allCommands.size() / elapsed_time << " ops/sec" << endl;

    return EXIT_SUCCESS;
}

