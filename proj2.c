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
    sem_t boarding;
    sem_t finish;
    stop_sem bs[];
} shr;

shr *shared;


int sem_mnau(sem_t *sem){
    int val;
    sem_getvalue(sem, &val);
    return val;
}
void skier(pid_t id, params par){
    printf("L %d: started\n", id);
    srand(time(NULL)*id);
    usleep((rand() % par.waiting_time));
    int stop = rand() % par.stops+2;

    (shared->bs[stop].count)++; // Increment the number of skiers at the stop
    printf("L %d: arrived to %d\n", id, stop);
    printf("zzBOARDING: %d , Z1SEM: %d , Z1CO: %d , Z2SEM: %d , Z2CO: %d \n", sem_mnau(&shared->boarding), sem_mnau(&shared->bs[1].sem), shared->bs[1].count,sem_mnau(&shared->bs[2].sem), shared->bs[2].count);
    sem_wait(&shared->bs[stop].sem); // Wait for the bus to arrive
    printf("bus prisiel\n");
    if(shared->cap_available == 0){
        sem_post(&shared->boarding); // If the bus is full, signal the bus to leave
    }

    printf("L %d: boarding\n", id);
    (shared->cap_available)--;    // Remove one seat from the capacity
    shared->bs[stop].count--; // Decrement the number of skiers at the stop
    printf("pusti dalsieho nastupit\n");
    sem_post(&shared->bs[stop].sem);         // Allows another skier to board
    if(shared->bs[stop].count == 0){ // If the last skier at the stop
        printf("nech ide bus do pici\n");
        sem_post(&shared->boarding); // Signal the bus to leave
    }

    sem_wait(&shared->finish); // If at finish go to ski :)
    printf("L %d: going to ski\n", id);
    (shared->skiers_left)--; // Decrement the number of skiers left
    (shared->cap_available)++; // Free the seat
}

void skibus(params par){
    printf("BUS: started\n");
    while(shared->skiers_left > 0){
        usleep(par.stops_time);
        for(int zastavka=1; zastavka<par.stops+1; zastavka++){
            printf("A: BUS: arrival to %d\n", zastavka);
            if(shared->bs[zastavka].count > 0){
                printf("na zastavke cakaju chlopi\n");
                sem_post(&shared->bs[zastavka].sem);
                printf("bus caka kym nastupi posledny chlop\n");
                sem_wait(&shared->boarding);
            }else {
                printf("na zastavke nikto necaka\n");
            }
            printf("A: BUS: leaving %d\n",zastavka);
            usleep(par.stops_time);
        }
        sem_post(&shared->finish);
    }
    printf("A: BUS: finish\n");
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
    for (int i = 0; i < param.stops; i++) {
        sem_init(&shared->bs[i].sem, 1, 0); // Initialize the semaphore
        shared->bs[i].count = 0;
    }

    // Initialize other components
    shared->skiers_left = param.skiers;
    shared->cap_available = param.capacity;

    // Initialize semaphores
    sem_init(&shared->boarding, 1, 0);
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
            printf("lyzar skonci\n");
            fprintf(stderr, "ID1: %d\n", getpid());
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
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    // Parsing arguments
    params param = arg_parsing(argc, argv);

    map_and_init(param);

    fork_gen(param);

    while(wait(NULL) > 0);
    fprintf(stderr, "ID2: %d\n", getpid());
    fprintf(stderr, "Error: pico failed\n");
    cleanup(param);

    return 0;
}