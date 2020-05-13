#include "main.hpp"


 /* TODO: Stworzenie typu */
    /* Poniższe (aż do MPI_Type_commit) potrzebne tylko, jeżeli
       brzydzimy się czymś w rodzaju MPI_Send(&typ, sizeof(pakiet_t), MPI_BYTE....
    */
    /* sklejone z stackoverflow */
   
/*
    const int nitems=3; bo packet_t ma trzy pola 
    int       blocklengths[3] = {1,1,1};
    MPI_Datatype typy[3] = {MPI_INT, MPI_INT, MPI_INT};
    MPI_Aint     offsets[3]; 
    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, src);
    offsets[2] = offsetof(packet_t, data);

    MPI_Type_create_struct(nitems, blocklengths, offsets, typy, &MPI_PAKIET_T);
    MPI_Type_commit(&MPI_PAKIET_T);
*/

MPI_Datatype MPI_PAKIET_T;

int size, rank, lamportClock; /* nie trzeba zerować, bo zmienna globalna statyczna */
//TODO: MPI_Datatype MPI_PAKIET_T;



int main(int argc, char** argv){
    initialize(&argc, &argv);

    //doMain

    finalize();
    return 0;
}



void check_thread_support(int provided)
{
    printf("THREAD SUPPORT: %d\n", provided);
    switch(provided) {
        case MPI_THREAD_SINGLE: 
            printf("Brak wsparcia dla wątków, kończę\n");
            /* Nie ma co, trzeba wychodzić */
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
        default: printf("Nikt nic nie wie\n");
    }
}


void initialize(int *argc, char ***argv)
{
    int provided;
    MPI_Init_thread(argc, argv, MPI_THREAD_MULTIPLE, &provided);
    check_thread_support(provided);

    const int nitems=4;// bo packet_t ma trzy pola 
    int       blocklengths[4] = {1,1,1,1};
    MPI_Datatype typy[4] = {MPI_INT, MPI_CHAR, MPI_INT, MPI_CHAR};
    MPI_Aint     offsets[4]; 
    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, type);
    offsets[2] = offsetof(packet_t, id);
    offsets[3] = offsetof(packet_t, action);

    MPI_Type_create_struct(nitems, blocklengths, offsets, typy, &MPI_PAKIET_T);
    MPI_Type_commit(&MPI_PAKIET_T);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    srand(rank);

    if (rank==0) {
        //TODO: monitor
    }
    printf("%d/%d Started\n", rank+1, size);
    //TODO: debug("jestem");
}

/* usunięcie zamkków, czeka, aż zakończy się drugi wątek, zwalnia przydzielony typ MPI_PAKIET_T
   wywoływane w funkcji main przed końcem
*/
void finalize()
{
    //TODO: println("czekam na wątek \"komunikacyjny\"\n" );

    //MPI_Type_free(&MPI_PAKIET_T);
    MPI_Finalize();
}


/*
    void sendPacket(packet_t *pkt, int destination, int tag){
    int freepkt=0;
    if (pkt==0) { pkt = malloc(sizeof(packet_t)); freepkt=1;}
    pkt->src = rank;
    pthread_mutex_lock( &clockMutex );
    pkt->ts = lamportClock++;
    pthread_mutex_unlock( &clockMutex );
    MPI_Send( pkt, 1, MPI_PAKIET_T, destination, tag, MPI_COMM_WORLD);
    if (freepkt) free(pkt);
}
*/

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
