/*
//  Name: proj2.c
//  Author: xuhliar00
//  Created: 26.04.2024
//  Description: IOS - Projekt 2 Synchronization - The Senate Bus Problem
*/

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv){
    int number_of_skiers;
    int number_of_stops;
    int capacity_of_skibus;
    int max_waiting_time;
    int max_two_stops_time;
    // Check if the number of arguments is correct
    if(argc != 6){
        fprintf(stderr, "Error: Wrong number of arguments\n");
        return 1;
    }
    // Sets the values from the arguments
    number_of_skiers = atoi(argv[1]);
    number_of_stops = atoi(argv[2]);
    capacity_of_skibus = atoi(argv[3]);
    max_waiting_time = atoi(argv[4]);
    max_two_stops_time = atoi(argv[5]);
    if(number_of_skiers > 1000){
        fprintf(stderr, "Error: The number of the skiers cant exceed 20000\n");
        return 1;
    } else if (number_of_stops <= 0 || number_of_stops > 10){
        fprintf(stderr, "Error: The number of the stops must be between 1 and 10\n");
    } else if (capacity_of_skibus < 10 || capacity_of_skibus > 100){
        fprintf(stderr, "Error: The capacity of the skibus must be between 10 and 100\n");
    } else if (max_waiting_time < 0 || max_waiting_time > 10000){
        fprintf(stderr, "Error: The max waiting time must be between 1 and 10000\n");
    } else if (max_two_stops_time < 0 || max_two_stops_time > 1000){
        fprintf(stderr, "Error: The max two stops time must be between 1 and 1000\n");
    }





    return 0;
}
