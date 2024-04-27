/*
//  Name: proj2.c
//  Author: xuhliar00
//  Created: 26.04.2024
//  Description: IOS - Projekt 2 Synchronization - The Senate Bus Problem
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>

typedef struct parameters
{
    int skiers;
    int stops;
    int capacity;
    int waiting_time;
    int stops_time;
}params;

typedef struct shared
{
    int curr_stop;
    int skiers_left;
    int bus_capacity;
}shr;

sem_t *mutex;
sem_t *bus_arrived;
sem_t *boarded;

void skier(pid_t id, params par, shr *shared){
    printf("L %d: started\n", id);
    srand(time(NULL)*id);
    usleep((rand() % par.waiting_time));
    int stop = rand() % par.stops+1;
    //printf("%d\n", stop);
    printf("L %d: arrived to %d", id, stop);
    while(shared->curr_stop != stop){
        usleep(1);
    }
    sem_wait(mutex);
    shared->skiers_left--;
    shared->bus_capacity++;
    sem_post(mutex);
    sem_post(boarded);
    printf("L %d: boarding\n", id);
    sem_wait(mutex);
    shared->bus_capacity--;
    printf("L %d: going to ski\n", id);
    sem_post(mutex);
}

void skibus(params par, shr *shared){
    printf("BUS: started\n");
    while(shared->skiers_left > 0){
        usleep(par.stops_time);
        for(int zastavka=1; zastavka<par.stops+1; zastavka++){
            printf("A: BUS: arrival to %d\n", zastavka);
            shared->curr_stop = zastavka;
            sem_post(bus_arrived);

            while(shared->bus_capacity < par.capacity){
                   //  printf("cyklus"); //loop
                sem_wait(boarded);
            }

            printf("A: BUS: leaving %d\n",zastavka);

            usleep(par.stops_time);
        }
        // tu nieco ze dosiel do ciela tak jebne semafor aby mohli dat lyzovat

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

int main(int argc, char **argv){
    //setbuf(stdout, NULL);
    // Parsing arguments
    params param = arg_parsing(argc, argv);

    // Shared memory allocation
    int shared_memory_id = shmget(IPC_PRIVATE, sizeof(shr), IPC_CREAT | 0666);
    if(shared_memory_id == -1){
        fprintf(stderr, "Error: Shared memory allocation failed\n");
        return 1;
    }

    // Attaching shared memory
    shr *shared_memory = (shr *)shmat(shared_memory_id, NULL, 0);
    if(shared_memory == (void *)-1){
        fprintf(stderr, "Error: Shared memory allocation failed\n");
        return 1;
    }

    mutex = sem_open("/xuhliar00_mutex", O_CREAT | O_EXCL, 0666, 1);
    bus_arrived = sem_open("/xuhliar00_bus_arrived", O_CREAT | O_EXCL, 0666, 0);
    boarded = sem_open("/xuhliar00_boarded", O_CREAT | O_EXCL, 0666, 0);

    // Initialization of shared memory
    shared_memory->curr_stop = 0;
    shared_memory->skiers_left = param.skiers;
    
    // Forking skibus
    pid_t skibus_pid = fork();
    if(skibus_pid == 0){
        skibus(param, shared_memory);
        return 0;
    } else if(skibus_pid < 0){
        fprintf(stderr, "Error: Fork failed\n");
        return 1;
    }

    // Forking skiers
    for(int i = 1; i < param.skiers+1; i++){
        pid_t skier_pid = fork();
        if(skier_pid == 0){
            skier(i, param, shared_memory);
            return 0; // not sure about this
        } else if(skier_pid < 0){
            fprintf(stderr, "Error: Fork failed\n");
            return 1;
        }
    }
    

    while(wait(NULL) > 0);

    // Detaching shared memory
    if(shmdt(shared_memory) == -1){
        fprintf(stderr, "Error: Detaching shared memory failed\n");
        return 1;
    }

    // Deleting shared memory
    if(shmctl(shared_memory_id, IPC_RMID, NULL) == -1){
        fprintf(stderr, "Error: Deleting shared memory failed\n");
        return 1;
    }

    // Closing and unlinking semaphores
    sem_close(mutex);
    sem_unlink("/xuhliar00_mutex");
    sem_close(bus_arrived);
    sem_unlink("/xuhliar00_bus_arrived");
    sem_close(boarded);
    sem_unlink("/xuhliar00_boarded");



    return 0;
}
