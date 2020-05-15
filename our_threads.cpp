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
    //MPI_Status status{};
    //packet_t packet{};

    while(state != end) {
        MPI_Status status{};
        packet_t packet{};
        MPI_Recv(&packet, 1, MPI_PACKET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        std::cout << rank << ". Insta RECV action:" << packet.action << " id:" << packet.id << " type:" << packet.type << " ts:" << packet.ts << " tag:" << status.MPI_TAG << std::endl;
        //lamportClockMutex.lock();
        lamportClock = max(lamportClock, packet.ts) + 1;
        //lamportClockMutex.unlock();

        switch(status.MPI_TAG) {
        case TAG_REQ:
            //std::cout << rank << ". Recieved REQ" << std::endl;
            //jesli nie chcemy zasobu to to robimy
            if (idChosen == packet.id && objectChosen == packet.type) {
                std::cout << rank << "." << reqLamportClock << " Fighting for resource " << objectChosen << "[" << idChosen << "] with process " << status.MPI_SOURCE << std::endl;
                if (reqLamportClock > packet.ts) {
                    std::cout << rank << "." << reqLamportClock << " I won " << objectChosen << "[" << idChosen << "] with process " << status.MPI_SOURCE << "." << packet.ts << std::endl;
                    //dodaj do kolejki
                    if (objectChosen == 't') {
                        toilets[idChosen].push_back(status.MPI_SOURCE);
                    } else if (objectChosen == 'f') {
                        flowerpots[idChosen].push_back(status.MPI_SOURCE);
                    }
                }
                else if (reqLamportClock < packet.ts) {
                    std::cout << rank << "." << reqLamportClock << " I lost " << objectChosen << "[" << idChosen << "] with process " << status.MPI_SOURCE << "." << packet.ts << std::endl;
                    if (objectChosen == 't') {
                        sendPacket(&packet, status.MPI_SOURCE, TAG_ACK, packet.type, packet.id, toiletsState[packet.id]);
                    }
                    else {
                        sendPacket(&packet, status.MPI_SOURCE, TAG_ACK, packet.type, packet.id, flowerpotsState[packet.id]);
                    }
                }
                else {
                    if (rank > status.MPI_SOURCE) {
                        std::cout << rank << "." << reqLamportClock << " I won " << objectChosen << "[" << idChosen << "] with process " << status.MPI_SOURCE << "." << packet.ts << std::endl;
                        if (objectChosen == 't') {
                            toilets[idChosen].push_back(status.MPI_SOURCE);
                        } else if (objectChosen == 'f') {
                            flowerpots[idChosen].push_back(status.MPI_SOURCE);
                        }
                    }
                    else {
                        std::cout << rank << "." << reqLamportClock << " I lost " << objectChosen << "[" << idChosen << "] with process " << status.MPI_SOURCE << "." << packet.ts << std::endl;
                        if (objectChosen == 't') {
                            sendPacket(&packet, status.MPI_SOURCE, TAG_ACK, packet.type, packet.id, toiletsState[packet.id]);
                        }
                        else {
                            sendPacket(&packet, status.MPI_SOURCE, TAG_ACK, packet.type, packet.id, flowerpotsState[packet.id]);
                        }
                    }
                }
            }
            else {
                //std::cout << rank << "." << lamportClock << " I allow resource " << packet.type << "[" << packet.id << "] for process " << status.MPI_SOURCE << std::endl;
                if (objectChosen == 't') {
                    sendPacket(&packet, status.MPI_SOURCE, TAG_ACK, packet.type, packet.id, toiletsState[packet.id]);
                }
                else {
                    sendPacket(&packet, status.MPI_SOURCE, TAG_ACK, packet.type, packet.id, flowerpotsState[packet.id]);
                }
            }
            break;
        case TAG_ACK: //FIXME: 
            //std::cout << rank << ". Recieved ACK" << std::endl;
            std::cout << rank << ". Recieved ACK action: " << packet.action << std::endl;
            std::cout << rank <<". From ACK :" << toiletsState << flowerpotsState << std::endl;
            globalAck++;
            if (objectChosen == 't') {
                toiletsState[packet.id] = packet.action;
            }
            else if (objectChosen == 'f') {
                flowerpotsState[packet.id] = packet.action;
            }
            if (globalAck == size-1) {
                std::cout << rank << "." << reqLamportClock << " I have all the ACKs " << objectChosen << "[" << idChosen << "]" << std::endl;
                globalAck = 0;
                globalAckMutex.unlock();
            }
            break;
       case TAG_END:
            std::cout << rank << "." << lamportClock << " Recieved END" << std::endl;
            globalAckMutex.unlock(); //FIXME: tymczasowe ale dziala
            state = end;
            return;
        }
    }
}

int max(int a, int b) {
    return (a > b)? a : b;
}