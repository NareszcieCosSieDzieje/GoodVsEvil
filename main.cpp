#include "main.hpp"
#include "our_threads.hpp"

MPI_Datatype MPI_PACKET_T;

std::mutex globalAckMutex;
std::mutex lamportClockMutex;

// Zmienne globalne statyczne
int size, rank, lamportClock, resourceCount, globalAck, reqLamportClock;
char role;
int badCount = 0, globalCount = 0;
state_t state;

int idChosen = -1;
char objectChosen = 'x';

std::vector<std::vector<int>> toilets;
std::vector<std::vector<int>> flowerpots;

std::vector<char> toiletsState;
std::vector<char> flowerpotsState;

std::thread monitorThread;
std::thread communcationThread;
/*
Wektor list blokowanych procesów dla doniczek -
    Tablica zawierająca tablice ID procesów
Wektor list blokowanych procesów dla toalet -
    Tablica zawierająca tablice ID procesów
Wektor zgód dla toalet -
    Tablica zawierająca liczby
Wektor zgód dla doniczek -
    Tablica zawierająca liczby
*/

int main(int argc, char** argv){
    if (argc < 3) {
        std::cerr << "Usage: <num_toilets> <num_flowerpots>" << std::endl;
        return 1;
    }

    
    globalAckMutex.lock();


    initialize(&argc, &argv);

    //doMain

    mainLoop();
    

    finalize();
    return 0;
}



void check_thread_support(int provided) {
    printf("THREAD SUPPORT: %d\n", provided);
    switch(provided) {
        case MPI_THREAD_SINGLE: 
            printf("Brak wsparcia dla wątków, kończę\n");
	        fprintf(stderr, "Brak wystarczającego wsparcia dla wątków - wychodzę!\n");
            MPI_Finalize();
            exit(-1);
	    break;
        case MPI_THREAD_FUNNELED: 
            printf("tylko te wątki, ktore wykonaly mpi_init_thread mogą wykonać wołania do biblioteki mpi\n");
	    break;
        case MPI_THREAD_SERIALIZED: 
            /* Potrzebne zamki wokół wywołań biblioteki MPI */
            printf("tylko jeden watek naraz może wykonać wołania do biblioteki MPI\n");
	    break;
        case MPI_THREAD_MULTIPLE: 
            printf("Pełne wsparcie dla wątków\n");
	    break;
        default: 
            printf("Nikt nic nie wie\n");
    }
}


void initialize(int *argc, char ***argv) {
    int argc1 = *argc;
    char **argv1 = *argv;

    
    int provided;
    MPI_Init_thread(argc, argv, MPI_THREAD_MULTIPLE, &provided);
    check_thread_support(provided);

    const int nitems=4;
    int       blocklengths[4] = {1,1,1,1};
    MPI_Datatype typy[4] = {MPI_INT, MPI_CHAR, MPI_INT, MPI_CHAR};
    MPI_Aint     offsets[4]; 
    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, type);
    offsets[2] = offsetof(packet_t, id);
    offsets[3] = offsetof(packet_t, action);

    MPI_Type_create_struct(nitems, blocklengths, offsets, typy, &MPI_PACKET_T);
    MPI_Type_commit(&MPI_PACKET_T);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    srand(rank);
    
    //enum role {good, bad} eRole;
    //TODO: zagwarantowac przynjmaniej jednego zlego i jednego dobrego
    
    

    role = (rand() % 100 >= 50)? 'g' : 'b';
    
    
    if (rank == 0) {
        role = 'g';
    } else if (rank == 1) {
        role = 'b';
    }
    

    if (role == 'g') {
        std::cout << "Process " << rank << " is a Goodguy :~)" << std::endl;
        //startGood()
    }
    else {
        std::cout << "Process " << rank << " is a Badguy :~(" << std::endl;
        //startBad()
    }

    int toiletsNum = atoi(argv1[1]);
    int flowerpotsNum = atoi(argv1[2]);

    toilets.resize(toiletsNum);
    flowerpots.resize(flowerpotsNum);
    toiletsState.resize(toiletsNum, 'g');
    flowerpotsState.resize(flowerpotsNum, 'g');
    
    if (rank == 0) {
        std::cout << "Resource Count: " << toiletsNum + flowerpotsNum << std::endl;
        std::cout << " - Toilet Count: " << toilets.size() << std::endl;
        std::cout << " - Flowerpots Count: " << flowerpots.size() << std::endl;
        monitorThread = std::thread(monitorLoop);
    }
    communcationThread = std::thread(communicationLoop);
    std::cout << rank << "." << lamportClock << " " << toiletsState << flowerpotsState << std::endl;

    std::cout << rank+1 << "/" << size << " Initialized" << std::endl;
}

/*
usunięcie zamków, czeka, aż zakończy się drugi wątek, zwalnia przydzielony typ MPI_PACKET_T
wywoływane w funkcji main przed końcem
*/
void finalize() {
    //TODO: println("czekam na wątek \"komunikacyjny\"\n" );
    communcationThread.join();
    if (rank == 0) {
        monitorThread.join();
    }
    std::cout << rank << "." << lamportClock << " actions: " << globalCount - badCount << "/" << globalCount << " = " << (float) (globalCount - badCount)/globalCount << std::endl;

    MPI_Type_free(&MPI_PACKET_T);
    MPI_Finalize();
}

void mainLoop(void) {
    const int baseChance = 50;
    const int missDecrease = 5;
    
    int tresh = baseChance;
    std::cout << rank << ". MainLoop Entered" << std::endl;
    while (state != end) {
        int chance = rand() % 100;
        if (chance >= tresh) {
            tresh = baseChance;
            
            std::vector<char> currentResource = toiletsState;

            // losowanie aż do wylosowania odpowiedniego zasobu
            
            do {
                int typeChance = rand() % 100;
                if (typeChance > 50) {
                    objectChosen = 't';
                    currentResource = toiletsState;
                    idChosen = rand() % toilets.size();
                } else {
                    objectChosen = 'f';
                    currentResource = flowerpotsState;
                    idChosen = rand() % flowerpots.size();
                }
            } while (currentResource[idChosen] == role);

            //std::cout << rank << "." << lamportClock << " Requesting resource " << objectChosen << "[" << idChosen << "]" << std::endl;
            
            packet_t packet{};
            MPI_Status status{};

            // send request
            reqLamportClock = lamportClock;
            for(int i = 0; i < size; i++){
                if(i == rank){ 
                    continue;
                }
                sendPacket(&packet, i, TAG_REQ, objectChosen, idChosen, role);
            }
            std::cout << rank <<". After REQ :" << toiletsState << flowerpotsState << std::endl;
            
            globalAckMutex.lock();

            if (objectChosen == 't') {
                if (toiletsState[idChosen] == role) {
                    //std::cout << rank << ". I wanted to do " << role << " but it was already " << toiletsState[idChosen] << std::endl;
                    badCount++;
                }
                else {
                    toiletsState[idChosen] = role;
                    //std::cout << rank << ". I wanted to do " << role << " and I did it" << std::endl;
                }
            }
            else if (objectChosen == 'f') {
                if (flowerpotsState[idChosen] == role) {
                    //std::cout << rank << ". I wanted to do " << role << " but it was already " << flowerpotsState[idChosen] << std::endl;
                    badCount++;
                }
                else {
                    flowerpotsState[idChosen] = role;
                    //std::cout << rank << ". I wanted to do " << role << " and I did it" << std::endl;
                }
            }
            globalCount++;

            // sekcja krytyczna

            // Wysylanie info o aktualizacji wartosci
            for (int i = 0; i < size; i++) {
                if (i == rank) {
                    continue;
                }
                sendPacket(&packet, i, TAG_INFO, objectChosen, idChosen, role);
            }
            
            // usuwanie z kolejki (wysyłanie do ludzi ack)
            if (objectChosen == 'f') {
                for (int i = 0; i < flowerpots[idChosen].size(); i++) {
                    //std::cout << rank << ". Send to " << flowerpots[idChosen].back() << std::endl;
                    sendPacket(&packet, flowerpots[idChosen].back(), TAG_ACK, 'f', idChosen, role);
                    flowerpots[idChosen].pop_back();
                }
            }
            else if (objectChosen == 't') {
                for (int i = 0; i < toilets[idChosen].size(); i++) {
                    //std::cout << rank << ". Send to " << toilets[idChosen].back() << std::endl;
                    sendPacket(&packet, toilets[idChosen].back(), TAG_ACK, 't', idChosen, role);
                    toilets[idChosen].pop_back();
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } else {
            //std::cout << rank << ". Goes to sleep" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            tresh -= missDecrease;
        }
        chance = rand() % 100;
        if (chance > baseChance) {
            //std::cout << rank << ". Goes to sleep" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}


void sendPacket(packet_t *pkt, int destination, int tag, char type, int id, char action) {
    const std::lock_guard<std::mutex> lock(lamportClockMutex);
    pkt->type = type;
    pkt->id = id;
    if (tag == TAG_REQ) {
        pkt->ts = reqLamportClock;
    }
    else {
        pkt->ts = lamportClock;
    }
    pkt->action = action;
    MPI_Send(pkt, 1, MPI_PACKET_T, destination, tag, MPI_COMM_WORLD);
}
