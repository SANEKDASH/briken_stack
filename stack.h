#ifndef STACKHEADER
#define STACKHEADER // STACK_HEADER_WITHOUT_COLLISIONS

#ifdef DEBUG
struct LogInfo
{
    const char *stack_name;
    const int  line;
    const char *func;
    const char *file;
    const char *date;
    const char *time;
};

#define STACKDUMP(stk) StackDump(stk, {#stk, __LINE__, __func__, __FILE__, __DATE__, __TIME__})
#define LGIN InitLog()
#define LGOUT CloseLog()

#else
#define STACKDUMP(stk) ;
#define LGIN ;
#define LGOUT ;
#endif

#ifdef HASH_PROTECTION
#define HASHSTACK(stk) HashStack(stk)
#else
#define HASHSTACK ;
#endif

#ifdef CANARY_PROTECTION
#define STRUCTPTR (char *)stk + sizeof(CanaryType_t)
#define DATASIZE stk->details.capacity * sizeof(StackType_t) + sizeof(CanaryType_t) * 2
#else
#define STRUCTPTR stk
#define DATASIZE stk->details.capacity * sizeof(StackType_t)
#endif

typedef unsigned StackErr_t;
typedef int StackType_t;
typedef unsigned long long CanaryType_t;

#ifdef HASH_PROTECTION
struct HashVals
{
    unsigned long long data_hash;
    unsigned long long struct_hash;
};
#endif

struct StackDetails
{
    StackType_t *data;
    int size;
    int capacity;
    unsigned int status;
};

struct Stack
{
    #ifdef CANARY_PROTECTION
    CanaryType_t left_canary;
    #endif

    StackDetails details;

    #ifdef HASH_PROTECTION
    HashVals hash;
    #endif

    #ifdef CANARY_PROTECTION
    CanaryType_t right_canary;
    #endif
};

typedef enum
{
    kStackClear           = 0,
    kNullData             = 1 << 0,
    kWrongPos             = 1 << 1,
    kWrongSize            = 1 << 2,
    kPosHigherSize        = 1 << 3,
    kDestroyedStack       = 1 << 4,
    kLeftCanaryScreams    = 1 << 5,
    kRightCanaryScreams   = 1 << 6,
    kRightDataCanaryFuck  = 1 << 7,
    kLeftDataCanaryFuck   = 1 << 8,
    kDataHashError        = 1 << 9,
    kStructHashError      = 1 << 10,
} StackStatus;

const size_t kStatusNumber = 12;


struct Error
{
    StackStatus err_code;
    const char* error_expl;
};

const Error ErrorArray[kStatusNumber] = {
                                            kStackClear         , "there is no troubles with stack",
                                            kNullData           , "data = nullptr",
                                            kWrongPos           , "current position in stack is wrong",
                                            kWrongSize          , "size of stack is wrong",
                                            kPosHigherSize      , "position on stack is higher than it's size",
                                            kDestroyedStack     , "stack is destroyed",
                                            kLeftCanaryScreams  , "left structure canary is broken, your data is in danger",
                                            kRightCanaryScreams , "right structure canary is broken, your data is in danger",
                                            kLeftDataCanaryFuck , "left data canary is fucked, your data may be fucked up",
                                            kRightDataCanaryFuck, "right data canary is fucked, your data may be fucked up",
                                            kDataHashError      , "data hash has wrong value, your data is in danger",
                                            kStructHashError    , "structure hash has wrong value, your data is in danger"
                                        };

StackErr_t StackVerify(Stack *stk);

StackErr_t StackInit(Stack *stk);

StackErr_t StackDtor(Stack *stk);

StackErr_t Push(Stack *stk, StackType_t in_value);

StackErr_t Pop(Stack *stk, StackType_t *ret_value);

void StackDump(const Stack *stk, LogInfo info);

CanaryType_t *GetRightDataCanaryPtr(const Stack *stk);

CanaryType_t *GetLeftDataCanaryPtr(const Stack *stk);

unsigned long long HashFunc(const void *data, int count);


#ifdef DEBUG
int InitLog();
int CloseLog();
#endif

#ifdef HASH_PROTECTION
static void HashStack(Stack *stk);
#endif

#endif
