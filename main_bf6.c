#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#define NOB_IMPLEMENTATION
#include "nob.h"

#define u8 uint8_t
#define u64 uint64_t

#define MAX_TAPE_SIZE 256
#define MAX_INST_COUNT 25600

typedef struct {
    u8 tape[MAX_TAPE_SIZE];
    size_t ex_number;
} Program;

typedef struct {
    Program *items;
    size_t count;
    size_t capacity;
} Programs;

typedef enum {
    O,
    MRL,
    MRR,
    MWL,
    MWR,
    MIL,
    MIR,
    S,
    WP,
    WE,
    WM,
    COUNT
} BF6;

static_assert(COUNT == 11, "Amount of instructions have changed");

const char *ins_bf6[COUNT] = {
    [O]   = "o",
    [MRL] = "<",
    [MRR] = ">",
    [MWL] = "{",
    [MWR] = "}",
    [MIL] = "l",
    [MIR] = "r",
    [S]   = "s",
    [WP]  = "p",
    [WE]  = "w",
    [WM]  = "m",
};

typedef enum {
    BFL1,
    BFL2,
    BFL3,
    BFL4,
    BFL5,
    BFL6,
    
    BFL_COUNT
} BFL;

const char *_bfl_str[BFL_COUNT] = {
    [BFL1] = "bf1",
    [BFL2] = "bf2",
    [BFL3] = "bf3",
    [BFL4] = "bf4",
    [BFL5] = "bf5",
    [BFL6] = "bf6",
};

Program *generate_random_program(Programs *programs) {
    Program program = {0};
    program.ex_number = 0;
    
    for(int i = 0; i < MAX_TAPE_SIZE; i++) {
        program.tape[i] = rand() % COUNT;
    }
    nob_da_append(programs, program);
    return &programs->items[programs->count - 1];
}

void print_program(Program *program) {
    if (program == NULL) return;
    
    for (int i = 0; i < MAX_TAPE_SIZE; i++) {
        unsigned char instruction = program->tape[i] % COUNT;
        printf("%s", ins_bf6[instruction]);
    }
    printf("\n");
}

void print_program_u8(Program *program) {
    if (program == NULL) return;
    
    for (int i = 0; i < MAX_TAPE_SIZE; i++) {
        printf("%d,", program->tape[i]);
    }
    printf("\n");
}

void print_programs(Programs list) {
    for (size_t i = 0; i < list.count; ++i) {
        print_program(&list.items[i]);
    }
}



/*
Hash Table implementation
*/

typedef struct {
    size_t program_index;
    bool occupied;
} PKV;

typedef struct {
    PKV *items;
    size_t count;
    size_t capacity;
}PKVs;

#define hash_init(ht, cap) \
    do { \
            (ht)->items = malloc(sizeof(*(ht)->items)*(cap)); \
            if ((ht)->items) { \
                memset((ht)->items, 0, sizeof((*(ht)->items))*(cap)); \
                (ht)->capacity = (cap); \
                (ht)->count = 0; \
            } else { \
                nob_log(NOB_ERROR, "Failed to allocate new hash table!"); \
            } \
    } while(0)
    
uint64_t hash(u8 *buf, size_t buf_size) {
    u64 hash = 5381;
    for( size_t i = 0; i < buf_size; ++i) {
        hash = ((hash << 5) + hash) + (u64)buf[i];
    }
    return hash;
}

bool tape_eq(u8 *a, u8* b) {
    if (a == NULL || b == NULL) {
            nob_log(NOB_ERROR, "One of the tape pointers is NULL!");
            return false;
        }
    
    bool cond = memcmp(a, b, MAX_TAPE_SIZE) == 0;
    return cond;    
}

// for sorting da
int compare_ex_nr(const void *a, const void *b) {
    const Program *ap = a;
    const Program *bp = b;
    return (int)ap->ex_number - (int)bp->ex_number;
}

size_t add_to_hash(PKVs *ht,Programs *programs, size_t program_index) {
    Program *p = &programs->items[program_index];
    u64 h = hash((u8*)p->tape, MAX_TAPE_SIZE)%ht->capacity;
    
    for (size_t i = 0; i < ht->capacity && ht->items[h].occupied && !tape_eq(programs->items[ht->items[h].program_index].tape, p->tape); ++i){
        h = (h+1)%ht->capacity;
    }
    if (ht->items[h].occupied) {
        if(!(tape_eq(programs->items[ht->items[h].program_index].tape, p->tape))) {
            nob_log(NOB_ERROR, "Table overflow, increase table slot number!");
            return 0;
        }
        size_t cycle_number = p->ex_number - programs->items[ht->items[h].program_index].ex_number;
        // nob_log(NOB_INFO,"Cycle detected with size: %zu, after %zu program executions", cycle_number, program->ex_number);
        return cycle_number; 
    } else {
        ht->items[h].occupied = true;
        ht->items[h].program_index =  program_index;
        ht->count++;
    }
    return 0;
}

#define MAX_EX_NUMBER 1000000
#define DO_SEARCH 100000000

bool write_programs_to_file(Programs *programs, size_t ex_number, size_t cycle_number, BFL bf6) {
    char dir_path[200];
        snprintf(dir_path, sizeof(dir_path), "./%s_programs", _bfl_str[bf6]);
        if (!nob_mkdir_if_not_exists(dir_path)) {
            nob_log(NOB_ERROR, "Could not create directory %s", dir_path);
            return false;
        }
    
    char file_path[200];
    FILE *file = NULL;
    int discriminator = 0;
    
    do {
        if (discriminator == 0) {
            snprintf(file_path, sizeof(file_path), "%s/%zu-%zu.txt", 
                    dir_path, cycle_number, ex_number);
        } else {
            snprintf(file_path, sizeof(file_path), "%s/%zu-%zu_%d.txt", 
                    dir_path, cycle_number, ex_number, discriminator);
        }
        
        // Try to open file in read mode to check if it exists
        FILE *test = fopen(file_path, "r");
        if (test == NULL) {
            // File doesn't exist, we can use this name
            file = fopen(file_path, "w");
            break;
        }
        fclose(test);
        discriminator++;
    } while (discriminator < 1000); // Reasonable upper limit to prevent infinite loops

    if (file == NULL) {
        nob_log(NOB_ERROR, "Could not create unique file name or open file %s", file_path);
        return false;
    }
    for (size_t i = 0; i < programs->count; ++i) {
        Program *program = &programs->items[i];
        for (int j = 0; j < MAX_TAPE_SIZE; j++) {
            unsigned char instruction = program->tape[j] % COUNT;
            fprintf(file, "%s", ins_bf6[instruction]);
        }
        fprintf(file, "\n");
    }
    fclose(file);
    return true;
}

bool flag_int(int *argc, char ***argv, size_t *value)
{
    const char *flag = nob_shift(*argv, *argc);
    if ((*argc) <= 0) {
        nob_log(NOB_ERROR, "No argument is provided for %s", flag);
        return false;
    }
    *value = (size_t)atoi(nob_shift(*argv, *argc));
    return true;
}

Program *evaluate_bf6(Programs *programs, Program *source) {
    if (source == NULL) {
        return NULL;
    }
    size_t read_head = 0;   // Head for reading from source tape
    size_t write_head = 0;  // Head for writing to result tape
    size_t ins_head = 0;    // Instruction pointer for source program
    
    size_t ins_count = 0;
    
    Program result = {0};
    result.ex_number = source->ex_number + 1;
    memcpy(result.tape, source->tape, MAX_TAPE_SIZE * sizeof(u8));
    while (ins_count < MAX_INST_COUNT) {
        if (ins_head >= MAX_TAPE_SIZE) break;
        BF6 instruction = result.tape[ins_head];
        if (instruction >= COUNT ){
            nob_log(NOB_ERROR, "IMPOSSIBLE INSTRUCTION %d at %d", instruction, ins_head);
            nob_log(NOB_ERROR, "%d%d%d", result.tape[ins_head-1],result.tape[ins_head],result.tape[ins_head+1] );
            nob_log(NOB_ERROR, "%d%d%d", source->tape[ins_head-1],source->tape[ins_head],source->tape[ins_head+1] );
            print_program_u8(source);
            print_program_u8(&result);
            exit(1);
        }

        switch (instruction) {
            case O:{
                ins_head = (ins_head + 1);
                break;
            }                            
            case MRL:{
                read_head = (read_head + MAX_TAPE_SIZE - 1) % MAX_TAPE_SIZE;    
                ins_head = (ins_head + 1);
                break;
            }
            case MRR: {
                read_head = (read_head + 1) % MAX_TAPE_SIZE;    
                ins_head = (ins_head + 1);
                break;
            }                                
            case MWL:{
                write_head = (write_head + MAX_TAPE_SIZE - 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1);
                break;
            }                               
            case MWR:{
                write_head = (write_head + 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1);
                break; 
            }                             
            case MIL:{
                if (result.tape[read_head] != 0) {
                    int bracket_count = 1;
                    while (bracket_count > 0 && ins_head < MAX_TAPE_SIZE && ins_head > 0) {
                        ins_head--;
                        if (result.tape[ins_head] == MIL) bracket_count++;
                        if (result.tape[ins_head] == MIR) bracket_count--;
                    }
                } else {
                    ins_head = (ins_head + 1);
                }
                break;
            }
            case MIR:{
                if (result.tape[read_head] == 0) {
                    int bracket_count = 1;
                    while (bracket_count > 0 && ins_head < MAX_TAPE_SIZE && ins_head > 0) {
                        ins_head++;
                        if (result.tape[ins_head] == MIR) bracket_count++;
                        if (result.tape[ins_head] == MIL) bracket_count--;
                    }
                } else {
                    ins_head = (ins_head + 1);
                }
                break;
            }
            case S:{
                size_t temp = read_head;
                read_head = write_head;
                write_head = temp;
                ins_head = (ins_head + 1);
                break;
            }            
            case WP:{
                result.tape[write_head] = (result.tape[read_head] + 1) % COUNT;
                ins_head = (ins_head + 1);
                break; 
            }  
            case WE: {
                result.tape[write_head] = result.tape[read_head];
                ins_head = (ins_head + 1);
                break;
            }                
            case WM:{
                result.tape[write_head] = (result.tape[read_head] + COUNT - 1) % COUNT;
                ins_head = (ins_head + 1);
                break; 
            }           
            default:{
                ins_head = (ins_head + 1);
                nob_log(NOB_INFO, "WARNING: skipping unkown instruction: %s!", instruction);
                break;
            }            
        }
        ins_count++;
        if(ins_head > MAX_TAPE_SIZE) break;
    }
    nob_da_append(programs, result);
    return &programs->items[programs->count-1];
}



int main(int argc, char **argv) {
    
    const char *program_name = nob_shift(argv, argc);
    
    srand(time(NULL));
    size_t do_search = DO_SEARCH;
    size_t cycle_number = 0;
    size_t highest_cycle_number = 0;
    size_t highest_execution_number = 1;
    size_t bfl = 6;
    Program* (*evaluate)(Programs *, Program *) = evaluate_bf6;
    
    while (argc > 0) {
        const char *flag = argv[0];
        if (strcmp(flag, "-e") == 0) {
            if (!flag_int(&argc, &argv, &do_search)) return 1;
        }
        else if (strcmp(flag, "-hc") == 0) {
            if (!flag_int(&argc, &argv, &highest_cycle_number)) return 1;
        }
        else if (strcmp(flag, "-he") == 0) {
            if (!flag_int(&argc, &argv, &highest_execution_number)) return 1;
        } else {
            break;
        }
    }
        
    while (do_search) {
        if( do_search % 10000== 0){
           nob_log(NOB_INFO, "experiments: %zu", DO_SEARCH - do_search); 
        }
        Programs programs = {0};
        PKVs ht = {0};
        hash_init(&ht, MAX_EX_NUMBER);
        size_t ex_number = 0;
        Program *p0 = generate_random_program(&programs);
        while (ex_number < MAX_EX_NUMBER) { 
            p0 = evaluate(&programs, p0);
            size_t index = p0 - programs.items;
            cycle_number = add_to_hash(&ht, &programs, index);
            if(cycle_number) break;
            ++ex_number;

        }
        
        // if (cycle_number > highest_cycle_number) {
        //     qsort(programs.items, programs.count, sizeof(programs.items[0]), compare_ex_nr);
        //     print_programs(programs);
        //     write_programs_to_file(&programs, ex_number, cycle_number, bfl-1);
        //     nob_log(NOB_INFO,"Cycle detected with size: %zu, after %zu program executions", cycle_number, programs.items[programs.count-1].ex_number);
        //     highest_cycle_number = cycle_number;
        // }
        if (ex_number == MAX_EX_NUMBER) {
                qsort(programs.items, programs.count, sizeof(programs.items[0]), compare_ex_nr);
                // print_programs(programs);
                write_programs_to_file(&programs, ex_number, cycle_number, bfl-1);
                nob_log(NOB_INFO,"%zu unique program executions, cycle_size: %zu", ex_number, cycle_number);
                highest_execution_number = ex_number;
        }
        nob_da_free(programs);
        nob_da_free(ht);
        --do_search;
    }
}