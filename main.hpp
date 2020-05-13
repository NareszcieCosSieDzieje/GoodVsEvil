#pragma once

#include <mpi.h>
#include <cstddef>
#include <iostream>
#include <thread>

typedef struct
{
    int ts;  /* timestamp (zegar lamporta */
    char type; /* pole nie przesyłane, ale ustawiane w main_loop */
    int id;
    char action; /* przykładowe pole z danymi; można zmienić nazwę na bardziej pasującą */
} packet_t;

extern MPI_Datatype MPI_PAKIET_T;

void check_thread_support(int provided);
void initialize(int *argc, char ***argv);
void finalize();