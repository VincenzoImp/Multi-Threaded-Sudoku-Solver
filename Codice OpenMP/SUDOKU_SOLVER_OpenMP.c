#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <omp.h>

//istruzione da inserire per compilare il file c con openMP
//gcc -g -Wall -fopenmp -o SUDOKU_SOLVER_OpenMP SUDOKU_SOLVER_OpenMP.c 

#define SUDOKU_SIZE 9
#define SUDOKU_SQRT 3

typedef struct{  //STRUTTURA CHE DEFINISCE UNA COORDINATA (RIGA E COLONNA)
    int row;
    int column;
} Coordinate;

typedef struct{  //STRUTTURA PER I SUDOKU
    int grid[SUDOKU_SIZE][SUDOKU_SIZE];    //griglia del sudoku
    Coordinate empty_cells[SUDOKU_SIZE*SUDOKU_SIZE];  //coordinate delle celle vuote del sudoku
    int index_next_empty_cell;  //indice di quante celle vuote sono rimaste nel sudoku
} Sudoku;


///////////////////FUNZIONI 
void compile_sudoku(char *str, Sudoku *sudoku);
void print_sudoku(Sudoku *sudoku);
int check_row_column(int value, int row, int column, Sudoku *sudoku);
int check_subgrid(int value, int row, int column, Sudoku *sudoku);
void solve_sudoku(int threads_number, Sudoku *sudoku);

//NUMERO DI THREAD
int threads_number;

//QUESTA FUNZIONE PRENDE 81 NUMERI E RIEMPIE LA GRIGLIA DEL SUDOKU INIZIALE
void compile_sudoku(char *str, Sudoku *sudoku){
    int i;
    sudoku->index_next_empty_cell = -1; //inizializzazione celle vuote
    for(int row = 0; row < SUDOKU_SIZE; row++){
        for(int column = 0; column < SUDOKU_SIZE; column++){
            i = (int) str[SUDOKU_SIZE*row+column] - '0';
            Coordinate coord;
            coord.row = row;
            coord.column = column;
            if (i == 0){ //se le cella contiene uno zero la mettiamo nella griglia e la contiamo come cella vuota
                sudoku->index_next_empty_cell++;
                sudoku->empty_cells[sudoku->index_next_empty_cell] = coord;
            }
            if (i >= 0 && i <= 9) sudoku->grid[row][column] = i; //se le cella contiene un numero tra 1 e 9 riempie la griglia
        }
    }
}


//QUESTA FUNZIONE STAMPA IL SUDOKU IN 2D
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


//QUESTA FUNZIONE SERVE PER CONTROLLARE SE UN SUDOKU PUÓ INSERIRE
//UN CERTO NUMERO IN UNA CERTA CELLA CONTROLLANDO RIGHE E COLONNE
int check_row_column(int value, int row, int column, Sudoku *sudoku){
    for(int i=0;i<SUDOKU_SIZE;i++){
        if(sudoku->grid[row][i] == value || sudoku->grid[i][column] == value) { 
        return 1;
        }
    }
    return 0;
}



//QUESTA FUNZIONE SERVE PER CONTROLLARE SE UN SUDOKU PUÓ INSERIRE UN CERTO
//NUMERO IN UNA CERTA CELLA CONTROLLANDO IL QUADRATO 3X3 IN CUI SI TROVA
int check_subgrid(int value, int row, int column, Sudoku *sudoku){
    int row_start = (row/SUDOKU_SQRT)*SUDOKU_SQRT;
    int col_start = (column/SUDOKU_SQRT)*SUDOKU_SQRT;
    for(int row = row_start; row < row_start+SUDOKU_SQRT; row++){
        for(int column = col_start; column < col_start+SUDOKU_SQRT; column++){
            if(sudoku->grid[row][column] == value)  {
                return 1;
            }
        }
    }
    return 0;
}


//FUNZIONE CHE PRESO UN SUDOKU COMPILATO LO RISOLVE MODIFICANDO IL SUDOKU INIZIALE
void solve_sudoku(int threads_number, Sudoku *original_sudoku){
    int solved_sudoku = 0; //indice che avverte se il sudoku è stato risolto (0=da risolvere, 1=risolto)
    int index_list = 1; //indice della lista iniziale che conterrà solo il sudoku iniziale
    Sudoku *list = (Sudoku *) malloc(index_list*sizeof(Sudoku)); //allocazione lista iniziale
    list[index_list-1] = *original_sudoku; // si inserisce nella lista iniziale il sudoku da compilare
	
/////////////////////////INIZIO DEL WHILE
    while(index_list > 0){ //se nella lista c'è almeno un sudoku
 
        if(solved_sudoku > 0) break; //se il sudoku è già risolto esci
        int new_index_list = 0; //indice della nuova lista che conterrà tutti i sudoku per il nuovo livello di computazione
        Sudoku *new_list = (Sudoku *) malloc((new_index_list+1)*sizeof(Sudoku)); //allocazione nuova lista
		
//////////////////////////INIZIO DEL FOR
        #pragma omp parallel for num_threads(threads_number) collapse(2) //costrutto openMP per parallelizzare il codice
        for(int x = 0; x < index_list; x++){ //scorre sugli elementi della lista
            for(int i = 1; i <= 9; i++){ //scorre sui numeri da inserire nel sudoku (da 1 a 9)
                Sudoku sudoku = list[x]; //prende il sudoku dalla lista che si trova all'indice x
                int row = sudoku.empty_cells[sudoku.index_next_empty_cell].row; //si prende la riga della cella vuota 
                int column = sudoku.empty_cells[sudoku.index_next_empty_cell].column; //si prende la colonna della cella vuota 
                if(check_row_column(i, row, column, &sudoku) == 0 && check_subgrid(i, row, column, &sudoku) == 0){ 
				//controlla se un certo numero da 1 a 9 si può inserire in quella cella vuota
                    sudoku.grid[row][column] = i; //se si può lo inserisce
                    sudoku.index_next_empty_cell--; //si diminuisce il numero di celle vuote
                    if(sudoku.index_next_empty_cell == -1){ //se non ci sono più celle vuote
                        #pragma omp critical //costrutto openMP per accedere ad una sezione critica
                        {
                            solved_sudoku = 1; //il sudoku è risolto
                            *original_sudoku = sudoku; //si modifica il sudoku iniziale con le celle opportunamente riempite
                        }
                    }
                    else{ //se ci sono ancora celle vuote
                        #pragma omp critical //costrutto openMP per accedere ad una sezione critica
                        {
                            new_list[new_index_list] = sudoku; //si inserisce nella nuova lista il nuovo sudoku col numero inserito
                            new_index_list++; //si aumenta l'indice della nuova lista
                            new_list = (Sudoku *) realloc(new_list, (new_index_list+1)*sizeof(Sudoku)); 
							//si rialloca lo spazio nella nuova lista per permettere l'inserimento di nuovi sudoku
                        }
                    }
                }
            }
        }
		
////////////////////////////////FINE DEL FOR
        free(list); //si libera dalla memoria lo spazio allocato alla prima lista
        index_list = new_index_list; //l'indice della lista prende il vecchio indice per sapere quanti sudoku c'erano al livello di computazione precedente
        list = new_list; //si sostituisce la vecchia lista con quella nuova (passo avanti nella computazione)
    }
	
/////////////////////////////FINE DEL WHILE
    free(list);
}


//MAIN DEL CODICE
int main(int argc, char **argv){
    threads_number = atoi(argv[1]); //prende il numero di thread a cui far eseguire il codice in input
    FILE *file_p = fopen(argv[2], "rb"); //prende dal file in input un sudoku
    char str[SUDOKU_SIZE*SUDOKU_SIZE+2];
    Sudoku sudoku;
    time_t start_time, end_time; 
    while(fgets(str, SUDOKU_SIZE*SUDOKU_SIZE+2, file_p) != NULL){
        start_time = clock();
        compile_sudoku(str, &sudoku); //compila la griglia del sudoku dal sudoku ricevuto in input
        print_sudoku(&sudoku); //stampa il sudoku da risolvere
        solve_sudoku(threads_number, &sudoku); //funzione che risolve il sudoku
        print_sudoku(&sudoku); //stampa il sudoku risolto
        end_time = clock();
        printf("%f seconds\n", (double)(end_time-start_time)/CLOCKS_PER_SEC); //stampa il tempo che ci ha messo per risolvere il sudoku
    }
    return 0;
}

 
