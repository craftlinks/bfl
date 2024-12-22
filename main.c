#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#define NOB_IMPLEMENTATION
#include "nob.h"

#define MIN_PROGRAM_SIZE 64
#define MAX_TAPE_SIZE 256
#define MAX_INST_COUNT 12800
#define u8 uint8_t
#define u64 uint64_t

/*
Programs dynamic array
*/

typedef struct {
    char tape[MAX_TAPE_SIZE];
    size_t ex_number;
} Program;

typedef struct {
    Program *items;
    size_t count;
    size_t capacity;
} Programs;


/*
Instructions
*/

typedef enum {
    ZERO,
    MOV_WRITE_LEFT, 
    MOV_WRITE_RIGHT, 
    MOV_READ_LEFT, 
    MOV_READ_RIGHT,
    WRITE,
    WRITE_P1,
    WRITE_M1,
    INS_JMP_LEFT,
    INS_JMP_RIGHT,
    
    COUNT
} BF1;

static_assert(COUNT == 10, "Amount of instructions have changed");

const char *ins_bf1[COUNT] = {
    [ZERO] = "o",
    [MOV_WRITE_LEFT] = "<",
    [MOV_WRITE_RIGHT] = ">",
    [MOV_READ_LEFT] = "{",
    [MOV_READ_RIGHT] = "}",
    [WRITE] = "|",
    [WRITE_P1] = "p",
    [WRITE_M1] = "m",
    [INS_JMP_LEFT] = "l",
    [INS_JMP_RIGHT] = "r",
};

typedef enum {
    BFL1,
    BFL2,
    BFL3,
    BFL4,
    BFL5,
    
    BFL_COUNT
} BFL;

const char *_bfl[BFL_COUNT] = {
    [BFL1] = "bf1",
    [BFL2] = "bf2",
    [BFL3] = "bf3",
    [BFL4] = "bf4",
    [BFL5] = "bf5",
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
        printf("%s", ins_bf1[instruction]);
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
    Program *program;
    bool occupied;
} PKV;

typedef struct {
    PKV *items;
    size_t count;
    size_t capacity;
}PKVs;

#define hash_init(ht, cap) \
    do { \
        (ht)->items = malloc(sizeof(*(ht)->items)*cap); \
        memset((ht)->items, 0, sizeof(*(ht)->items)*cap); \
        (ht)->capacity = (cap); \
        (ht)->count = 0; \
    } while(0)
    
uint64_t hash(u8 *buf, size_t buf_size) {
    u64 hash = 5381;
    for( size_t i = 0; i < buf_size; ++i) {
        hash = ((hash << 5) + hash) + (u64)buf[i];
    }
    return hash;
}

bool tape_eq(char *a, char* b) {
    return memcmp(a, b, MAX_TAPE_SIZE) == 0;
    return true;
}

// for sorting da
int compare_ex_nr(const void *a, const void *b) {
    const Program *ap = a;
    const Program *bp = b;
    return (int)ap->ex_number - (int)bp->ex_number;
}

size_t add_to_hash(PKVs *ht, Program *program) {
    u64 h = hash((u8*)program->tape, MAX_TAPE_SIZE)%ht->capacity;
    for (size_t i = 0; i < ht->capacity && ht->items[h].occupied && !tape_eq(ht->items[h].program->tape, program->tape); ++i){
        h = (h+1)%ht->capacity;
    }
    if (ht->items[h].occupied) {
        if(!(tape_eq(ht->items[h].program->tape, program->tape))) {
            nob_log(NOB_ERROR, "Table overflow, increase table slot number!");
            return false;
        }
        size_t cycle_number = program->ex_number - ht->items[h].program->ex_number;
        // nob_log(NOB_INFO,"Cycle detected with size: %zu, after %zu program executions", cycle_number, program->ex_number);
        return cycle_number; 
    } else {
        ht->items[h].occupied = true;
        ht->items[h].program =  program;
    }
    return 0;
}

/*
BF variations
*/

Program *evaluate_test(Programs *programs, Program *source) {
    if (source == NULL) {
        return NULL;
    }
    Program result = {0};
    result.ex_number = source->ex_number + 1;
    size_t ins_head = 0;
    size_t ins_count = 0;
    while (ins_head < MAX_TAPE_SIZE/2 && ins_count < MAX_INST_COUNT) {
        result.tape[ins_head] = source->tape[ins_head];
        ++ins_head;
    }
    nob_da_append(programs, result);
    return &programs->items[programs->count-1];
}

Program *evaluate_bf1(Programs *programs, Program *source) {
    if (source == NULL) {
        return NULL;
    }

    Program result = {0};
    result.ex_number = source->ex_number + 1;
    size_t read_head = 0;   // Head for reading from source tape
    size_t write_head = 0;  // Head for writing to result tape
    size_t ins_head = 0;    // Instruction pointer for source program
    
    size_t ins_count = 0;
    // Process instructions until END or max tape size reached
    while (ins_count < MAX_INST_COUNT) {
        char instruction = source->tape[ins_head];
        
        switch (instruction) {
                
            case MOV_WRITE_LEFT:
                write_head = (write_head - 1) % MAX_TAPE_SIZE;
                result.tape[write_head] = source->tape[read_head];
                ins_head = (ins_head + 1) % MAX_TAPE_SIZE;
                break;
                
            case MOV_WRITE_RIGHT:
                write_head = (write_head + 1) % MAX_TAPE_SIZE;
                result.tape[write_head] = source->tape[read_head];
                ins_head = (ins_head + 1) % MAX_TAPE_SIZE;
                break;
                
            case MOV_READ_LEFT:
                read_head = (read_head - 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1) % MAX_TAPE_SIZE;
                break;
                
            case MOV_READ_RIGHT:
                read_head = (read_head + 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1) % MAX_TAPE_SIZE;
                break;
                
            case WRITE:
                result.tape[write_head] = source->tape[read_head];
                ins_head = (ins_head + 1) % MAX_TAPE_SIZE;
                break;
                
            case WRITE_P1:{
                BF1 o = (source->tape[read_head] + 1)%COUNT;
                result.tape[write_head] = o == 0 ? ZERO : o;
                write_head = (write_head + 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1) % MAX_TAPE_SIZE;
                break;
            }
            case WRITE_M1:{
                BF1 o = (source->tape[read_head] + 1)%COUNT;
                result.tape[write_head] = o == 0 ? ZERO : o;
                write_head = (write_head - 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1) % MAX_TAPE_SIZE;
                break; 
            }
               
                
            case INS_JMP_LEFT:
                result.tape[write_head] = source->tape[read_head];
                ins_head = (ins_head + 1) % MAX_TAPE_SIZE;
                break;
                
            case INS_JMP_RIGHT:
                result.tape[write_head] = source->tape[read_head];
                ins_head = (ins_head + 1) % MAX_TAPE_SIZE;
                break;
            default:
                ins_head = (ins_head + 1) % MAX_TAPE_SIZE;
                write_head = (write_head + 1) % MAX_TAPE_SIZE;
                read_head = (read_head + 1) % MAX_TAPE_SIZE;
                break;
        }
        ins_count++;
    }
    
    nob_da_append(programs, result);

    return &programs->items[programs->count-1];
}

Program *evaluate_bf2(Programs *programs, Program *source) {
    if (source == NULL) {
        return NULL;
    }

    Program result = {0};
    result.ex_number = source->ex_number + 1;
    size_t read_head = 0;   // Head for reading from source tape
    size_t write_head = 0;  // Head for writing to result tape
    size_t ins_head = 0;    // Instruction pointer for source program
    
    size_t ins_count = 0;
    // Process instructions until END or max tape size reached
    while (ins_count < MAX_INST_COUNT) {
        char instruction = source->tape[ins_head];
        
        switch (instruction) {
                
            case MOV_WRITE_LEFT:
                write_head = (write_head - 1) % MAX_TAPE_SIZE;
                result.tape[write_head] = source->tape[read_head];
                ins_head = (ins_head + 1);
                break;
                
            case MOV_WRITE_RIGHT:
                write_head = (write_head + 1) % MAX_TAPE_SIZE;
                result.tape[write_head] = source->tape[read_head];
                ins_head = (ins_head + 1);
                break;
                
            case MOV_READ_LEFT:
                read_head = (read_head - 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1);
                break;
                
            case MOV_READ_RIGHT:
                read_head = (read_head + 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1);
                break;
                
            case WRITE:
                result.tape[write_head] = source->tape[read_head];
                ins_head = (ins_head + 1);
                break;
                
            case WRITE_P1:{
                BF1 o = (source->tape[read_head] + 1)%COUNT;
                result.tape[write_head] = o == 0 ? ZERO : o;
                write_head = (write_head + 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1);
                break;
            }
            case WRITE_M1:{
                BF1 o = (source->tape[read_head] + 1)%COUNT;
                result.tape[write_head] = o == 0 ? ZERO : o;
                write_head = (write_head - 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1);
                break; 
            }
                
            case INS_JMP_LEFT:
                result.tape[write_head] = source->tape[read_head];
                ins_head = (ins_head + 1);
                break;
                
            case INS_JMP_RIGHT:
                result.tape[write_head] = source->tape[read_head];
                ins_head = (ins_head + 1);
                break;
            default:
                ins_head = (ins_head + 1);
                write_head = (write_head + 1) % MAX_TAPE_SIZE;
                read_head = (read_head + 1) % MAX_TAPE_SIZE;
                break;
        }
        ins_count++;
        if (ins_head > MAX_TAPE_SIZE) break;
    }
    
    nob_da_append(programs, result);

    return &programs->items[programs->count-1];
}

Program *evaluate_bf3(Programs *programs, Program *source) {
    if (source == NULL) {
        return NULL;
    }

    Program result = {0};
    result.ex_number = source->ex_number + 1;
    memcpy(result.tape, source->tape, MAX_TAPE_SIZE * sizeof(char));
    size_t read_head = 0;   // Head for reading from source tape
    size_t write_head = 0;  // Head for writing to result tape
    size_t ins_head = 0;    // Instruction pointer for source program
    
    size_t ins_count = 0;
    // Process instructions until END or max tape size reached
    while (ins_count < MAX_INST_COUNT) {
        char instruction = source->tape[ins_head];
        
        switch (instruction) {
                
            case MOV_WRITE_LEFT:
                write_head = (write_head - 1) % MAX_TAPE_SIZE;
                result.tape[write_head] = source->tape[read_head];
                ins_head = (ins_head + 1);
                break;
                
            case MOV_WRITE_RIGHT:
                write_head = (write_head + 1) % MAX_TAPE_SIZE;
                result.tape[write_head] = source->tape[read_head];
                ins_head = (ins_head + 1);
                break;
                
            case MOV_READ_LEFT:
                read_head = (read_head - 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1);
                break;
                
            case MOV_READ_RIGHT:
                read_head = (read_head + 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1);
                break;
                
            case WRITE:
                result.tape[write_head] = source->tape[read_head];
                ins_head = (ins_head + 1);
                break;
                
            case WRITE_P1:{
                BF1 o = (source->tape[read_head] + 1)%COUNT;
                result.tape[write_head] = o == 0 ? ZERO : o;
                write_head = (write_head + 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1);
                break;
            }
            case WRITE_M1:{
                BF1 o = (source->tape[read_head] + 1)%COUNT;
                result.tape[write_head] = o == 0 ? ZERO : o;
                write_head = (write_head - 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1);
                break; 
            }
               
                
            case INS_JMP_LEFT:
                // if (source->tape[read_head] == 0) {
                //     // Find matching right jump
                //     int bracket_count = 1;
                //     while (bracket_count > 0 && ins_head < MAX_TAPE_SIZE) {
                //         ins_head = (ins_head + 1) % MAX_TAPE_SIZE;
                //         if (source->tape[ins_head] == INS_JMP_LEFT) bracket_count++;
                //         if (source->tape[ins_head] == INS_JMP_RIGHT) bracket_count--;
                //     }
                // }
                result.tape[write_head] = source->tape[read_head];
                ins_head = (ins_head + 1);
                break;
                
            case INS_JMP_RIGHT:
                // if (source->tape[read_head] != 0) {
                //     // Find matching left jump
                //     int bracket_count = 1;
                //     while (bracket_count > 0 && ins_head > 0) {
                //         ins_head = (ins_head - 1) % MAX_TAPE_SIZE;
                //         if (source->tape[ins_head] == INS_JMP_RIGHT) bracket_count++;
                //         if (source->tape[ins_head] == INS_JMP_LEFT) bracket_count--;
                //     }
                // }
                result.tape[write_head] = source->tape[read_head];
                ins_head = (ins_head + 1);
                break;
            default:
                ins_head = (ins_head + 1);
                write_head = (write_head + 1) % MAX_TAPE_SIZE;
                read_head = (read_head + 1) % MAX_TAPE_SIZE;
                break;
        }
        ins_count++;
        if(ins_head > MAX_TAPE_SIZE) break;
    }
    
    nob_da_append(programs, result);

    return &programs->items[programs->count-1];
}

Program *evaluate_bf4(Programs *programs, Program *source) {
    if (source == NULL) {
        return NULL;
    }

    Program result = {0};
    result.ex_number = source->ex_number + 1;
    memcpy(result.tape, source->tape, MAX_TAPE_SIZE * sizeof(char));
    size_t read_head = 0;   // Head for reading from source tape
    size_t write_head = 0;  // Head for writing to result tape
    size_t ins_head = 0;    // Instruction pointer for source program
    
    size_t ins_count = 0;
    // Process instructions until END or max tape size reached
    while (ins_count < MAX_INST_COUNT) {
        char instruction = result.tape[ins_head];
        
        switch (instruction) {
                
            case MOV_WRITE_LEFT:
                write_head = (write_head - 1) % MAX_TAPE_SIZE;
                result.tape[write_head] = result.tape[read_head];
                ins_head = (ins_head + 1);
                break;
                
            case MOV_WRITE_RIGHT:
                write_head = (write_head + 1) % MAX_TAPE_SIZE;
                result.tape[write_head] = result.tape[read_head];
                ins_head = (ins_head + 1);
                break;
                
            case MOV_READ_LEFT:
                read_head = (read_head - 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1);
                break;
                
            case MOV_READ_RIGHT:
                read_head = (read_head + 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1);
                break;
                
            case WRITE:
                result.tape[write_head] = result.tape[read_head];
                ins_head = (ins_head + 1);
                break;
                
            case WRITE_P1:{
                BF1 o = (result.tape[read_head] + 1)%COUNT;
                result.tape[write_head] = o == 0 ? ZERO : o;
                write_head = (write_head + 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1);
                break;
            }
            case WRITE_M1:{
                BF1 o = (result.tape[read_head] + 1)%COUNT;
                result.tape[write_head] = o == 0 ? ZERO : o;
                write_head = (write_head - 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1);
                break; 
            }  
            case INS_JMP_LEFT:
                result.tape[write_head] = result.tape[read_head];
                ins_head = (ins_head + 1);
                break;
                
            case INS_JMP_RIGHT:
                result.tape[write_head] = result.tape[read_head];
                ins_head = (ins_head + 1);
                break;
            default:
                ins_head = (ins_head + 1);
                write_head = (write_head + 1) % MAX_TAPE_SIZE;
                read_head = (read_head + 1) % MAX_TAPE_SIZE;
                break;
        }
        ins_count++;
        if(ins_head > MAX_TAPE_SIZE) break;
    }
    
    nob_da_append(programs, result);

    return &programs->items[programs->count-1];
}

Program *evaluate_bf5(Programs *programs, Program *source) {
    if (source == NULL) {
        return NULL;
    }
    size_t read_head = 0;   // Head for reading from source tape
    size_t write_head = 0;  // Head for writing to result tape
    size_t ins_head = 0;    // Instruction pointer for source program
    
    size_t ins_count = 0;
    
    Program result = {0};
    result.ex_number = source->ex_number + 1;
    memcpy(result.tape, source->tape, MAX_TAPE_SIZE * sizeof(char));
    
    while (ins_count < MAX_INST_COUNT) {
        char instruction = result.tape[ins_head];
        
        switch (instruction) {
                
            case MOV_WRITE_LEFT:
                write_head = (write_head - 1) % MAX_TAPE_SIZE;
                result.tape[write_head] = result.tape[read_head];
                ins_head = (ins_head + 1);
                break;
                
            case MOV_WRITE_RIGHT:
                write_head = (write_head + 1) % MAX_TAPE_SIZE;
                result.tape[write_head] = result.tape[read_head];
                ins_head = (ins_head + 1);
                break;
                
            case MOV_READ_LEFT:
                read_head = (read_head - 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1);
                break;
                
            case MOV_READ_RIGHT:
                read_head = (read_head + 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1);
                break;
                
            case WRITE:
                result.tape[write_head] = result.tape[read_head];
                ins_head = (ins_head + 1);
                break;
                
            case WRITE_P1:{
                BF1 o = (result.tape[read_head] + 1)%COUNT;
                result.tape[write_head] = o == 0 ? ZERO : o;
                write_head = (write_head + 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1);
                break;
            }
            case WRITE_M1:{
                BF1 o = (result.tape[read_head] + 1)%COUNT;
                result.tape[write_head] = o == 0 ? ZERO : o;
                write_head = (write_head - 1) % MAX_TAPE_SIZE;
                ins_head = (ins_head + 1);
                break; 
            }  
            case INS_JMP_LEFT:
                result.tape[write_head] = result.tape[read_head];
                ins_head = (ins_head - 1);
                break;
                
            case INS_JMP_RIGHT:
                result.tape[write_head] = result.tape[read_head];
                ins_head = (ins_head + 1);
                break;
            default:
                ins_head = (ins_head + 1);
                write_head = (write_head + 1) % MAX_TAPE_SIZE;
                read_head = (read_head + 1) % MAX_TAPE_SIZE;
                break;
        }

        ins_count++;
        if(ins_head > MAX_TAPE_SIZE) break;
    }
   
    
    nob_da_append(programs, result);

    return &programs->items[programs->count-1];
}

#define MAX_EX_NUMBER 100000
#define DO_SEARCH 100000000

bool write_programs_to_file(Programs *programs, size_t ex_number, size_t cycle_number, BFL bfl) {
    char dir_path[100];
        snprintf(dir_path, sizeof(dir_path), "./%s_programs", _bfl[bfl]);
        if (!nob_mkdir_if_not_exists(dir_path)) {
            nob_log(NOB_ERROR, "Could not create directory %s", dir_path);
            return false;
        }
    
    char file_path[100];
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
            fprintf(file, "%s", ins_bf1[instruction]);
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

int main(int argc, char **argv) {
    
    const char *program_name = nob_shift(argv, argc);
    
    srand(time(NULL));
    size_t do_search = DO_SEARCH;
    size_t cycle_number = 0;
    size_t highest_cycle_number = 0;
    size_t highest_execution_number = 50;
    size_t bfl = 5;
    Program* (*evaluate)(Programs *, Program *) = evaluate_bf5;
    
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
        }
        else if (strcmp(flag, "-bfl") == 0) {
            if (!flag_int(&argc, &argv, &bfl)) return 1;
        } else {
            break;
        }
    }
    
    switch (bfl - 1) {
        case BFL1:
            evaluate = evaluate_bf1;
            break;
        case BFL2:
            evaluate = evaluate_bf2;
            break;
        case BFL3:
            evaluate = evaluate_bf3;
            break;
        case BFL4:
            evaluate = evaluate_bf4;
            break;
        case BFL5:
            evaluate = evaluate_bf5;
            break;
        default:
            nob_log(NOB_ERROR, "invalid bfl variant selection");
            return 1;
    }
        
    while (do_search) {
        
        if( do_search % 100000 == 0){
           nob_log(NOB_INFO, "experiments: %zu", DO_SEARCH - do_search); 
        }
        Programs programs = {0};
        PKVs ht = {0};
        hash_init(&ht, 50000);
        size_t ex_number = 0;
        
        Program *p0 = generate_random_program(&programs);
        
        while (ex_number < MAX_EX_NUMBER) { 
            p0 = evaluate(&programs, p0);
            cycle_number = add_to_hash(&ht, p0);
            if(cycle_number) break;
            ++ex_number;
        }
        
        if (cycle_number > highest_cycle_number) {
            qsort(programs.items, programs.count, sizeof(programs.items[0]), compare_ex_nr);
            print_programs(programs);
            write_programs_to_file(&programs, ex_number, cycle_number, bfl-1);
            nob_log(NOB_INFO,"Cycle detected with size: %zu, after %zu program executions", cycle_number, programs.items[programs.count-1].ex_number);
            highest_cycle_number = cycle_number;
        }
        else if (ex_number>= highest_execution_number) {
            qsort(programs.items, programs.count, sizeof(programs.items[0]), compare_ex_nr);
            print_programs(programs);
            write_programs_to_file(&programs, ex_number, cycle_number, bfl-1);
            nob_log(NOB_INFO,"%zu unique program executions, cycle_size: %zu", ex_number, cycle_number);
            highest_execution_number = ex_number;
        }
        nob_da_free(programs);
        nob_da_free(ht);
        --do_search;
    }
}