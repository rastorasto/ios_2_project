/*
//  Name: proj2.c
//  Author: xuhliar00
//  Created: 26.04.2024
//  Description: IOS - Projekt 2 Synchronization - The Senate Bus Problem
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>

typedef struct parameters{
    int skiers;
    int stops;
    int capacity;
    int waiting_time;
    int stops_time;
}params;

typedef struct stop_sem{
    sem_t sem;
    int count;
} stop_sem;

typedef struct shared{
    int skiers_left;
    int cap_available;
    int lines;
    sem_t write;
    sem_t boarding;
    sem_t finish;
    sem_t bus_empty;
    stop_sem bs[];
} shr;

shr *shared;
FILE *file;


// int sem_mnau(sem_t *sem){
//     int val;
//     sem_getvalue(sem, &val);
//     return val;
// }
// void print_sem(){
//     int boarding = sem_mnau(&shared->boarding);
//     int finish = sem_mnau(&shared->finish);
//     int z1_sem = sem_mnau(&shared->bs[1].sem);
//     int z1_co = shared->bs[1].count;
//     int z2_sem = sem_mnau(&shared->bs[2].sem);
//     int z2_co = shared->bs[2].count;
//     int z3_sem = sem_mnau(&shared->bs[3].sem);
//     int z3_co = shared->bs[3].count;
//     int cap = shared->cap_available;
//     printf("Cap: %d Boarding: %d Finish: %d Z1: %d/%d Z2: %d/%d Z3: %d/%d\n", cap, boarding, finish, z1_sem, z1_co, z2_sem, z2_co, z3_sem, z3_co);
// }

void print_skier_start(int id){
    sem_wait(&shared->write);
    fprintf(file,"%d: L %d: started\n", shared->lines, id);
    shared->lines++;
    sem_post(&shared->write);
}
void print_skier_arrived(int id, int stop){
    sem_wait(&shared->write);
    shared->bs[stop].count++; // Increment the number of skiers at the stop
    fprintf(file,"%d: L %d: arrived to %d\n", shared->lines, id, stop);
    shared->lines++;
    sem_post(&shared->write);
}
void print_skier_boarding(int id, int stop){
    sem_wait(&shared->write);
    fprintf(file,"%d: L %d: boarding\n", shared->lines, id);
    shared->lines++;
    shared->cap_available--;    // Remove one seat from the capacity
    shared->bs[stop].count--; // Decrement the number of skiers at the stop
    sem_post(&shared->write);
}
void skier_going_to_ski(int id, int capacity){
    sem_wait(&shared->write);
    fprintf(file,"%d: L %d: going to ski\n", shared->lines, id);
    shared->lines++;
    shared->skiers_left--; // Decrement the number of skiers left
    shared->cap_available++; // Free the seat

    if(shared->cap_available == capacity){ // If the bus is empty
        sem_post(&shared->bus_empty); // Signal the skiers to go skiing
    }else {
        sem_post(&shared->finish); // Signal anoter skier to go skiing
    } 
    sem_post(&shared->write);
}
void print_skibus_start(){
    sem_wait(&shared->write);
    fprintf(file,"%d: BUS: started\n", shared->lines);
    shared->lines++;
    sem_post(&shared->write);
}
void print_skibus_arrival(int stop){
    sem_wait(&shared->write);
    fprintf(file,"%d: BUS: arrived to %d\n", shared->lines, stop);
    shared->lines++;
    sem_post(&shared->write);
}
void print_skibus_leaving(int stop){
    sem_wait(&shared->write);
    fprintf(file,"%d: BUS: leaving %d\n", shared->lines, stop);
    shared->lines++;
    sem_post(&shared->write);
}
void print_skibus_finish(){
    sem_wait(&shared->write);
    fprintf(file,"%d: BUS: finish\n", shared->lines);
    shared->lines++;
    sem_post(&shared->write);
}
void print_skibus_arrived_to_final(){
    sem_wait(&shared->write);
    fprintf(file,"%d: BUS: arrived to final\n", shared->lines);
    shared->lines++;
    sem_post(&shared->write);
}
void print_skibus_leaving_final(){
    sem_wait(&shared->write);
    fprintf(file,"%d: BUS: leaving final\n", shared->lines);
    shared->lines++;
    sem_post(&shared->write);
}
void bus_full_signal(){
    sem_wait(&shared->write);
    if(shared->cap_available == 0){
        sem_post(&shared->boarding); // If the bus is full, signal the bus to leave
    }
    sem_post(&shared->write);
}
void last_skier_at_stop(int stop){
    sem_wait(&shared->write);
    if(shared->bs[stop].count == 0){ // If the last skier at the stop
        sem_post(&shared->boarding); // Signal the bus to leave
    }
    sem_post(&shared->write);
}

void skier(pid_t id, params par){
    print_skier_start(id);

    srand(time(NULL)*id);
    usleep((rand() % par.waiting_time));
    int stop = rand() % par.stops+1;

    print_skier_arrived(id, stop);

    sem_wait(&shared->bs[stop].sem); // Wait for the bus to arrive

    bus_full_signal();

    print_skier_boarding(id, stop);
    
    sem_post(&shared->bs[stop].sem); // Allows another skier to board

    last_skier_at_stop(stop);

    sem_wait(&shared->finish); // If at finish go to ski :)
    
    skier_going_to_ski(id, par.capacity);

}

void skibus(params par){
    print_skibus_start();
    while(shared->skiers_left > 0){
        usleep(par.stops_time);
        for(int zastavka=1; zastavka<par.stops+1; zastavka++){
            print_skibus_arrival(zastavka);
            if(shared->bs[zastavka].count > 0){
                sem_post(&shared->bs[zastavka].sem);
                sem_wait(&shared->boarding);
            }
            print_skibus_leaving(zastavka);
            usleep(par.stops_time);
        }
        print_skibus_arrived_to_final();

        if(shared->cap_available < par.capacity){ // If the bus is not empty
            sem_post(&shared->finish);
            sem_wait(&shared->bus_empty); // Wait for all skiers to leave
        }

        print_skibus_leaving_final();
    }
    print_skibus_finish();
}


struct parameters arg_parsing(int argc, char **argv){
    if(argc != 6){
        fprintf(stderr, "Error: Wrong number of arguments\n");
        exit(1);
    }

    struct parameters param;

    param.skiers = atoi(argv[1]);
    param.stops = atoi(argv[2]);
    param.capacity = atoi(argv[3]);
    param.waiting_time = atoi(argv[4]);
    param.stops_time = atoi(argv[5]);

    if(param.skiers > 20000){
        fprintf(stderr, "Error: The number of the skiers cant exceed 20000\n");
        exit(1);
    } else if (param.stops <= 0 || param.stops > 10){
        fprintf(stderr, "Error: The number of the stops must be between 1 and 10\n");
        exit(1);
    } else if (param.capacity < 10 || param.capacity > 100){
        fprintf(stderr, "Error: The capacity of the skibus must be between 10 and 100\n");
        exit(1);
    } else if (param.waiting_time < 0 || param.waiting_time > 10000){
        fprintf(stderr, "Error: The max waiting time must be between 1 and 10000\n");
        exit(1);
    } else if (param.stops_time < 0 || param.stops_time > 1000){
        fprintf(stderr, "Error: The max two stops time must be between 1 and 1000\n");
        exit(1);
    }

    return param;
}

void map_and_init(params param) {
    // Allocate memory for the entire shr structure
    size_t shared_size = sizeof(shr) + sizeof(stop_sem) * (param.stops + 1);
    shared = mmap(NULL, shared_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // Initialize bs array
    for (int i = 1; i < param.stops+1; i++) {
        sem_init(&shared->bs[i].sem, 1, 0); // Initialize the semaphore
        shared->bs[i].count = 0;
    }

    // Initialize other components
    shared->skiers_left = param.skiers;
    shared->cap_available = param.capacity;
    shared->lines = 1;

    // Initialize semaphores
    sem_init(&shared->write, 1, 1);
    sem_init(&shared->boarding, 1, 0);
    sem_init(&shared->bus_empty, 1, 0);
    sem_init(&shared->finish, 1, 0);
}

void fork_gen(params param){
    // Forking skibus
    pid_t skibus_pid = fork();
    if(skibus_pid == 0){
        skibus(param);
        exit(0);
    } else if(skibus_pid < 0){
        fprintf(stderr, "Error: Fork failed\n");
        exit(1);
    }
    // Forking skiers
    for(int i = 1; i < param.skiers+1; i++){
        pid_t skier_pid = fork();
        if(skier_pid == 0){
            skier(i, param);
           // printf("lyzar skonci\n");
           // fprintf(stderr, "ID1: %d\n", getpid());
            exit(0);
        } else if(skier_pid < 0){
            fprintf(stderr, "Error: Fork failed\n");
            exit(1);
        }
    }
}
void cleanup(params param){
    // Cleanup: Destroy semaphores
    sem_destroy(&shared->finish);
    sem_destroy(&shared->boarding);
    for(int i=1; i<param.stops+1; i++){
        sem_destroy(&shared->bs[i].sem);
    }

    // Unmap shared memory
    size_t shared_size = sizeof(shr) + sizeof(stop_sem) * (param.stops + 1);
    munmap(shared, shared_size);
    
}

int main(int argc, char **argv){

    file = fopen("proj2.out", "w");
    if(file == NULL){
        fprintf(stderr, "Error: File failed to open\n");
        exit(1);
    }
    setbuf(file, NULL);
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    // Parsing arguments
    params param = arg_parsing(argc, argv);

    map_and_init(param);

    fork_gen(param);

    while(wait(NULL) > 0);
    cleanup(param);

    return 0;
}