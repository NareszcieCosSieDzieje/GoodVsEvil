#include "main.hpp"
#include "constants.hpp"
#include "our_threads.hpp"

void monitorLoop(void){
    std::cout << "Monitor started" << std::endl;
    std::string instruction = "";
    while(instruction != "end"){
        std::cin >> instruction;
        packet_t packet;

        if (instruction == "testack") {   
            for (int i = 0; i < size; i++) {
                MPI_Send(&packet, 1, MPI_PACKET_T, i, TAG_ACK, MPI_COMM_WORLD);
            }
        }
        else if (instruction == "testreq"){
            for (int i = 0; i < size; i++) {
                MPI_Send(&packet, 1, MPI_PACKET_T, i, TAG_REQ, MPI_COMM_WORLD);
            }
        }
        else if (instruction == "end"){
            for (int i = 0; i < size; i++) {
                MPI_Send(&packet, 1, MPI_PACKET_T, i, TAG_END, MPI_COMM_WORLD);
            }
        }
    }
    std::cout << "Monitor ended" << std::endl;
}

void communicationLoop(void) {
    std::cout << rank << ". Communication started" << std::endl;
    MPI_Status status{};
    packet_t packet{};

    while(true) {
        MPI_Recv(&packet, 1, MPI_PACKET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        lamportClockMutex.lock();
        lamportClock = max(lamportClock, packet.ts) + 1;
        lamportClockMutex.unlock();

        switch(status.MPI_TAG) {
        case TAG_REQ:
            std::cout << rank << ". Recieved REQ" << std::endl;
            //jesli nie chcemy zasobu to to robimy
            if (idChosen == packet.id && objectChosen == packet.type) {
                if (lamportClock > packet.ts) {
                    //dodaj do kolejki
                    if (objectChosen == 't') {
                        toilets[idChosen].push_back(status.MPI_SOURCE);
                    } else if (objectChosen == 'f') {
                        flowerpots[idChosen].push_back(status.MPI_SOURCE);
                    }
                }
                else if (lamportClock < packet.ts) {
                    MPI_Send(&packet, 1, MPI_PACKET_T, status.MPI_SOURCE, TAG_ACK, MPI_COMM_WORLD);
                }
                else {
                    if (rank > status.MPI_SOURCE) {
                        if (objectChosen == 't') {
                            toilets[idChosen].push_back(status.MPI_SOURCE);
                        } else if (objectChosen == 'f') {
                            flowerpots[idChosen].push_back(status.MPI_SOURCE);
                        }
                    }
                    else {
                        MPI_Send(&packet, 1, MPI_PACKET_T, status.MPI_SOURCE, TAG_ACK, MPI_COMM_WORLD);
                    }
                }
            }
            else {
                MPI_Send(&packet, 1, MPI_PACKET_T, status.MPI_SOURCE, TAG_ACK, MPI_COMM_WORLD);
            }
            break;
        case TAG_ACK: //FIXME: 
            std::cout << rank << ". Recieved ACK" << std::endl;
            globalAck++;
            if(globalAck == size-1){
                globalAck=0;
                globalAckMutex.unlock();
            }
            break;
       case TAG_END:
            std::cout << rank << ". Recieved END" << std::endl;
            state = end;
            return;
        }
    }
    std::cout << rank << ". Communication ended" << std::endl;
}

int max(int a, int b) {
    return (a > b)? a : b;
}