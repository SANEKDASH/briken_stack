#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "stack.h"
#include "debug.h"

static const int kBaseStackSize = 8;

static const int kMultiplier = 2;
static const double kDevider = 0.5;

static const StackType_t kPoisonVal  = 0xBADBABA;
static const StackType_t kDestrVal   = 0xDEAD;
static const StackType_t *kDestrPtr  = (StackType_t*)0xDEADBEEF;

static const int kRabinBase = 3;

#ifdef CANARY_PROTECTION
const CanaryType_t kCanaryVal = 0xDEADB14D;
#endif

#ifdef DEBUG
static FILE *LogFile = nullptr;
#define LOGNAME "log_file.txt"
#endif

static int GetBit(int num, size_t pos);

static void PoisonMem(Stack *stk, StackType_t poison_value);

static StackErr_t ChangeStackCapacity(Stack *stk, double multiplier); // expand

static void HashStack(Stack *stk);

unsigned long long HashFunc(const void *data, int count);

#ifdef CANARY_PROTECTION
CanaryType_t *GetLeftDataCanaryPtr(const Stack *stk)
{
    return (CanaryType_t *)((char *)stk->details.data - sizeof(CanaryType_t)); // shift
}

CanaryType_t *GetRightDataCanaryPtr(const Stack *stk)
{
    return (CanaryType_t *)((char *)stk->details.data + stk->details.capacity * sizeof(StackType_t));
}
#endif

StackErr_t StackVerify(Stack *stk)
{
    CHECK(stk);
    if (stk == nullptr)
    {
        return kWrongSize;
    }

    if (stk->details.data == nullptr)
    {
        stk->details.status |= kNullData;
    }

    if (stk->details.size < 0)
    {
        stk->details.status |= kWrongPos;
    }

    if (stk->details.capacity <= 0)
    {
        stk->details.status |= kWrongSize;
    }

    if (stk->details.size > stk->details.capacity)
    {
        stk->details.status |= kPosHigherSize;
    }

    if  (  stk->details.data == kDestrPtr
        || stk->details.size == kDestrVal
        || stk->details.capacity == kDestrVal
        #ifdef HASH_PROTECTION
        || stk->hash.data_hash == kDestrVal
        || stk->hash.struct_hash == kDestrVal
        #endif
        #ifdef CANARY_PROTECTION
        || stk->left_canary  == kDestrVal
        || stk->right_canary == kDestrVal
        #endif
        )
    {
        stk->details.status |= kDestroyedStack;
    }
    #ifdef HASH_PROTECTION


    if (stk->details.status == kStackClear)
    {
        if (HashFunc(stk->details.data, sizeof(StackType_t) * stk->details.capacity) != stk->hash.data_hash)
        {
            stk->details.status |= kDataHashError;
        }
    }

    if (HashFunc(STRUCTPTR , sizeof(StackDetails)) != stk->hash.struct_hash)
    {
        stk->details.status |= kStructHashError;
    }
    #endif

    #ifdef CANARY_PROTECTION
    if (stk->left_canary != kCanaryVal)
    {
        stk->details.status |= kLeftCanaryScreams;
    }

    if (stk->right_canary != kCanaryVal)
    {
        stk->details.status |= kRightCanaryScreams;
    }

    if (stk->details.status == kStackClear)
    {
        if (*GetRightDataCanaryPtr(stk) != kCanaryVal)
        {
            stk->details.status |= kRightDataCanaryFuck;
        }

        if (*GetLeftDataCanaryPtr(stk) != kCanaryVal)
        {
            stk->details.status |= kLeftDataCanaryFuck;
        }
    }
    #endif


    return (StackStatus) stk->details.status;
}

StackErr_t StackInit(Stack *stk)
{
    stk->details.capacity = kBaseStackSize;
    stk->details.size = 0;
    stk->details.status = kStackClear;

    #ifdef CANARY_PROTECTION
    stk->left_canary = stk->right_canary = kCanaryVal;

    stk->details.data = (StackType_t *) calloc(kBaseStackSize * sizeof(StackType_t) + 2 * sizeof(CanaryType_t), sizeof(char));

    #else
    stk->details.data = (StackType_t *) calloc(kBaseStackSize, sizeof(StackType_t));
    #endif

    int errcode = errno;
    if (stk->details.data == nullptr)
    {
        perror(strerror(errcode));
        return kNullData;
    }

    #ifdef CANARY_PROTECTION
    stk->details.data = (StackType_t *)((char *)stk->details.data + sizeof(CanaryType_t));

    *GetLeftDataCanaryPtr(stk)  = kCanaryVal;
    *GetRightDataCanaryPtr(stk) = kCanaryVal;
    #endif

    PoisonMem(stk, kPoisonVal);

    HASHSTACK(stk);

    return kStackClear;
}

StackErr_t StackDtor(Stack *stk)
{
    CHECK(stk);

    stk->details.size  = kDestrVal;
    stk->details.capacity = kDestrVal;

    #ifdef CANARY_PROTECTION

    stk ->left_canary = stk->right_canary = kDestrVal;

    if (stk->details.data != nullptr)
    {
        free((char *)stk->details.data - sizeof(CanaryType_t));
    }

    #else
    free(stk->details.data);
    #endif

    stk->details.data = nullptr;

    #ifdef HASH_PROTECTION
    stk->hash.data_hash = stk->hash.struct_hash = -10;
    #endif

    return kStackClear;
}

StackErr_t Push(Stack *stk, StackType_t in_value)
{
    CHECK(stk);
    StackVerify(stk);

    STACKDUMP(stk);

    StackErr_t CheckExtend = kStackClear;
    if (stk->details.status == kStackClear)
    {
        if (stk->details.size >= stk->details.capacity)
        {
            CheckExtend = ChangeStackCapacity(stk, kMultiplier);
        }

        if (CheckExtend == kStackClear)
        {
            stk->details.data[(stk->details.size)++] = in_value;
        }

    HASHSTACK(stk);
    }


    return stk->details.status;
}

StackErr_t Pop(Stack *stk, StackType_t *ret_value)
{
    CHECK(stk);

    StackVerify(stk);
    STACKDUMP(stk);

    if (stk->details.status == kStackClear && stk->details.size > 0 )
    {
        *ret_value = stk->details.data[--(stk->details.size)];
        stk->details.data[stk->details.size] = kPoisonVal;

        if (stk->details.size < (stk->details.capacity * kDevider * kDevider)
            && (stk->details.capacity * kDevider) >= kBaseStackSize)
        {
            ChangeStackCapacity(stk, kDevider);
        }
    HASHSTACK(stk);
    }

    return stk->details.status;
}

void StackDump(const Stack *stk, LogInfo info)
{

    CHECK(stk);

    fprintf(LogFile, "[%s, %s]\nstack[%p] \"%s\" from line[%d], func[%s], file [%s]\n"
                     "{\n\tpos = %d\n\tsize = %d \n\tdata[%p]\n\tstack status = %d\n\n",
                                                                                        info.time,
                                                                                        info.date,
                                                                                        stk,
                                                                                        info.stack_name,
                                                                                        info.line,
                                                                                        info.func,
                                                                                        info.file,
                                                                                        stk->details.size,
                                                                                        stk->details.capacity,
                                                                                        stk->details.data,
                                                                                        stk->details.status);

    if (stk->details.status == 0)
    {
        fprintf(LogFile, "\terror: no errors.\n\n");
    }
    else
    {
        for (size_t pos = 0; pos < kStatusNumber; ++pos)
        {
            if (GetBit(stk->details.status, pos) > 0)
            {
                fprintf(LogFile, "\t*error: %s\n\n", ErrorArray[pos + 1].error_expl);
            }
        }
    }

    #ifdef CANARY_PROTECTION
    if (stk->details.status == kStackClear)
    {
        fprintf(LogFile, "\tleft structure canary val = 0x%x\n", stk->left_canary);
        fprintf(LogFile, "\tright structure canary val = 0x%x\n\n", stk->right_canary);
        fprintf(LogFile, "\tleft data canary val = 0x%x\n", *GetLeftDataCanaryPtr(stk));
        fprintf(LogFile, "\tright data canary val = 0x%x\n\n", *GetRightDataCanaryPtr(stk));
    }
    #endif

    #ifdef HASH_PROTECTION


        fprintf(LogFile, "\tcurrent structure hash value = %llu\n", stk->hash.struct_hash);
        fprintf(LogFile, "\tcurrent data hash value = %llu\n\n\t{\n", stk->hash.data_hash);

    #endif

    if (stk->details.status == kStackClear)
    {
        for (size_t i = 0; i < stk->details.capacity; ++i)
        {
            if (stk->details.data[i] == kPoisonVal)
            {
                fprintf(LogFile, "\t\t[%d] = 0x%x(PoisonVal)\n", i, kPoisonVal);
            }
            else
            {
                fprintf(LogFile, "\t\t[%d] = %d\n", i, stk->details.data[i]);
            }
        }
    }
    fprintf(LogFile, "\t}\n}\n\n");
}

static int GetBit(int num, size_t pos)
{
    CHECK(pos < kStatusNumber);

    return num & (1 << pos);
}

static void PoisonMem(Stack *stk, StackType_t poison_value)
{
    for (size_t i = stk->details.size; i < stk->details.capacity; i++)
    {
        stk->details.data[i] = poison_value;
    }
}

static StackErr_t ChangeStackCapacity(Stack *stk, double multiplier)
{
    STACKDUMP(stk);

    if (stk->details.status == kStackClear)
    {
        stk->details.capacity *= multiplier;

        #ifdef CANARY_PROTECTION
        stk->details.data = (StackType_t *)((char *)stk->details.data - sizeof(CanaryType_t));
        #endif

        StackType_t *check_ptr = (StackType_t *)realloc(stk->details.data, DATASIZE);

        int errcode = errno;
        if (check_ptr != nullptr)
        {
            stk->details.data = check_ptr;
        }
        else
        {
            printf("realloc error: %s\n", strerror(errcode));

            return kWrongSize;
        }

        #ifdef CANARY_PROTECTION
        stk->details.data = (StackType_t *)((char *)stk->details.data + sizeof(CanaryType_t));

        *GetRightDataCanaryPtr(stk) = kCanaryVal;
        *GetLeftDataCanaryPtr(stk)  = kCanaryVal;
        #endif

        PoisonMem(stk, kPoisonVal);
    }

    return stk->details.status;
}

unsigned long long HashFunc(const void *data, int count)
{
    unsigned long long hash_sum = 0;

    if (count <= 0)
    {
        return 0;
    }

    for (size_t i = 1; i <= count; i++)
    {
        hash_sum += i * *((char *)data + (i - 1));
    }

    return hash_sum;
}

int InitLog()
{
    int errcode = 0;
    if (LogFile == nullptr)
    {
        LogFile = fopen(LOGNAME, "w");

        errcode = errno;

        if (LogFile == nullptr)
        {
            printf("failed to init log (error: %s)\n", strerror(errcode));

            return errcode;
        }
    }

    return errcode;
}

int CloseLog()
{
    fclose(LogFile);

    int errcode = errno;
    if (errcode != 0)
    {
        printf("failed to close log (error: %s)\n", strerror(errcode));

        return errcode;
    }

    LogFile = nullptr;

    return errcode;
}


#ifdef HASH_PROTECTION
void HashStack(Stack *stk)
{
    if (stk->details.status == kStackClear)
    {
        stk->hash.data_hash   = HashFunc(stk->details.data, stk->details.capacity * sizeof(StackType_t));
        stk->hash.struct_hash = HashFunc(STRUCTPTR, sizeof(StackDetails));
    }
}
#endif
