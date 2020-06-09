#include "main.hpp"
#include "constants.hpp"
#include "our_threads.hpp"

/* Wątek odpalany przez jeden z procesów obsługujący komunikację z użytkownikie przez terminal */
void monitorLoop(void)
{
    bool isStop = false;
    std::cout << rank << "." << lamportClock << " Monitor thread started" << std::endl;
    std::string instruction = "";
    while (instruction != "end")
    {
        std::cin >> instruction;
        packet_t packet;

        // Po wpisaniu "end" użytkownik zamyka program
        if (instruction == "end" || instruction == "exit" || instruction == "quit") 
        {
            for (int i = 0; i < size; i++)
            {
                MPI_Send(&packet, 1, MPI_PACKET_T, i, TAG_END, MPI_COMM_WORLD);
            }
        } //Po wpisaniu "stop" użytkownik zatrzymuje działanie programu
        else if (instruction == "stop" || instruction == "pause" || instruction == "oh god please stop" ) 
        {
            if(!isStop)
            {
                for (int i = 0; i < size; i++)
                {
                    MPI_Send(&packet, 1, MPI_PACKET_T, i, TAG_STOP, MPI_COMM_WORLD);
                }
                std::cout << rank << "." << lamportClock << "\n\n\n---------->Program paused" << std::endl;
                isStop = true;
            }
            else
            {
                std::cout << rank << "." << lamportClock << "\n\n\n---------->Program was already paused" << std::endl;
            }
        } // Po wpisaniu "resume" użytkownik wznawia działanie programu
        else if (instruction == "resume" || instruction == "play" || instruction == "start") 
        {
            if(isStop)
            {
                for (int i = 0; i < size; i++)
                {
                    MPI_Send(&packet, 1, MPI_PACKET_T, i, TAG_RESUME, MPI_COMM_WORLD);
                }
                std::cout << rank << "." << lamportClock << "\n\n\n---------->Program resumed" << std::endl;
                isStop = false;
            }
            else
            {
                std::cout << rank << "." << lamportClock << "\n\n\n---------->Program wasn't stopped" << std::endl;
            }
        }
    }
    std::cout << rank << "." << lamportClock << " Monitor thread ended" << std::endl;
}

/* Wątek uruchamiany przez każdy proces, obsługuje komunikacje z innymi procesami (odbiera i reaguje na wiadomości) */
void communicationLoop(void)
{
    std::cout << rank << "." << lamportClock << " Communication thread started" << std::endl;

    while (state != end)
    {
        MPI_Status status{};
        packet_t packet{};

        MPI_Recv(&packet, 1, MPI_PACKET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        std::cout << rank << "." << lamportClock << " Recieved packet {action:" << packet.action << " id:" << packet.id << " type:" << packet.type << " ts:" << packet.ts << " tag:" << status.MPI_TAG << "}" << std::endl;

        lamportClockMutex.lock();
        lamportClock = max(lamportClock, packet.ts) + 1;
        lamportClockMutex.unlock();

        // Obsługa wiadomości
        switch (status.MPI_TAG)
        {
        case TAG_REQ:
            // dostaliśmy prośbę o zasób którego chcemy sami używać
            if (idChosen == packet.id && objectChosen == packet.type)
            {
                std::cout << rank << "." << lamportClock << " Fighting for resource " << objectChosen << "[" << idChosen << "] with process " << status.MPI_SOURCE << std::endl;
                // Mamy wyższy priorytet nic nie wysyłamy ( drugi wie że przegrał)
                if (packet.ts > reqLamportClock)
                {
                    std::cout << rank << "." << reqLamportClock << " I won[LC] " << objectChosen << "[" << idChosen << "] with process " << status.MPI_SOURCE << "." << packet.ts << std::endl;
                }
                else if (packet.ts < reqLamportClock) // Przegraliśmy, więc losujemy nowy zasób i wysyłamy ACK
                {
                    std::cout << rank << "." << reqLamportClock << " I lost " << objectChosen << "[" << idChosen << "] with process " << status.MPI_SOURCE << "." << packet.ts << std::endl;

                        idChosen = -1;
                        objectChosen = 'x';
                        globalAck = 0;
                        packetID += 1;
                        lostResource = true;
                        globalAckMutex.unlock();

                        if (objectChosen == 't')
                        {
                            MACRO_LOCK(mutexToiletsState, sendPacket(&packet, status.MPI_SOURCE, TAG_ACK, packet.type, packet.id, toiletsState[packet.id], packet.packet_id));
                        }
                        else
                        {
                            MACRO_LOCK(mutexFlowerpotsState, sendPacket(&packet, status.MPI_SOURCE, TAG_ACK, packet.type, packet.id, flowerpotsState[packet.id], packet.packet_id));
                        }
                }
                else // Było po równo więc porównujemy rangi
                {
                    if (rank > status.MPI_SOURCE)
                    {
                        std::cout << rank << "." << reqLamportClock << " I won[R] " << objectChosen << "[" << idChosen << "] with process " << status.MPI_SOURCE << "." << packet.ts << std::endl;
                    }
                    else // Mamy niższą rangę, wysyłamy ACK do drugiego procesu
                    {
                        std::cout << rank << "." << reqLamportClock << " I lost " << objectChosen << "[" << idChosen << "] with process " << status.MPI_SOURCE << "." << packet.ts << std::endl;

                        idChosen = -1;
                        objectChosen = 'x';
                        globalAck = 0;
                        packetID += 1;
                        lostResource = true;
                        globalAckMutex.unlock();

                        if (objectChosen == 't')
                        {
                            MACRO_LOCK(mutexToiletsState, sendPacket(&packet, status.MPI_SOURCE, TAG_ACK, packet.type, packet.id, toiletsState[packet.id], packet.packet_id));
                        }
                        else
                        {
                            MACRO_LOCK(mutexFlowerpotsState, sendPacket(&packet, status.MPI_SOURCE, TAG_ACK, packet.type, packet.id, flowerpotsState[packet.id], packet.packet_id));
                        }
                    }
                }
            }
            else // Nie interesuje nas zasób
            {
                // Wysyłamy ACK do proszącego procesu
                std::cout << rank << "." << lamportClock << " I allow resource " << packet.type << "[" << packet.id << "] for process " << status.MPI_SOURCE << std::endl;
                if (packet.type == 't')
                {
                    if (packet.action == role)
                        usableToilets.erase(packet.id);
                    MACRO_LOCK(mutexToiletsState, sendPacket(&packet, status.MPI_SOURCE, TAG_ACK, packet.type, packet.id, toiletsState[packet.id], packet.packet_id));
                }
                else
                {
                    if (packet.action == role)
                        usableFlowerpots.erase(packet.id);
                    MACRO_LOCK(mutexFlowerpotsState, sendPacket(&packet, status.MPI_SOURCE, TAG_ACK, packet.type, packet.id, flowerpotsState[packet.id], packet.packet_id));
                }
            }
            break;
        case TAG_ACK:
            // Zwiększamy licznik otrzymanych zgód
            if (packet.packet_id != packetID)
            {
                std::cout << rank << "." << lamportClock << " I received old ACK and ignored it" << std::endl;
                break;
            }
            globalAck++;
            // Aktualizujemy wartość zasobu na otrzymaną od procesu
            if (objectChosen == 't')
            {
                if (packet.action == 'g' || packet.action == 'b')
                {
                    MACRO_LOCK(mutexToiletsState ,toiletsState[packet.id] = packet.action);
                    /*if (packet.action != role)
                        usableToilets.insert(packet.id);
                    else
                        usableToilets.erase(packet.id);*/
                }
            }
            else if (objectChosen == 'f')
            {
                if (packet.action == 'g' || packet.action == 'b')
                {
                    MACRO_LOCK(mutexFlowerpotsState, flowerpotsState[packet.id] = packet.action);
                    /*if (packet.action != role)
                        usableFlowerpots.insert(packet.id);
                    else
                        usableFlowerpots.erase(packet.id);*/
                }
            }

            // Mamy wszystkie zgody
            if (globalAck == size - 1)
            {
                std::cout << rank << "." << reqLamportClock << " I have all the ACKs " << objectChosen << "[" << idChosen << "]" << std::endl;
                globalAck = 0;
                globalAckMutex.unlock();
            }
            break;
        case TAG_INFO:
            // Aktualizujemy wartość zasobu na taką jak w orzymanym pakiecie
            if (packet.type == 't')
            {
                if (packet.action == 'g' || packet.action == 'b')
                {
                    MACRO_LOCK(mutexToiletsState, toiletsState[packet.id] = packet.action);
                    if (packet.action != role)
                        usableToilets.insert(packet.id);
                    else
                        usableToilets.erase(packet.id);
                }
            }
            else if (packet.type == 'f')
            {
                if (packet.action == 'g' || packet.action == 'b')
                {
                    MACRO_LOCK(mutexFlowerpotsState ,flowerpotsState[packet.id] = packet.action);
                    if (packet.action != role)
                        usableFlowerpots.insert(packet.id);
                    else
                        usableFlowerpots.erase(packet.id);
                }
            }

            break;
        case TAG_STOP:
            // Zatrzymujemy program
            stopMutex.lock();
            break;
        case TAG_RESUME:
            // Wznawiamy program
            stopMutex.unlock();
            break;
        case TAG_END:
            // Kończymy działanie programu
            std::cout << rank << "." << lamportClock << " Recieved END" << std::endl;
            stopMutex.unlock();
            globalAckMutex.unlock();
            state = end;
            return;
        }
    }
}

/* Funkcja zwraca większą z dwóch podanych liczb całkowitych) */
int max(int a, int b)
{
    return (a > b) ? a : b;
}