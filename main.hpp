#ifndef _MAIN_HPP_
#define _MAIN_HPP_

#include <mpi.h>
#include <cstddef>
#include <iostream>
#include <thread>
#include <vector>
#include <cstdlib>
#include <string>
#include <chrono>
#include <mutex>
#include "constants.hpp"

typedef struct {
    int ts;  /* timestamp (zegar lamporta */
    char type; /* pole nie przesyłane, ale ustawiane w main_loop */
    int id;
    char action; /* przykładowe pole z danymi; można zmienić nazwę na bardziej pasującą */
} packet_t;

typedef enum {
    init,
    req_resource,
    wait_ack,
    critical_section,
    azerstaff_sleep,
    end
} state_t;

extern MPI_Datatype MPI_PACKET_T;
extern int size;
extern int rank;
extern int lamportClock;
extern int reqLamportClock;
extern int resourceCount;
extern char role;
extern state_t state;
extern int globalAck;
extern std::mutex globalAckMutex;
extern std::mutex lamportClockMutex;

extern int idChosen;
extern char objectChosen;

extern std::vector<std::vector<int>> toilets;
extern std::vector<std::vector<int>> flowerpots;
extern std::vector<char> toiletsState;
extern std::vector<char> flowerpotsState;


void check_thread_support(int provided);
void initialize(int *argc, char ***argv);
void mainLoop(void);
void finalize();
void sendPacket(packet_t *pkt, int destination, int tag, char type, int id, char action);


template<typename T>
inline std::ostream& operator << (std::ostream& os, const std::vector<T>& v){
    os << "[";
    for(const auto &i : v){
        os << " " <<  i;
    }
    os << " ]";
    return os;
}


#endif //_MAIN_HPP_