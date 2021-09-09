#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>

#define MAX_THREADS 1024
#define SUDOKU_SIZE 9
#define SUDOKU_SQRT 3

typedef struct{
    int row;
    int column;
} Coordinate;

typedef struct{
    int grid[SUDOKU_SIZE][SUDOKU_SIZE];
    Coordinate empty_cells[SUDOKU_SIZE*SUDOKU_SIZE];
    int index_next_empty_cell;
} Sudoku;

typedef struct{
    int previous_node;
    int next_node;
} Node;

typedef struct{
    int first_node;
    int last_node;
    Node *node_list;
} FIFO;

int max_delegations;
int problem_solved;
Sudoku *sudokus_to_solve;
sem_t *semaphores;
pthread_mutex_t mutex_shared_data;
FIFO shared_fifo_idle_threads;
pthread_mutex_t mutex_solved_sudoku;
Sudoku solved_sudoku;
sem_t end;

void print_FIFO(FIFO *fifo);
void FIFO_PUSH(FIFO *fifo, int index);
int FIFO_POP(FIFO *fifo);
void compile_sudoku(char *str, Sudoku *sudoku);
void print_sudoku(Sudoku *sudoku);
int check_row_column(int value, int row, int column, Sudoku *sudoku);
int check_subgrid(int value, int row, int column, Sudoku *sudoku);
void solve_sudoku(int threads_number, pthread_t *threads, Sudoku *sudoku);
void *thread_function(void *arg);
int thread_subfunction(Sudoku *sudoku, int my_index);
int tn;

void print_FIFO(FIFO *fifo){
    int node = fifo->first_node;
    while(node != -1){
        printf("%d ", node);
        node = fifo->node_list[node].next_node;
    }
    printf("\n");
}
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
            if (i == 0){
                sudoku->index_next_empty_cell++;
                sudoku->empty_cells[sudoku->index_next_empty_cell] = coord;
            }
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
    problem_solved = 0;
    sudokus_to_solve = (Sudoku *) malloc(threads_number*sizeof(Sudoku));
    semaphores = (sem_t *) malloc(threads_number*sizeof(sem_t));
    pthread_mutex_init(&mutex_shared_data, NULL);
    Node *node_list = (Node *) malloc(threads_number*sizeof(Node));
    shared_fifo_idle_threads.first_node = -1;
    shared_fifo_idle_threads.last_node = -1;
    shared_fifo_idle_threads.node_list = node_list;
    pthread_mutex_init(&mutex_solved_sudoku, NULL);
    sem_init(&end, 0, 0);
    for(long i = 0; i < threads_number; i++){
        sem_init(&semaphores[i], 1, 0);
        Node node;
        shared_fifo_idle_threads.node_list[i] = node;
        FIFO_PUSH(&shared_fifo_idle_threads, i);
        pthread_create(&threads[i], NULL, thread_function, (void *) i);
    }
    int first_thrade_index = FIFO_POP(&shared_fifo_idle_threads);
    sudokus_to_solve[first_thrade_index] = *sudoku;
    sem_post(&semaphores[first_thrade_index]);
    sem_wait(&end);
    for(int i = 0; i < threads_number; i++) sem_post(&semaphores[i]);
    for(int i = 0; i < threads_number; i++){
        pthread_join(threads[i], NULL);
        sem_destroy(&semaphores[i]);
    }
    free(sudokus_to_solve);
    free(semaphores);
    pthread_mutex_destroy(&mutex_shared_data);
    free(shared_fifo_idle_threads.node_list);
    pthread_mutex_destroy(&mutex_solved_sudoku);
    *sudoku = solved_sudoku;
    sem_destroy(&end);
}

void *thread_function(void *arg){
    long my_index = (long) arg;
    while(1){
        if(problem_solved > 0) break;
        sem_wait(&semaphores[my_index]);
        if(problem_solved > 0) break;
        if(thread_subfunction(&sudokus_to_solve[my_index], my_index) == 1) break;
        pthread_mutex_lock(&mutex_shared_data);
        FIFO_PUSH(&shared_fifo_idle_threads, my_index);
        //print_FIFO(&shared_fifo_idle_threads);
        pthread_mutex_unlock(&mutex_shared_data);
    }
    return NULL;
}

int thread_subfunction(Sudoku *sudoku, int my_index){
    if(problem_solved > 0) return 1;
    if(sudoku->index_next_empty_cell == -1){
        problem_solved++;
        sem_post(&end);
        pthread_mutex_lock(&mutex_solved_sudoku);
        solved_sudoku = *sudoku;
        pthread_mutex_unlock(&mutex_shared_data);
        for(int i = 0; i < tn; i++) sem_post(&semaphores[i]);
        return 1;
    }
    int row = sudoku->empty_cells[sudoku->index_next_empty_cell].row;
    int column = sudoku->empty_cells[sudoku->index_next_empty_cell].column;
    sudoku->index_next_empty_cell--;
    int i = 1;
    int ok_i = 0;
    while(i >= 1 && i <= 9){
        if(problem_solved > 0) return 1;
        if(check_row_column(i, row, column, sudoku) == 0 && check_subgrid(i, row, column, sudoku) == 0){
            ok_i = 1;
            if(pthread_mutex_trylock(&mutex_shared_data) == 0){
                //print_FIFO(&shared_fifo_idle_threads);
                int delegations_counter = 0;
                while(delegations_counter < max_delegations && i >= 1 && i <= 9){
                    if(ok_i == 0){
                        if(check_row_column(i, row, column, sudoku) == 0 && check_subgrid(i, row, column, sudoku) == 0) ok_i = 1;
                        else{
                            i++;
                            ok_i = 0;
                            continue;
                        }
                    }
                    if(ok_i == 1){
                        int assistant_index = FIFO_POP(&shared_fifo_idle_threads);
                        if(assistant_index != -1){
                            sudokus_to_solve[assistant_index] = (Sudoku) *sudoku;
                            sudokus_to_solve[assistant_index].grid[row][column] = i;
                            sem_post(&semaphores[assistant_index]);
                            delegations_counter++;
                            i++;
                            ok_i = 0;
                            //print_FIFO(&shared_fifo_idle_threads);
                        }
                        else{
                            break;
                        }
                        
                    }
                }
                pthread_mutex_unlock(&mutex_shared_data);
            }
            if(i >= 1 && i <= 9 && ok_i == 1){
                sudoku->grid[row][column] = i;
                if(thread_subfunction(sudoku, my_index) == 1){

                    return 1;
                }
            }
        }
        i++;
        ok_i = 0;
    }
    sudoku->grid[row][column] = 0;
    sudoku->index_next_empty_cell++;
    return 0;
}

/*
int thread_subfunction(Sudoku *sudoku, int my_index){
    if(problem_solved > 0) return 1;
    if(sudoku->index_next_empty_cell == -1){
        problem_solved++;
        pthread_mutex_lock(&mutex_solved_sudoku);
        solved_sudoku = *sudoku;
        pthread_mutex_unlock(&mutex_solved_sudoku);
        return 1;
    }
    int row = sudoku->empty_cells[sudoku->index_next_empty_cell].row;
    int column = sudoku->empty_cells[sudoku->index_next_empty_cell].column;
    sudoku->index_next_empty_cell--;
    for(int i = 1; i <= 9; i++){
        if(problem_solved > 0) return 1;
        if(check_row_column(i, row, column, sudoku) == 0 && check_subgrid(i, row, column, sudoku) == 0){
            sudoku->grid[row][column] = i;

            if(pthread_mutex_trylock(&mutex_shared_data) == 0){
                int assistant_index = FIFO_POP(&shared_fifo_idle_threads);
                pthread_mutex_unlock(&mutex_shared_data);
                if(assistant_index != -1){
                    sudokus_to_solve[assistant_index] = (Sudoku) *sudoku;
                    sem_post(&semaphores[assistant_index]);
                    continue;
                }
            }
            if(thread_subfunction(sudoku, my_index) == 1){
                return 1;
            }
        }
    }
    sudoku->grid[row][column] = 0;
    sudoku->index_next_empty_cell++;
    return 0;
}
*/

int main(int argc, char **argv){
    int threads_number = atoi(argv[1]);
    int tn= threads_number;
    FILE *file_p = fopen(argv[2], "rb");
    max_delegations = atoi(argv[3]);
    if(threads_number <= 0 || max_delegations < 0 || (max_delegations == 0 && threads_number != 1) || max_delegations > threads_number-1 || max_delegations > 9) return 0;
    char str[SUDOKU_SIZE*SUDOKU_SIZE+2];
    Sudoku sudoku;
    time_t start_time, end_time;
    pthread_t threads[threads_number];
    int count = 0;
    while(fgets(str, SUDOKU_SIZE*SUDOKU_SIZE+2, file_p) != NULL){
        start_time = clock();
        compile_sudoku(str, &sudoku);
        print_sudoku(&sudoku);
        solve_sudoku(threads_number, threads, &sudoku);
        print_sudoku(&sudoku);
        end_time = clock();
        printf("\t%f seconds\n\n\n", (double)(end_time-start_time)/CLOCKS_PER_SEC);
        count++;
    }
    return 0;
}
