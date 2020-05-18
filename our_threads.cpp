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
        if (instruction == "end") 
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
                std::cout << rank << "." << reqLamportClock << " Fighting for resource " << objectChosen << "[" << idChosen << "] with process " << status.MPI_SOURCE << std::endl;
                // Mamy wyższy priorytet, dodajemy proces do kolejki procesów blokowanych
                if (reqLamportClock > packet.ts)
                {
                    std::cout << rank << "." << reqLamportClock << " I won " << objectChosen << "[" << idChosen << "] with process " << status.MPI_SOURCE << "." << packet.ts << std::endl;
                    if (objectChosen == 't')
                    {
                        toilets[idChosen].push_back(status.MPI_SOURCE);
                    }
                    else if (objectChosen == 'f')
                    {
                        flowerpots[idChosen].push_back(status.MPI_SOURCE);
                    }
                }
                else if (reqLamportClock < packet.ts) // Mamy niższy priorytet, wysyłamy ACK do drugiego procesu
                {
                    std::cout << rank << "." << reqLamportClock << " I lost " << objectChosen << "[" << idChosen << "] with process " << status.MPI_SOURCE << "." << packet.ts << std::endl;
                    if (objectChosen == 't')
                    {
                        sendPacket(&packet, status.MPI_SOURCE, TAG_ACK, packet.type, packet.id, toiletsState[packet.id]);
                    }
                    else
                    {
                        sendPacket(&packet, status.MPI_SOURCE, TAG_ACK, packet.type, packet.id, flowerpotsState[packet.id]);
                    }
                }
                else // Mamy równe priorytety
                {
                    // Mamy wyższą rangę, dodajemy proces do kolejki procesów blokowanych
                    if (rank > status.MPI_SOURCE)
                    {
                        std::cout << rank << "." << reqLamportClock << " I won " << objectChosen << "[" << idChosen << "] with process " << status.MPI_SOURCE << "." << packet.ts << std::endl;
                        if (objectChosen == 't')
                        {
                            toilets[idChosen].push_back(status.MPI_SOURCE);
                        }
                        else if (objectChosen == 'f')
                        {
                            flowerpots[idChosen].push_back(status.MPI_SOURCE);
                        }
                    }
                    else // Mamy niższą rangę, wysyłamy ACK do drugiego procesu
                    {
                        std::cout << rank << "." << reqLamportClock << " I lost " << objectChosen << "[" << idChosen << "] with process " << status.MPI_SOURCE << "." << packet.ts << std::endl;
                        if (objectChosen == 't')
                        {
                            sendPacket(&packet, status.MPI_SOURCE, TAG_ACK, packet.type, packet.id, toiletsState[packet.id]);
                        }
                        else
                        {
                            sendPacket(&packet, status.MPI_SOURCE, TAG_ACK, packet.type, packet.id, flowerpotsState[packet.id]);
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
                    sendPacket(&packet, status.MPI_SOURCE, TAG_ACK, packet.type, packet.id, toiletsState[packet.id]);
                }
                else
                {
                    sendPacket(&packet, status.MPI_SOURCE, TAG_ACK, packet.type, packet.id, flowerpotsState[packet.id]);
                }
            }
            break;
        case TAG_ACK:
            // Zwiększamy licznik otrzymanych zgód
            globalAck++;
            // Aktualizujemy wartość zasobu na otrzymaną od procesu
            if (objectChosen == 't')
            {
                if (packet.action == 'g' || packet.action == 'b')
                    toiletsState[packet.id] = packet.action;
            }
            else if (objectChosen == 'f')
            {
                if (packet.action == 'g' || packet.action == 'b')
                    flowerpotsState[packet.id] = packet.action;
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
                    toiletsState[packet.id] = packet.action;
            }
            else if (packet.type == 'f')
            {
                if (packet.action == 'g' || packet.action == 'b')
                    flowerpotsState[packet.id] = packet.action;
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