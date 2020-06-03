#include "main.hpp"
#include "our_threads.hpp"

MPI_Datatype MPI_PACKET_T;

std::mutex globalAckMutex;
std::mutex lamportClockMutex;

std::mutex stopMutex;

std::mutex mutexToiletsState;
std::mutex mutexFlowerpotsState;
std::mutex mutexToilets;
std::mutex mutexFlowerpots;


// zmienne globalne statyczne
int size, rank, lamportClock, resourceCount, globalAck, reqLamportClock;
char role;
int badCount = 0, globalCount = 0;
state_t state;

// startowe wartości dla parametrów wyboru
int idChosen = -1;
char objectChosen = 'x';

// kolejki procesów blokowanych przez nas
std::vector<std::vector<int>> toilets;
std::vector<std::vector<int>> flowerpots;

// wektory przechowujace stan zasobów
std::vector<char> toiletsState;
std::vector<char> flowerpotsState;

std::set<int> usableFlowerpots;
std::set<int> usableToilets;

std::thread monitorThread;
std::thread communcationThread;

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <num_toilets> <num_flowerpots> [num_goodguy]" << std::endl;
        return 1;
    }

    // mutex zaczyna zamknięty
    globalAckMutex.lock();

    initialize(&argc, &argv);

    mainLoop();

    finalize();

    return 0;
}

/* Sprawdzamy jak dobre wsparcie dla wielowątkowości ma komputer */
void check_thread_support(int provided)
{
    printf("THREAD SUPPORT: %d\n", provided);
    switch (provided)
    {
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

/* Inicjalizacja zmiennych, tworzenie typu pakietu, losowanie ról i uruchamianie wątkow */
void initialize(int *argc, char ***argv)
{
    int argc1 = *argc;
    char **argv1 = *argv;

    int provided;
    MPI_Init_thread(argc, argv, MPI_THREAD_MULTIPLE, &provided);
    check_thread_support(provided);

    const int nitems = 4;
    int blocklengths[4] = {1, 1, 1, 1};
    MPI_Datatype typy[4] = {MPI_INT, MPI_CHAR, MPI_INT, MPI_CHAR};
    MPI_Aint offsets[4];
    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, type);
    offsets[2] = offsetof(packet_t, id);
    offsets[3] = offsetof(packet_t, action);

    MPI_Type_create_struct(nitems, blocklengths, offsets, typy, &MPI_PACKET_T);
    MPI_Type_commit(&MPI_PACKET_T);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    srand(rank);

    if (argc1 < 4) {
        role = (rand() % 100 >= 50) ? 'g' : 'b';

        if (rank == 0)
        {
            role = 'g';
        }
        else if (rank == 1)
        {
            role = 'b';
        }
    }
    else {
        if (rank < atoi(argv1[3])) {
            role = 'g';
        }
        else {
            role = 'b';
        }
    }

    int toiletsNum = atoi(argv1[1]);
    int flowerpotsNum = atoi(argv1[2]);

    toilets.resize(toiletsNum);
    flowerpots.resize(flowerpotsNum);
    toiletsState.resize(toiletsNum, 'g');
    flowerpotsState.resize(flowerpotsNum, 'g');

    if (role == 'b')
    {
        for (int i = 0; i < flowerpotsNum; i++)
            usableFlowerpots.insert(i);
        for (int i = 0; i < toiletsNum; i++)
            usableToilets.insert(i);
    }

    if (rank == 0)
    {
        monitorThread = std::thread(monitorLoop);
    }
    communcationThread = std::thread(communicationLoop);

    std::cout << rank << "." << lamportClock << " Initialized" << std::endl;
}

/* Usunięcie zamków, czeka, aż zakończy się drugi wątek, zwalnia przydzielony typ MPI_PACKET_T
wywoływane w funkcji main przed końcem */
void finalize()
{
    communcationThread.join();
    if (rank == 0)
    {
        monitorThread.join();
    }
    std::cout << rank << "." << lamportClock << " actions: " << globalCount - badCount << "/" <<
                globalCount << " = " << (float)(globalCount - badCount) / globalCount << " " << role << std::endl;

    MPI_Type_free(&MPI_PACKET_T);
    MPI_Finalize();
}

void mainLoop(void)
{
    const int baseChance = 50;
    const int missDecrease = 5;

    int tresh = baseChance;

    // Wykonujemy pętle dopóki nie dostaniemy enda od Monitora
    while (state != end)
    {

        stopMutex.lock();
        stopMutex.unlock();
        // Szansa na podjęcie akcji
        int chance = rand() % 100;
        // Wykonujemy akcję
        if (chance >= tresh)
        {
            // Resetujemy próg podjęcia akcji
            tresh = baseChance;

            int typeChance = rand() % 100; // Losujemy jaki typ zasobu chcemy dostać
            if (typeChance > 50) // Wylosowaliśmy toaletę
            {
                if (usableToilets.empty()) // Nie ma wolnych toalet, sprawdzamy doniczki
                {
                    if (usableFlowerpots.empty()) // Nie ma wolnych doniczek
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        continue;
                    }
                    else // Są wolne doniczki
                    {
                        objectChosen = 'f';
                        int idx = rand() % usableFlowerpots.size();
                        int x = *std::next(usableFlowerpots.begin(), idx);
                        usableFlowerpots.erase(x);
                        idChosen = x;
                    }
                }
                else // Są wolne toaletu
                {
                    objectChosen = 't';
                    int idx = rand() % usableToilets.size();
                    int x = *std::next(usableToilets.begin(), idx);
                    usableToilets.erase(x);
                    idChosen = x;
                }
            }
            else // Wylosowaliśmy donczikę
            {
                if (usableFlowerpots.empty()) // Nie ma wolnych doniczek, sprawdzamy toalety
                {
                    if (usableToilets.empty()) // Nie ma też wolnych toalet
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        continue;
                    }
                    else // Ale są wolne toalety
                    {
                        objectChosen = 't';
                        int idx = rand() % usableToilets.size();
                        int x = *std::next(usableToilets.begin(), idx);
                        usableToilets.erase(x);
                        idChosen = x;
                    }
                }
                else // Są wolne doniczki
                {
                    objectChosen = 'f';
                    int idx = rand() % usableFlowerpots.size();
                    int x = *std::next(usableFlowerpots.begin(), idx);
                    usableFlowerpots.erase(x);
                    idChosen = x;
                }
            }


            packet_t packet{};
            MPI_Status status{};

            std::cout << rank << "." << lamportClock << " Sending REQ to all" << std::endl;
            // Wysyłanie zapytań (requesty)
            MACRO_LOCK(lamportClockMutex, reqLamportClock = lamportClock);
            for (int i = 0; i < size; i++)
            {
                if (i == rank)
                {
                    continue;
                }
                sendPacket(&packet, i, TAG_REQ, objectChosen, idChosen, role);
            }

            // Przechodzimy tylko jeśli mamy wszystkie ACK
            globalAckMutex.lock();
            // Sekcja krytyczna
            

            // zależnie od zasobu dokonujemy naszej modyfikacji lub zliczamy błąd (nie wykonanie akcji)
            std::string succes = "failure";
            if (objectChosen == 't')
            {
                mutexToiletsState.lock();
                if (toiletsState[idChosen] == role)
                {
                    badCount++;
                }
                else
                {
                    toiletsState[idChosen] = role;
                    succes = "succes";
                }
                mutexToiletsState.unlock();
            }
            else if (objectChosen == 'f')
            {   
                mutexFlowerpotsState.lock();
                if (flowerpotsState[idChosen] == role)
                {
                    badCount++;
                }
                else
                {
                    flowerpotsState[idChosen] = role;
                    succes = "succes";
                }
                mutexFlowerpotsState.unlock();
            }
            std::cout << rank << "." << lamportClock << " Critical section action " <<  succes << std::endl;
            std::cout << rank << "." << lamportClock << " Local resource state [t,f] " << toiletsState << " " << flowerpotsState << std::endl;
            // zwiększamy liczbę dostepów do sekcji krytycznej
            globalCount++;

            std::cout << rank << "." << lamportClock << " Send INFO to all" << std::endl;
            // Wysylanie wiadomosci info o aktualizacji stanu danego zasobu
            for (int i = 0; i < size; i++)
            {
                if (i == rank)
                {
                    continue;
                }
                sendPacket(&packet, i, TAG_INFO, objectChosen, idChosen, role);
            }

            std::cout << rank << "." << lamportClock << " FREE processes (Send ACK)" << std::endl;
            // Usuwanie z kolejki (wysyłanie ACK do procesów zainteresowanych zasobem)
            if (objectChosen == 'f')
            {
                mutexFlowerpots.lock();
                for (int i = 0; i < flowerpots[idChosen].size(); i++)
                {
                    sendPacket(&packet, flowerpots[idChosen].back(), TAG_ACK, 'f', idChosen, role);
                    flowerpots[idChosen].pop_back();
                }
                mutexFlowerpots.unlock();
            }
            else if (objectChosen == 't') 
            {
                mutexToilets.lock();
                for (int i = 0; i < toilets[idChosen].size(); i++)
                {
                    sendPacket(&packet, toilets[idChosen].back(), TAG_ACK, 't', idChosen, role);
                    toilets[idChosen].pop_back();
                }
                mutexToilets.unlock();
            }

            // odpoczynek po wykonaniu akcji
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        else // wylosowaliśmy nie wykonywanie akcji
        {
            // śpimy przez 100 ms, zmniejszamy próg wykowania akcji (większa szansa na wykonanie akcji)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            tresh -= missDecrease;
        }
    }
}

// Funkcja obsługująca wysyłanie pakietów pomiędzy procesami, ustawia pakiet i go wysyła
void sendPacket(packet_t *pkt, int destination, int tag, char type, int id, char action)
{
    const std::lock_guard<std::mutex> lock(lamportClockMutex);
    pkt->type = type;
    pkt->id = id;
    // Zapobiega wysłaniu innego lamportclocka jeśli odbierzemy wiadomość w trakcie pętli
    if (tag == TAG_REQ)
    {
        pkt->ts = reqLamportClock;
    }
    else
    {
        pkt->ts = lamportClock;
    }
    pkt->action = action;
    MPI_Send(pkt, 1, MPI_PACKET_T, destination, tag, MPI_COMM_WORLD);
}
