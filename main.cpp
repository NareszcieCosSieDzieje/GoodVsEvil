#include "main.hpp"
#include "our_threads.hpp"

MPI_Datatype MPI_PACKET_T;

std::mutex globalAckMutex;
std::mutex lamportClockMutex;

// Zmienne globalne statyczne
int size, rank, lamportClock, resourceCount, globalAck;
char role;
state_t state;

int idChosen = -1;
char objectChosen = 'x';

std::vector<std::vector<int>> toilets;
std::vector<std::vector<int>> flowerpots;

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

    globalAckMutex.lock();


    printf("Wchodze w init\n");
    initialize(&argc, &argv);

    printf("Wchpodze w mainLoop\n");    
    //doMain

    mainLoop();
    

    printf("Wchodze w final\n");

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
    role = (rand() % 100 >= 50)? 'g' : 'b';
    
    if(role == 'g') {
        std::cout << "Process " << rank << " is a Goodguy :~)" << std::endl;
        //startGood()
    }
    else {
        std::cout << "Process " << rank << " is a Badguy :~(" << std::endl;
        //startBad()
    }

    
    if(rank == 0){
        resourceCount = size / 2;
        int randomCount = rand() % (resourceCount - 1) + 1;
        toilets.resize(randomCount);
        flowerpots.resize(resourceCount - randomCount);
        std::cout << "Resource Count: " << resourceCount << std::endl;
        std::cout << " - Toilet Count: " << toilets.size() << std::endl;
        std::cout << " - Flowerpots Count: " << flowerpots.size() << std::endl;
        monitorThread = std::thread(monitorLoop);
    }
    communcationThread = std::thread(communicationLoop);

    std::cout << rank+1 << "/" << size << " Initialized" << std::endl;
    //TODO: debug("jestem");
}

/* usunięcie zamków, czeka, aż zakończy się drugi wątek, zwalnia przydzielony typ MPI_PACKET_T
   wywoływane w funkcji main przed końcem
*/
void finalize() {
    //TODO: println("czekam na wątek \"komunikacyjny\"\n" );
    communcationThread.join();
    if (rank == 0) {
        monitorThread.join();
    }

    MPI_Type_free(&MPI_PACKET_T);
    MPI_Finalize();
}

//TODO: dodac petle
void mainLoop(void) {
    const int baseChance = 50;
    const int missDecrease = 5;
    
    int tresh = baseChance;
    while (state != end) {
        int chance = rand() % 100;
        if (chance >= tresh) {
            tresh = baseChance;

            idChosen = -1;        
            objectChosen = 'x';
            if(rand()%2){
                objectChosen = 't';
                idChosen = rand() % toilets.size();
            } else {
                objectChosen = 'f';
                idChosen = rand() % flowerpots.size();
            }
            printf("DEBUG1\n");
            // losowanie toaleta vs doniczka
            // losowanie id
            packet_t packet{};
            MPI_Status status{};

            // send request
            for(int i = 0; i< size; i++){
                if(i == rank){ 
                    continue;
                }
                //TODO: packet uzupelnic, moze jakas funkcyjka
                sendPacket(&packet, i, TAG_REQ, objectChosen, rank, role); 
            }
            printf("DEBUG2\n");

            
            globalAckMutex.lock();

            printf("DEBUG3\n");
            // sekcja krytyczna

            // usuwanie z kolejki (wysyłanie do ludzi ack)
            if (objectChosen == 'f') {
                for (int i = 0; i < flowerpots[idChosen].size(); i++) {
                    sendPacket(&packet, flowerpots[idChosen].back(), TAG_ACK, 'f', idChosen, role);
                    flowerpots[idChosen].pop_back();
                }
            }
            else if (objectChosen == 't') {
                for (int i = 0; i < toilets[idChosen].size(); i++) {
                    sendPacket(&packet, toilets[idChosen].back(), TAG_ACK, 't', idChosen, role);
                    toilets[idChosen].pop_back();
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            tresh -= missDecrease;
        }
        chance = rand() % 100;
        if (chance > tresh) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}


void sendPacket(packet_t *pkt, int destination, int tag, char type, int id, char action) {
    const std::lock_guard<std::mutex> lock(lamportClockMutex);
    pkt->type = type;
    pkt->id = id;
    pkt->ts = lamportClock++;
    pkt->action = action;
    MPI_Send(pkt, 1, MPI_PACKET_T, destination, tag, MPI_COMM_WORLD);
}

/*
    void changeState( state_t newState )
    {
    pthread_mutex_lock( &stateMut );
    if (stan==InFinish) { 
	pthread_mutex_unlock( &stateMut );
        return;
    }
    stan = newState;
    pthread_mutex_unlock( &stateMut );
}
*/
