#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define NOB_IMPLEMENTATION
#include "nob.h"
#define ARENA_IMPLEMENTATION
#include "arena.h"


static Arena static_arena = {0};
static Arena *context_arena = &static_arena;
#define context_da_append(da, x) arena_da_append(context_arena, (da), (x))

#define u8 uint8_t
#define u64 uint64_t

#define MAX_TAPE_SIZE 64
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
} BF7;

static_assert(COUNT == 11, "Amount of instructions have changed");

const char *ins_bf7[COUNT] = {
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
    BFL7,
    
    BFL_COUNT
} BFL;

const char *_bfl_str[BFL_COUNT] = {
    [BFL1] = "bf1",
    [BFL2] = "bf2",
    [BFL3] = "bf3",
    [BFL4] = "bf4",
    [BFL5] = "bf5",
    [BFL6] = "bf6",
    [BFL7] = "bf7",
};

Program *generate_random_program_full_length(Programs *programs) {
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
        printf("%s", ins_bf7[instruction]);
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
Hash Table implementations
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


// we should allocate these da in an arena.
typedef struct {
    Program *items;     //  array of Programs with their tape data, can be NULL 
    size_t index;
    size_t counter;     // cycle or sequence length
    size_t count;
    size_t capacity;
} HIST_DATA ; // Program Sequence Length

typedef enum {
    PCL,
    PSL,
} HIST_KIND;

// we should allocate tisbn da in an arena.
typedef struct {
    HIST_DATA *items;
    size_t count;
    size_t capacity;
    HIST_KIND as;
} HIST; // Histogram


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

#define add_to_hist(da, idx, cutoff, prog)  \
    do { \
        bool found = false; \
        size_t index_to_add = (idx); \
        for (size_t i = 0; i < (da)->count; ++i) { \
            if ((da)->items[i].index == index_to_add) { \
                (da)->items[i].counter++; \
                if (index_to_add >= cutoff) { \
                    Program newProg = {0}; \
                    memcpy(newProg.tape, prog.tape, MAX_TAPE_SIZE * sizeof(u8)); \
                    nob_da_append((&(da)->items[i]), newProg); \
                } \
                found = true; \
                break; \
            } \
        } \
        if (!found) { \
            HIST_DATA new_item = {.items = NULL, .index = index_to_add, .counter = 1}; \
            if (index_to_add >= cutoff) { \
                Program newProg = {0}; \
                memcpy(newProg.tape, prog.tape, MAX_TAPE_SIZE * sizeof(u8)); \
                nob_da_append((&new_item), newProg); \
            } \
            nob_da_append((da), new_item); \
        } \
    } while(0)
    
    
int compare_index(const void *a, const void *b) {
    const  HIST_DATA *ap = a;
    const  HIST_DATA *bp = b;    
    return (int)ap->index - (int)bp->index;
}
    
#define print_histo(da) \
    do { \
        qsort((da).items, (da).count, sizeof((da).items[0]), compare_index); \
        for (size_t i = 0; i < (da).count; ++i) { \
            printf("%zu: %zu\n", (da).items[i].index, (da).items[i].counter);\
        } \
    } while(0)

// bool test_ad_add() {
//     PSLs psls = {0};
//     psls.items = NULL;
//     psls.count = 0;
//     psls.capacity = 0;
    
//     for (size_t i = 0; i < 10; ++i) {
//         add_to_da(psls, i);
//         for (size_t j = 0; j < 20; ++j) {
//             add_to_da(psls, j);
//         }
        
//     }
//     print_da(psls);
    
//     nob_da_free(psls);
//     return true;
// }

bool write_programs_to_file(Programs *programs, size_t ex_number, size_t cycle_number, BFL bf) {
    char dir_path[200];
        snprintf(dir_path, sizeof(dir_path), "./%s_programs", _bfl_str[bf]);
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
                    dir_path,cycle_number, ex_number);
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
            fprintf(file, "%s", ins_bf7[instruction]);
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
    //memcpy(result.tape, source->tape, MAX_TAPE_SIZE * sizeof(u8));
    while (ins_count < MAX_INST_COUNT) {
        if (ins_head >= MAX_TAPE_SIZE) break;
        BF7 instruction = source->tape[ins_head];
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
                if (source->tape[read_head] != 0) {
                    int bracket_count = 1;
                    while (bracket_count > 0 && ins_head < MAX_TAPE_SIZE && ins_head > 0) {
                        ins_head--;
                        if (source->tape[ins_head] == MIL) bracket_count++;
                        if (source->tape[ins_head] == MIR) bracket_count--;
                    }
                } else {
                    ins_head = (ins_head + 1);
                }
                break;
            }
            case MIR:{
                if (source->tape[read_head] == 0) {
                    int bracket_count = 1;
                    while (bracket_count > 0 && ins_head < MAX_TAPE_SIZE && ins_head > 0) {
                        ins_head++;
                        if (source->tape[ins_head] == MIR) bracket_count++;
                        if (source->tape[ins_head] == MIL) bracket_count--;
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
                result.tape[write_head] = (source->tape[read_head] + 1) % COUNT;
                ins_head = (ins_head + 1);
                break; 
            }  
            case WE: {
                result.tape[write_head] = source->tape[read_head];
                ins_head = (ins_head + 1);
                break;
            }                
            case WM:{
                result.tape[write_head] = (source->tape[read_head] + COUNT - 1) % COUNT;
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

// Program *evaluate_bf7(Programs *programs, Program *source) {
//     if (source == NULL) {
//         return NULL;
//     }
//     size_t read_head = 0;   // Head for reading from source tape
//     size_t write_head = 0;  // Head for writing to result tape
//     size_t ins_head = 0;    // Instruction pointer for source program
//     int readh_d = 1;
//     int writeh_d = 1;
//     int insh_d = 1;
//     int write = 0;
    
//     size_t ins_count = 0;
    
//     Program result = {0};
//     result.ex_number = source->ex_number + 1;
//     //memcpy(result.tape, source->tape, MAX_TAPE_SIZE * sizeof(u8));
//     while (ins_count < MAX_INST_COUNT) {
//         if (ins_head >= MAX_TAPE_SIZE) break;
//         BF7 instruction = source->tape[ins_head];
//         if (instruction >= COUNT ){
//             nob_log(NOB_ERROR, "IMPOSSIBLE INSTRUCTION %d at %d", instruction, ins_head);
//             exit(1);
//         }
//         switch (instruction) {
//             case O:{
//                 break;
//             }                            
//             case MRL:{
//                 readh_d = -1;
//                 break;
//             }
//             case MRR: {
//                 readh_d = 1;
//                 break;
//             }                                
//             case MWL:{
//                 writeh_d = -1;
//                 break;
//             }                               
//             case MWR:{
//                 writeh_d = 1;
//                 break; 
//             }                             
//             case MIL:{
//                 insh_d = -1;
//                 break;
//             }
//             case MIR:{
//                 insh_d = 1;
//                 break;
//             }
//             case S:{
//                 size_t temp = read_head;
//                 read_head = write_head;
//                 write_head = temp;
//                 break;
//             }            
//             case WP:{
//                 write = 1;
//                 break; 
//             }  
//             case WE: {
//                 write = 0;
//                 break;
//             }                
//             case WM:{
//                 write = -1;
//                 break; 
//             }           
//             default:{
//                 nob_log(NOB_INFO, "WARNING: skipping unkown instruction: %s!", instruction);
//                 break;
//             }            
//         }
        
//         result.tape[write_head] = (source->tape[read_head] + write + COUNT) % COUNT;
//         read_head = (read_head + readh_d + MAX_TAPE_SIZE) % MAX_TAPE_SIZE;
//         write_head = (write_head + writeh_d + MAX_TAPE_SIZE) % MAX_TAPE_SIZE;
//         ins_head = ins_head + insh_d;
//         ins_count++;
//         if(ins_head > MAX_TAPE_SIZE || ins_head < 0) break;
//     }
//     nob_da_append(programs, result);
//     return &programs->items[programs->count-1];
// }

typedef struct {
    u8 *items;
    size_t count;
    size_t capacity;
} SEQ;


bool generate_instruction_sequence(SEQ *s, size_t idx) {
    
    size_t seq_len = 1;
    while (idx >= (pow(COUNT-1, seq_len))) {
        idx = idx -(pow(COUNT-1, seq_len));
        seq_len += 1;
    }
    
    while(seq_len > 0) {
        nob_da_append(s, (idx % (COUNT-1)) +1);
        idx = (div(idx, (COUNT-1)).quot);
        seq_len = seq_len - 1;
    }
    return true;
}

bool generate_random_instruction_sequence(SEQ *s, size_t seq_length) {
    for(int i = 0; i < seq_length; i++) {
        nob_da_append(s, rand() % COUNT);
    }
    return true;
}

#define print_da(da) \
    do { \
        for (size_t i = 0; i < (da).count; ++i) { \
            printf("%d,", (da).items[i]);\
        } \
        printf("\n");\
    } while(0)

void test_gen_ins_seq() {
    for (size_t i = 0; i < COUNT *3; ++i) {
        SEQ s ={0};
        generate_instruction_sequence(&s, i );
        print_da(s);
        nob_da_free(s);
    }
}

Program *generate_program(Programs *prgs, size_t idx) {
    SEQ s = {0};
    generate_instruction_sequence(&s, idx);
    Program p = {0};
    // size_t mid_s = s.count / 2;
    // size_t corrected_mid_tape = (MAX_TAPE_SIZE / 2)-mid_s; 
    memcpy(&p.tape, s.items, s.count * sizeof(u8));
    nob_da_append(prgs, p);
    nob_da_free(s);
    return &prgs->items[prgs->count - 1];
}

Program *generate_random_program(Programs *prgs, size_t seq_length) {
    SEQ s = {0};
    generate_random_instruction_sequence(&s, seq_length);
    Program p = {0};
    // size_t mid_s = s.count / 2;
    // size_t corrected_mid_tape = (MAX_TAPE_SIZE / 2)-mid_s; 
    memcpy(&p.tape, s.items, s.count * sizeof(u8));
    nob_da_append(prgs, p);
    nob_da_free(s);
    return &prgs->items[prgs->count - 1];
}

bool dump_histo_to_file(HIST *hist, size_t cutoff, size_t c, BFL bf) {
    char dir_path[100];
    snprintf(dir_path, sizeof(dir_path), "./%s_init_programs", _bfl_str[bf]);
    if (!nob_mkdir_if_not_exists(dir_path)) {
        nob_log(NOB_ERROR, "Could not create directory %s", dir_path);
        return false;
    }
    
    for (size_t i = 0; i < hist->count; ++i) { // iterate over the buckets
        if((hist->items[i].index >= cutoff) && (hist->items[i].counter >= c)) {
            
            char file_path[100];
            FILE *file = NULL;
            
            const char *hist_type = hist->as == PCL ? "cycle" : "seq"; 
            
            snprintf(file_path, sizeof(file_path), "%s/%s-%zu-%zu.txt",
                    dir_path, hist_type, hist->items[i].index, hist->items[i].counter);
            
            file = fopen(file_path, "w");
            
            if (file == NULL) {
                nob_log(NOB_ERROR, "Could not create unique file name or open file %s", file_path);
                return false;
            }
            
            for (size_t j = 0; j < hist->items[i].count; ++j) { 
                Program p = hist->items[i].items[j];
                for (int j = 0; j < MAX_TAPE_SIZE; j++) {
                    unsigned char instruction = p.tape[j] % COUNT;
                    fprintf(file, "%s", ins_bf7[instruction]);
                }
                fprintf(file, "\n");
            }
            fclose(file);
        }
    }
    return true;
}
    
#define MAX_EX_NUMBER 1024
#define DO_SEARCH 100000000
    
    

int main(int argc, char **argv) {
    
    const char *program_name = nob_shift(argv, argc);
    
    srand(time(NULL));
    size_t do_search = DO_SEARCH;
    size_t cycle_number = 0;
    size_t highest_cycle_number = 0;
    size_t highest_execution_number = 1;
    size_t cutoff_cycle_length = 66;
    size_t cutoff_sequence_length = 200;
    size_t cutoff_counter = 1;
    size_t bfl = 6;
    size_t start_idx = 0;
    Program* (*evaluate)(Programs *, Program *) = evaluate_bf6;
    
    char *file_name = NULL;
    
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
        else if (strcmp(flag, "-s") == 0){
            if (!flag_int(&argc, &argv, &start_idx)) return 1;
        }
        else if (strcmp(flag, "-f") == 0){
            const char *flag = nob_shift(argv, argc);
            if ((argc) <= 0) {
                nob_log(NOB_ERROR, "No argument is provided for %s", flag);
                return false;
            }
            file_name = nob_shift(argv, argc);
        }
        else {
            break;
        }
    }
    HIST psls = {0};
    psls.as = PSL;
    HIST pcls = {0};
    pcls.as = PCL;
        
    
    if (file_name != NULL) {
        nob_log(NOB_INFO, "Evaluating programs from file %s", file_name);

        FILE *file = fopen(file_name, "r");
        if (file == NULL) {
            nob_log(NOB_ERROR, "Could not open file %s", file_name);
            return 1;
        }

        char line[MAX_TAPE_SIZE*2];
        while (fgets(line, sizeof(line), file)) {
            Programs programs = {0};
            PKVs ht_pkv = {0};
            hash_init(&ht_pkv, MAX_EX_NUMBER);

            Program init_p = {0};
            size_t len = strlen(line);
            if (len > 0 && line[len-1] == '\n') {
                line[--len] = 0;
            }

            for (size_t i = 0; i < len && i < MAX_TAPE_SIZE; i++) {
                for (int j = 0; j < COUNT; j++) {
                    if (line[i] == ins_bf7[j][0]) {
                        init_p.tape[i] = j;
                        break;
                    }
                }
            }
            nob_log(NOB_INFO, "P:  %s", init_p.tape);

            nob_da_append(&programs, init_p);
            Program *p0 = &programs.items[0];
            // print_program(p0);
            size_t ex_number = 0;
            while (ex_number < MAX_EX_NUMBER) {
                p0 = evaluate(&programs, p0);
                size_t index = p0 - programs.items;
                cycle_number = add_to_hash(&ht_pkv, &programs, index);
                if(cycle_number) break;
                ++ex_number;
            }

            add_to_hist(&pcls, cycle_number, cutoff_cycle_length, &init_p);
            add_to_hist(&psls, ex_number, cutoff_sequence_length, &init_p);

            if (cycle_number >= highest_cycle_number) {
                qsort(programs.items, programs.count, sizeof(programs.items[0]), compare_ex_nr);
                write_programs_to_file(&programs, ex_number, cycle_number, bfl-1);
                nob_log(NOB_INFO,"Cycle detected with size: %zu, after %zu program executions", cycle_number, programs.items[programs.count-1].ex_number);
                highest_cycle_number = cycle_number;
            }
            if (ex_number >= highest_execution_number) {
                qsort(programs.items, programs.count, sizeof(programs.items[0]), compare_ex_nr);
                write_programs_to_file(&programs, ex_number, cycle_number, bfl-1);
                nob_log(NOB_INFO,"%zu unique program executions, cycle_size: %zu", ex_number, cycle_number);
                highest_execution_number = ex_number;
            }

            nob_da_free(programs);
            nob_da_free(ht_pkv);
        }

        fclose(file);

        nob_log(NOB_INFO,"Cycle length histogram:");
        print_histo(pcls);
        nob_log(NOB_INFO,"Program execution sequence length histogram");
        print_histo(psls);
        dump_histo_to_file(&pcls, cutoff_cycle_length, cutoff_counter, bfl-1);
        dump_histo_to_file(&psls, cutoff_sequence_length, cutoff_counter, bfl-1);
        nob_da_free(pcls);
        nob_da_free(psls);

    } else {
        nob_log(NOB_INFO,"Starting Experiment...");
        
        while (do_search) {
            if( do_search % 100000== 0){
               nob_log(NOB_INFO,"Cycle length histogram:");
               print_histo(pcls);
               nob_log(NOB_INFO,"Program execution sequence length histogram");
               print_histo(psls);
               nob_log(NOB_INFO, "experiments: %zu", DO_SEARCH - do_search);
            }
            Programs programs = {0};
            PKVs ht_pkv = {0};
            hash_init(&ht_pkv, MAX_EX_NUMBER);
            
            size_t ex_number = 0;
            Program *p0 = generate_program(&programs, (start_idx + DO_SEARCH) - do_search);
            Program init_p = *p0;
            while (ex_number < MAX_EX_NUMBER) { 
                p0 = evaluate(&programs, p0);
                size_t index = p0 - programs.items;
                cycle_number = add_to_hash(&ht_pkv, &programs, index);
                if(cycle_number) break;
                ++ex_number;
            }
            add_to_hist(&pcls, cycle_number, cutoff_cycle_length, &init_p);
            add_to_hist(&psls, ex_number, cutoff_sequence_length, &init_p);
            // nob_log(NOB_INFO, "p%zu: %zu-%zu", DO_SEARCH - do_search, cycle_number, ex_number);
            // write_programs_to_file(&programs, ex_number, cycle_number, bfl-1, DO_SEARCH - do_search);
            if (cycle_number > highest_cycle_number) {
                qsort(programs.items, programs.count, sizeof(programs.items[0]), compare_ex_nr);
                // print_programs(programs);
                write_programs_to_file(&programs, ex_number, cycle_number, bfl-1);
                nob_log(NOB_INFO,"Cycle detected with size: %zu, after %zu program executions", cycle_number, programs.items[programs.count-1].ex_number);
                highest_cycle_number = cycle_number;
            }
            if (ex_number > highest_execution_number) {
                qsort(programs.items, programs.count, sizeof(programs.items[0]), compare_ex_nr);
                // print_programs(programs);
                write_programs_to_file(&programs, ex_number, cycle_number, bfl-1);
                nob_log(NOB_INFO,"%zu unique program executions, cycle_size: %zu", ex_number, cycle_number);
                highest_execution_number = ex_number;
            }
            nob_da_free(programs);
            nob_da_free(ht_pkv);
            --do_search;
        }
        nob_log(NOB_INFO,"Cycle length histogram:");
        print_histo(pcls);
        nob_log(NOB_INFO,"Program execution sequence length histogram");
        print_histo(psls);
        dump_histo_to_file(&pcls, cutoff_cycle_length, cutoff_counter, bfl-1);
        dump_histo_to_file(&psls, cutoff_sequence_length, cutoff_counter, bfl-1);
        nob_da_free(pcls);
        nob_da_free(psls);
        
    }
    
    
    
}
