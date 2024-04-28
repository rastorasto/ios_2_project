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

// Struktura pre argumenty programu
typedef struct parameters{
    int skiers;
    int stops;
    int capacity;
    int waiting_time;
    int stops_time;
}params;

// Struktura zastavky
typedef struct stop {
    sem_t sem;  // Semafor na zastavke
    int count;  // Pocet lyziarov na zastavke
} bus_stop;
// Struktura zdielanej pamate
typedef struct shared{
    int skiers_left;    // Pocet lyziarov, ktori este nelyzuju
    int cap_available;  // Pocet volnych miest v autobuse
    int lines;          // Poradove cislo printu
    sem_t write;        // Semafor na zapis do suboru
    sem_t boarding;     // Semafor pre odjazd autobusu
    sem_t finish;       // Semafor sme v cieli
    sem_t bus_empty;    // Semafor autobus je prazdny
    bus_stop bs[];      // Pole zastavok
} shr;

shr *shared;    // Ukazatel na zdielanuy pamat
FILE *file;     // Ukazatel na subor

// Vypise, ze lyziar zacal
void print_skier_start(int id){
    sem_wait(&shared->write);
    fprintf(file,"%d: L %d: started\n", shared->lines, id);
    shared->lines++;
    sem_post(&shared->write);
}

// Lyziar pride na zastavku
void print_skier_arrived(int id, int stop){
    sem_wait(&shared->write);
    shared->bs[stop].count++; // Pripocita pocet lyziarov na zastavke
    fprintf(file,"%d: L %d: arrived to %d\n", shared->lines, id, stop);
    shared->lines++; // Pripocita poradove cislo na vypis
    sem_post(&shared->write);
}

// Nastupuje do autobusu
void print_skier_boarding(int id, int stop){
    sem_wait(&shared->write);
    fprintf(file,"%d: L %d: boarding\n", shared->lines, id);
    shared->lines++;
    shared->cap_available--;    // Odpocita miesto v autobuse
    shared->bs[stop].count--;   // Odpocita pocet lyziarov na zastavke
    sem_post(&shared->write);
}

// Ide sa lyzovat
void skier_going_to_ski(int id, int capacity){
    sem_wait(&shared->write);
    fprintf(file,"%d: L %d: going to ski\n", shared->lines, id); 
    shared->lines++;
    shared->skiers_left--;      // Odpocita celkovy pocet lyziarov
    shared->cap_available++;    // Prida kapacitu v autobuse

    // Ak je autobus plny, tak povie autobusu, nech ide prec
    if(shared->cap_available == capacity){ 
        sem_post(&shared->bus_empty);   // Posle autobus prec
    }else {
        sem_post(&shared->finish);      // Povie dalsiemu lyziarovi, ze moze vystupit
    } 
    sem_post(&shared->write);
}

// Autobus zacal
void print_skibus_start(){
    sem_wait(&shared->write);
    fprintf(file,"%d: BUS: started\n", shared->lines);
    shared->lines++;
    sem_post(&shared->write);
}

// Autobus pride na zastavku
void print_skibus_arrival(int stop){
    sem_wait(&shared->write);
    fprintf(file,"%d: BUS: arrived to %d\n", shared->lines, stop);
    shared->lines++;
    sem_post(&shared->write);
}

// Autobus odchadza zo zastavky
void print_skibus_leaving(int stop){
    sem_wait(&shared->write);
    fprintf(file,"%d: BUS: leaving %d\n", shared->lines, stop);
    shared->lines++;
    sem_post(&shared->write);
}

// Autobus odviezol vsetkych lyziarov
void print_skibus_finish(){
    sem_wait(&shared->write);
    fprintf(file,"%d: BUS: finish\n", shared->lines);
    shared->lines++;
    sem_post(&shared->write);
}

// Autobus prisiel do ciela
void print_skibus_arrived_to_final(){
    sem_wait(&shared->write);
    fprintf(file,"%d: BUS: arrived to final\n", shared->lines);
    shared->lines++;
    sem_post(&shared->write);
}

// Autobus odchadza z ciela
void print_skibus_leaving_final(){
    sem_wait(&shared->write);
    fprintf(file,"%d: BUS: leaving final\n", shared->lines);
    shared->lines++;
    sem_post(&shared->write);
}

// Ak je autobus plny, tak moze ist prec
void bus_full_signal(){
    sem_wait(&shared->write);
    if(shared->cap_available == 0){
        sem_post(&shared->boarding);
    }
    sem_post(&shared->write);
}

// Ak uz na zastavke nikto nie je, tak moze ist prec
void last_skier_at_stop(int stop){
    sem_wait(&shared->write);
    if(shared->bs[stop].count == 0){ // Ak uz nikto nie je na zastavke
        sem_post(&shared->boarding); // Povie autobusu, nech ide prec
    }
    sem_post(&shared->write);
}

void skier(pid_t id, params par){
    print_skier_start(id); // Lyziar zacal

    srand(time(NULL)*id); // Vytvori seed pre generovanie zastavky
    usleep((rand() % par.waiting_time)); // Papaaa snidani
    int stop = rand() % par.stops+1; // Vygeneruje zastavku

    print_skier_arrived(id, stop); // Pride na zastavku

    sem_wait(&shared->bs[stop].sem); // Caka, kym pride skibus

    bus_full_signal(); // Ked je autobus plny, povie mu, aby isiel prec

    print_skier_boarding(id, stop); // Nastupi do autobusu
    
    sem_post(&shared->bs[stop].sem); // Povie dalsiemu lyziarovi, ze moze nastupit

    last_skier_at_stop(stop); // Ak uz na zastavke nikto nie je, povie autobusu, aby isiel prec

    sem_wait(&shared->finish); // Caka, kym pride autobus do ciela
    
    skier_going_to_ski(id, par.capacity); // Ide sa lyzovat a kontroluje stav autobusu

}

void skibus(params par){
    print_skibus_start(); // Autobus zacal
    while(shared->skiers_left > 0){ // Kym neodviezol vsetkych lyziarov
        usleep(par.stops_time); // Caka kym pride na zastavku

        for(int zastavka=1; zastavka<par.stops+1; zastavka++){
            print_skibus_arrival(zastavka); // Autobus prisiel na zastavku

            if(shared->bs[zastavka].count > 0){ // Ak je nejaky lyziar na zastavke
                sem_post(&shared->bs[zastavka].sem); // Povie lyziarovi nech nastupi
                sem_wait(&shared->boarding);    // Caka, kym nedostane signal, ze moze ist prec
            }

            print_skibus_leaving(zastavka); // Odchadza prec zo zastavky
            usleep(par.stops_time); // Caka kym pride na dalsiu zastavku
        }
        print_skibus_arrived_to_final(); // Autobus prisiel do ciela

        if(shared->cap_available < par.capacity){ // Ak su lyziari v autobuse
            sem_post(&shared->finish); // Povie lyziarovi, ze je v cieli
            sem_wait(&shared->bus_empty); // Caka, kym vystupi posledny lyziar
        }
        print_skibus_leaving_final(); // Odchadza zo zastavky
    }
    print_skibus_finish(); // Autobus odviezol vsetkych lyziarov
}


struct parameters arg_parsing(int argc, char **argv){

    // Ak nie je spravny pocet argumentov ukonci program
    if(argc != 6){
        fprintf(stderr, "Error: Wrong number of arguments\n");
        exit(1);
    }

    // Ulozi argumenty do struktury
    params param;
    param.skiers = atoi(argv[1]);
    param.stops = atoi(argv[2]);
    param.capacity = atoi(argv[3]);
    param.waiting_time = atoi(argv[4]);
    param.stops_time = atoi(argv[5]);

    // Kontrola argumentov
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

    // Alokuje zdielanu pamat
    size_t shared_size = sizeof(shr) + sizeof(bus_stop) * (param.stops + 1);
    shared = mmap(NULL, shared_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // Inicializuje semafory a pocet lyziarov na zastavke
    // Vytvaram o 1 viac zastavok, aby som mohol zacat od 1
    // a nemusel robit -1 pri indexovani
    for (int i = 1; i < param.stops+1; i++) {
        sem_init(&shared->bs[i].sem, 1, 0);
        shared->bs[i].count = 0;
    }

    // Inicializuje ostatne semafory
    sem_init(&shared->write, 1, 1);
    sem_init(&shared->boarding, 1, 0);
    sem_init(&shared->bus_empty, 1, 0);
    sem_init(&shared->finish, 1, 0);

    // Nastavi hodnoty v zdielanej pamati
    shared->skiers_left = param.skiers;
    shared->cap_available = param.capacity;
    shared->lines = 1;

}

void fork_gen(params param){
    // Vytvori proces skibus
    pid_t skibus_pid = fork();
    if(skibus_pid == 0){
        skibus(param); // Zvola funkciu skibus s argumentami
        exit(0);
    } else if(skibus_pid < 0){
        fprintf(stderr, "Error: Fork failed\n");
        exit(1);
    }
    // Vytvori lyziarov
    for(int i = 1; i < param.skiers+1; i++){
        pid_t skier_pid = fork();
        if(skier_pid == 0){
            skier(i, param); // Zvola funkciu skier s argumentami a id
            exit(0);
        } else if(skier_pid < 0){
            fprintf(stderr, "Error: Fork failed\n");
            exit(1);
        }
    }
}
void cleanup(params param){
    // Odstrani semafory
    sem_destroy(&shared->finish);
    sem_destroy(&shared->boarding);
    sem_destroy(&shared->bus_empty);
    sem_destroy(&shared->write);

    for(int i=1; i<param.stops+1; i++){
        sem_destroy(&shared->bs[i].sem);
    }

    // Uvolni zdielanu pamat
    size_t shared_size = sizeof(shr) + sizeof(bus_stop) * (param.stops + 1);
    munmap(shared, shared_size);
    
}

int main(int argc, char **argv){
    // Nastavi subor na vypis
    file = fopen("proj2.out", "w");
    if(file == NULL){
        fprintf(stderr, "Error: File failed to open\n");
        exit(1);
    }

    // Nastavi buffer na NULL
    setbuf(file, NULL);
    setbuf(stderr, NULL);

    // Skontroluje a nastavi argumenty
    params param = arg_parsing(argc, argv);

    // Vytvori zdielanu pamat a inicializuje semafory
    map_and_init(param);

    // Vytvori procesy
    fork_gen(param);

    // Caka na ukoncenie vsetkych procesov
    while(wait(NULL) > 0);

    // Odstrani semafory a uvolni zdielanu pamat
    cleanup(param);

    return 0;
}