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

typedef struct parameters
{
    int skiers;
    int stops;
    int capacity;
    int waiting_time;
    int stops_time;
}params;

struct stop_sem{
    sem_t *sem;
    int stop;
};

typdef struct shared{
    sem_t *boarding;
    sem_t *cap_available;
    sem_t *finish;
    struct stop_sem *stops_sem;
} shared;


void skier(pid_t id, params par){
    printf("L %d: started\n", id);
    srand(time(NULL)*id);
    usleep((rand() % par.waiting_time));
    int stop = rand() % par.stops+1;

    stops_sem[stop].stop++;
    printf("L %d: arrived to %d\n", id, stop);

    sem_wait(boarding);

    printf("L %d: boarding\n", id);
    skiers_left--;
    stops_sem[stop].stop--;
    if(stops_sem[stop].stop == 0){
        sem_post(boarding);
    }

    sem_wait(finish);
    printf("L %d: going to ski\n", id);
    sem_post(cap_available);
}

void skibus(params par){
    printf("BUS: started\n");
    while(*skiers_left){
        usleep(par.stops_time);
        for(int zastavka=1; zastavka<par.stops+1; zastavka++){
            *curr_stop = zastavka;
            printf("A: BUS: arrival to %d\n", zastavka);
            if(stops_sem[zastavka].stop > 0){
                sem_post(stops_sem[zastavka].sem);
            }
            printf("A: BUS: leaving %d\n",zastavka);
            usleep(par.stops_time);
        }
        sem_post(finish);
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

void map_and_init(params param){
    // curr_stop = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    // skiers_left = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    // cap_available = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    // finish = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    // boarding =mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    shared = mmap(NULL, sizeof(shared), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    
    //stops_sem = mmap(NULL, (param.stops+1) * sizeof(stopsm), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    for(int i=1; i<param.stops+1; i++){
        sem_init(shared.stops_sem[i].sem, 1, 0);
        stops_sem[i].stop = 0;

    }

    sem_init(cap_available, 1, param.capacity);
    sem_init(finish, 1, 1);

    // Initialization of shared memory
    *curr_stop = 0;
    *skiers_left = param.skiers;
}
int fork_gen(params param){
    // Forking skibus
    pid_t skibus_pid = fork();
    if(skibus_pid == 0){
        skibus(param);
        return 0;
    } else if(skibus_pid < 0){
        fprintf(stderr, "Error: Fork failed\n");
        return 1;
    }
    // Forking skiers
    for(int i = 1; i < param.skiers+1; i++){
        pid_t skier_pid = fork();
        if(skier_pid == 0){
            skier(i, param);
            return 0; // not sure about this
        } else if(skier_pid < 0){
            fprintf(stderr, "Error: Fork failed\n");
            return 1;
        }
    }
    return 0;
}
void cleanup(params param){
    // Cleanup: Destroy semaphores
    sem_destroy(cap_available);
    sem_destroy(finish);
    sem_destroy(boarding);
    for(int i=1; i<param.stops+1; i++){
        sem_destroy(stops_sem[i].sem);
    }

    // Unmap shared memory
    munmap(stops_sem, (param.stops+1) * sizeof(sem_t));
    
    munmap(curr_stop, sizeof(int));
    munmap(skiers_left, sizeof(int));
    munmap(cap_available, sizeof(sem_t));
    munmap(finish, sizeof(sem_t));
    munmap(boarding, sizeof(sem_t));
}

int main(int argc, char **argv){
    //setbuf(stdout, NULL);

    // Parsing arguments
    params param = arg_parsing(argc, argv);

    map_and_init(param);

    if(fork_gen(param)){
        return 1;
    }
    
    while(wait(NULL) > 0);

    cleanup(param);

    return 0;
}