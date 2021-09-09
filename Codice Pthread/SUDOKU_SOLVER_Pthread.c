#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#define MAX_THREADS 1024
#define SUDOKU_SIZE 9
#define SUDOKU_SQRT 3
//gcc -o SUDOKU_SOLVER_Pthread SUDOKU_SOLVER_Pthread.c -pthread


//ogetto coordinata per rappresentare le celle vuote
typedef struct{
    int row;
    int column;
} Coordinate;

//ogetto sudoku che contiene la grid da riempire, la lista delle coordinate delle celle vuote, e l'indice della prima cella vuota da riempire
typedef struct{
    int grid[SUDOKU_SIZE][SUDOKU_SIZE];
    Coordinate empty_cells[SUDOKU_SIZE*SUDOKU_SIZE];
    int index_next_empty_cell;
} Sudoku;

//ogetto nodo, componente della FIFO
typedef struct{
    int previous_node;
    int next_node;
} Node;

//ogetto FIFO, una lista lincata di nodi sulla quale e' possibile fare pop e push
//i nodi nella FIFO sono finiti, e sono contenuti in node_list
//se un nodo e' nella FIFO vuol dire che il corrispondente thread e' idle validator
typedef struct{
    int first_node;
    int last_node;
    Node *node_list;
} FIFO;

int problem_solved;//intero che se diverso da zero indica che un sudoku risolto e' stato trovato
Sudoku *sudokus_to_solve;//lista di sudoku da validare, uno per ogni thread. l'idle validator thread, prende da qui il sudoku da validare
sem_t *semaphores;//lista di semafori, uno per ogni thread. gli idle validator threads attendono sul proprio semaforo in attesa di diventare active validator threads. saranno i delegator threads a svegiare gli idle validator threads
pthread_mutex_t mutex_shared_data;//mutex per accedere in mutua esclusione alla FIFO
FIFO shared_fifo_idle_threads;//FIFO condivisa tra i vari thread
pthread_mutex_t mutex_solved_sudoku;//mutex per accedere in mutua esclusione alla scrittura del sudoku che conterra' una grid valida
Sudoku solved_sudoku;//sudoku che conterra' una grid valida

void FIFO_PUSH(FIFO *fifo, int index);//funzione di push della FIFO
int FIFO_POP(FIFO *fifo);//funzione di pop della FIFO
void compile_sudoku(char *str, Sudoku *sudoku);//funzione che compila una struttura sudoku tramite la stringa data in input
void print_sudoku(Sudoku *sudoku);//funzione che stampa la grid di un sudoku dato in argomento
int check_row_column(int value, int row, int column, Sudoku *sudoku);//funzione che controlla se value e' un valore valido nelle coordinate date in argomento nel sudoku dato in argomento. controllo effettuato su riga e colonna
int check_subgrid(int value, int row, int column, Sudoku *sudoku);//funzione che controlla se value e' un valore valido nelle coordinate date in argomento nel sudoku dato in argomento. controllo effettauato sulla subgrid
void solve_sudoku(int threads_number, pthread_t *threads, Sudoku *sudoku);//funzione delegata all risoluzione del sudoku passato per argomento tramite l'utilizzo di un numero di thread passato per argomento
void *thread_function(void *arg);//funzione che esegue ogni thread al lancio
int thread_subfunction(Sudoku *sudoku);//sottofunzione che esegue ogni thread
int tn;

void FIFO_PUSH(FIFO *fifo, int index){
    fifo->node_list[index].next_node = -1;
    if(fifo->first_node == -1 || fifo->last_node == -1){
        fifo->node_list[index].previous_node = -1;
        fifo->first_node = index;
    }
    else{
        fifo->node_list[index].previous_node = fifo->last_node;
        fifo->node_list[fifo->last_node].next_node = index;
    }
    fifo->last_node = index;
}

int FIFO_POP(FIFO *fifo){
    if(fifo->first_node == -1 || fifo->last_node == -1) return -1;
    int index = fifo->first_node;
    fifo->first_node = fifo->node_list[index].next_node;
    return index;
}

void compile_sudoku(char *str, Sudoku *sudoku){
    int i;
    sudoku->index_next_empty_cell = -1;
    for(int row = 0; row < SUDOKU_SIZE; row++){
        for(int column = 0; column < SUDOKU_SIZE; column++){
            i = (int) str[SUDOKU_SIZE*row+column] - '0';
            Coordinate coord;
            coord.row = row;
            coord.column = column;
            if (i == 0) sudoku->empty_cells[sudoku->index_next_empty_cell++] = coord;
            if (i >= 0 && i <= 9) sudoku->grid[row][column] = i;
        }
    }
}

void print_sudoku(Sudoku *sudoku){
    printf("\t+-------+-------+-------+\n");
    for(int row = 0; row < SUDOKU_SIZE; row++){
        printf("\t| ");
        for(int column = 0; column < SUDOKU_SIZE; column++){
            printf("%d ",sudoku->grid[row][column]);
            if((column+1)%SUDOKU_SQRT==0)
                printf("| ");
            if((SUDOKU_SIZE*row+column+1)%SUDOKU_SIZE==0)
                printf("\n");
        }
        if((row+1)%SUDOKU_SQRT==0)
            printf("\t+-------+-------+-------+\n");
    }
}

int check_row_column(int value, int row, int column, Sudoku *sudoku){
    for(int i=0;i<SUDOKU_SIZE;i++){
        if(sudoku->grid[row][i] == value || sudoku->grid[i][column] == value) return 1;
    }
    return 0;
}

int check_subgrid(int value, int row, int column, Sudoku *sudoku){
    int row_start = (row/SUDOKU_SQRT)*SUDOKU_SQRT;
    int col_start = (column/SUDOKU_SQRT)*SUDOKU_SQRT;
    for(int row = row_start; row < row_start+SUDOKU_SQRT; row++){
        for(int column = col_start; column < col_start+SUDOKU_SQRT; column++){
            if(sudoku->grid[row][column] == value) return 1;
        }
    }
    return 0;
}

void solve_sudoku(int threads_number, pthread_t *threads, Sudoku *sudoku){
    //inizializzo le variabili precedentemente dichiarate
    problem_solved = 0;
    sudokus_to_solve = (Sudoku *) malloc(threads_number*sizeof(Sudoku));
    semaphores = (sem_t *) malloc(threads_number*sizeof(sem_t));
    pthread_mutex_init(&mutex_shared_data, NULL);
    Node *node_list = (Node *) malloc(threads_number*sizeof(Node));
    shared_fifo_idle_threads.first_node = -1;
    shared_fifo_idle_threads.last_node = -1;
    shared_fifo_idle_threads.node_list = node_list;
    pthread_mutex_init(&mutex_solved_sudoku, NULL);
    //lancio ogni thread nel modo opportuno
    for(long i = 0; i < threads_number; i++){
        sem_init(&semaphores[i], 0, 0);//setto il semaforo del thread a 0, cosi' che sia idle validator
        //creo il nodo che corrispondera' al thread in questione e lo inserisco della FIFO
        Node node;
        shared_fifo_idle_threads.node_list[i] = node;
        FIFO_PUSH(&shared_fifo_idle_threads, i);
        pthread_create(&threads[i], NULL, thread_function, (void *) i);//lancio il thread passandogli il suo identificativo
    }
    //ora tutti i thread sono stati lanciati come idle validator threads
    int first_thrade_index = FIFO_POP(&shared_fifo_idle_threads);//estrago un thread dalla FIFO
    sudokus_to_solve[first_thrade_index] = *sudoku;//scrivo nel suo sudoku da validare il sudoku di partenza
    sem_post(&semaphores[first_thrade_index]);//sveglio il seguente thread, cosi' che diventi active validator
    //faccio la join di ogni thread e distruggo i semafori dei thread
    for(int i = 0; i < threads_number; i++){
        pthread_join(threads[i], NULL);
        sem_destroy(&semaphores[i]);
    }
    //distruggo i mutex e libero la memoria precedentemente allocata
    free(sudokus_to_solve);
    free(semaphores);
    pthread_mutex_destroy(&mutex_shared_data);
    free(shared_fifo_idle_threads.node_list);
    pthread_mutex_destroy(&mutex_solved_sudoku);
    //scrivo nel sudoku di partenza il contenuto del sudoku valido che e' stato trovato
    *sudoku = solved_sudoku;
}

void *thread_function(void *arg){
    //il thread e' idle validator
    long my_index = (long) arg;
    while(1){
        if(problem_solved > 0) break;
        sem_wait(&semaphores[my_index]);
        //il thread e' active validator
        if(problem_solved > 0) break;
        if(thread_subfunction(&sudokus_to_solve[my_index]) == 1) break;
        //il thread si aggiunge nella FIFO
        pthread_mutex_lock(&mutex_shared_data);
        FIFO_PUSH(&shared_fifo_idle_threads, my_index);
        pthread_mutex_unlock(&mutex_shared_data);
        //il thread e' idle validator
    }
    return NULL;
}

int thread_subfunction(Sudoku *sudoku){
    if(problem_solved > 0) return 1;
    //se il sudoku da validare non ha piu' celle vuote significa che e' valido, cosi' possiamo compilare solved_sudoku e notificare che si e' trovata una soluzione
    if(sudoku->index_next_empty_cell == -1){
        problem_solved++;
        pthread_mutex_lock(&mutex_solved_sudoku);
        solved_sudoku = *sudoku;
        pthread_mutex_unlock(&mutex_solved_sudoku);
        for(int i = 0; i < tn; i++) sem_post(&semaphores[i]);
        return 1;
    }
    //ottengo le coordinate di una cella vuota
    int row = sudoku->empty_cells[sudoku->index_next_empty_cell].row;
    int column = sudoku->empty_cells[sudoku->index_next_empty_cell].column;
    sudoku->index_next_empty_cell--;
    //provo ad inserire nella cella i 9 numeri possibili
    for(int i = 1; i <= 9; i++){
        if(problem_solved > 0) return 1;
        //se il numero inserito nella cella risulta valido svolgo il seguente blocco di istruzioni
        if(check_row_column(i, row, column, sudoku) == 0 && check_subgrid(i, row, column, sudoku) == 0){
            sudoku->grid[row][column] = i;//inserisco il numero valido nella cella vuota
            //provo a leggere la FIFO, se ci riesco svolgo il seguente blocco di istruzioni
            if(pthread_mutex_trylock(&mutex_shared_data) == 0){
                //il thread e' delegator
                int assistant_index = FIFO_POP(&shared_fifo_idle_threads);//estreggo un nodo dalla FIFO
                pthread_mutex_unlock(&mutex_shared_data);
                //se il nodo estratto corrisoponde ad un thread allora svolgo il seguente blocco di istruzioni
                if(assistant_index != -1){
                    sudokus_to_solve[assistant_index] = (Sudoku) *sudoku;
                    sem_post(&semaphores[assistant_index]);
                    continue;
                }
                //il thread e' active validator
            }
            //arrivati a questo punto il thread deve validare il sudoku con un numero valido nella cella vuota precedentemente riempita
            if(thread_subfunction(sudoku) == 1){
                return 1;
            }
        }
    }
    //arrivati a questo punto, il numero valido nella cella vuota precedentemente riempita, porta ad un sudoku non valido, quindi la cella riempita torna ad essere vuota
    sudoku->grid[row][column] = 0;
    sudoku->index_next_empty_cell++;
    return 0;
}

int main(int argc, char **argv){
    int threads_number = atoi(argv[1]);
    tn = threads_number;
    FILE *file_p = fopen(argv[2], "rb");
    char str[SUDOKU_SIZE*SUDOKU_SIZE+2];
    Sudoku sudoku;
    time_t start_time, end_time;
    pthread_t threads[threads_number];
    while(fgets(str, SUDOKU_SIZE*SUDOKU_SIZE+2, file_p) != NULL){
        start_time = clock();
        compile_sudoku(str, &sudoku);
        print_sudoku(&sudoku);
        solve_sudoku(threads_number, threads, &sudoku);
        print_sudoku(&sudoku);
        end_time = clock();
        printf("\t%f seconds\n\n\n", (double)(end_time-start_time)/CLOCKS_PER_SEC);
    }
    return 0;
}
 
