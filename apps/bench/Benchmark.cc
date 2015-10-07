#include <unistd.h>

#include <pthread.h>
#include <iostream>
#include <assert.h>
#include <signal.h>
#include <vector>

#include "atomic_ops.h"

#include "Benchmark.h"

using std::cout;
using std::endl;

static volatile bool Green_light __attribute__ ((aligned(64))) = false;
static std::string TxnOpDesc[TXN_NUM_OPS]  __attribute__ ((aligned(64))) =
    {"      ID", "Total   ", "Insert  ", "Remove  ", "Lookup T", "Lookup F"};

// signal handler to end the test
static void catch_SIGALRM(int sig_num)
{
    Green_light = false;
}

void barrier(int id, unsigned long nthreads)
{
    static struct
    {
        volatile unsigned long count;
        volatile unsigned long sense;
        volatile unsigned long thread_sense[MAX_NO_THREADS];
    } __attribute__ ((aligned(64))) bar = {0};

    bar.thread_sense[id] = !bar.thread_sense[id];
    if (r_fai(&bar.count) == nthreads - 1) {
        bar.count = 0;
        bar.sense = !bar.sense;
    }
    else
        while (bar.sense != bar.thread_sense[id]);     // spin
}

static void* fast_forward(void* arg)
{
    thread_args_t* args  = (thread_args_t*)arg;
    int            id    = args->id;
    unsigned int   seed  = id;
    int            i;
    int warmup = args->warmup;

    // choose actions before timing starts
    unsigned int* vals   = (unsigned int*)malloc(sizeof(unsigned int)*10000);
    unsigned int* chance = (unsigned int*)malloc(sizeof(unsigned int)*10000);
    for (i = 0; i < 10000; i++) {
        vals[i] = rand_r(&seed);
        chance[i] = rand_r(&seed) % BMCONFIG.NUM_OUTCOMES;
    }

    Benchmark* b = args->b;
    i = 0;

    // do fast_forward
    for (int w = 0; w < warmup; w++) {
        b->random_transaction(args, &seed, vals[i], chance[i]);
        ++i %= 10000;
    }

    return 0;
}

static void* work_thread(void* arg)
{
    thread_args_t* args  = (thread_args_t*)arg;
    int            id    = args->id;
    unsigned int   seed  = id;
    int            i;

    int warmup = args->warmup;
    int execute = args->execute;

    // choose actions before timing starts
    unsigned int* vals   = (unsigned int*)malloc(sizeof(unsigned int)*10000);
    unsigned int* chance = (unsigned int*)malloc(sizeof(unsigned int)*10000);

    for (i = 0; i < 10000; i++) {
        vals[i] = rand_r(&seed);
        chance[i] = rand_r(&seed) % BMCONFIG.NUM_OUTCOMES;
    }

    Benchmark* b = args->b;
    i = 0;


    barrier(args->id, args->threads);

    // do warmup. for simulation warmup is done by fast forwarding
    for (int w = 0; w < warmup; w++) {
        b->random_transaction(args, &seed, vals[i], chance[i]);
        ++args->count[TXN_GENERIC];
        ++i %= 10000;
    }
    barrier(args->id, args->threads);


    if (execute > 0) {
        for (int e = 0; e < execute; e++) {
            b->random_transaction(args, &seed, vals[i], chance[i]);
            ++args->count[TXN_GENERIC];
            ++i %= 10000;
        }
    }
    else {
        do {
            b->random_transaction(args, &seed, vals[i], chance[i]);
            ++args->count[TXN_GENERIC];
            ++i %= 10000;
        } while (Green_light);
    }

    barrier(args->id, args->threads);

    return 0;
}

void Benchmark::measure_speed()
{
    std::vector<thread_args_t> args;
    args.resize(BMCONFIG.threads);

    std::vector<pthread_t> tid;
    tid.resize(BMCONFIG.threads);


    // create <threads> threads, passing info via args
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    for (int i = 0; i < BMCONFIG.threads; i++) {
        args[i].id = i;
        args[i].b = this;
        args[i].duration = BMCONFIG.duration;
        args[i].threads = BMCONFIG.threads;
        args[i].warmup = BMCONFIG.warmup;
        args[i].execute = BMCONFIG.execute;
        for (int op = 0; op < TXN_NUM_OPS; op++)
            args[i].count[op] = 0;
    }

    fast_forward((void*)(&args[0]));

    // set the start flag so that worker threads can execute transactions
    Green_light = true;

    for (int j = 1; j < BMCONFIG.threads; j++)
        pthread_create(&tid[j], &attr, &work_thread, &args[j]);

    // set the signal handler unless warmup/execute are nonzero
    if (BMCONFIG.warmup * BMCONFIG.execute == 0)
        signal(SIGALRM, catch_SIGALRM);
    // start the timer, then set a signal to end the experiment
    double starttime = get_td();

    if (BMCONFIG.warmup * BMCONFIG.execute == 0)
        alarm(BMCONFIG.duration);

    // now have the main thread start doing transactions, too
    work_thread((void*)(&args[0]));

    // stop everyone, then join all threads and stop the timer
    Green_light = false;
    for (int k = 1; k < BMCONFIG.threads; k++)
        pthread_join(tid[k], NULL);
    double endtime = get_td();

    // Report the timing
    //stm::internal::report_timing();

    // Run the sanity check
    if (BMCONFIG.verify) {
        assert((args[0].b)->sanity_check());
        if (BMCONFIG.verbosity > 0)
            cout << "Completed sanity check." << endl;
    }

    // count the work performed
    long Total_count = 0;
    for (int l = 0; l < BMCONFIG.threads; l++) {
        Total_count += args[l].count[TXN_GENERIC];
    }

    // raw output:  total tx, total time
    cout << "Transactions: " << Total_count
         << ",  time: " << endtime - starttime << endl;

    // prettier output:  total tx/time
    if (BMCONFIG.verbosity > 0) {
        cout << (Total_count) / (endtime - starttime)
             << " txns per second (whole system)" << endl;
    }

    // even prettier output:  print the number of each type of tx
    if (BMCONFIG.verbosity > 1) {
        for (int op = 0; op < TXN_NUM_OPS; op++) {
            cout << TxnOpDesc[op];
            for (int l = 0; l < BMCONFIG.threads; l++) {
                cout << " | ";
                cout.width(7);
                if (op == TXN_ID)
                    cout << l;

                else cout << args[l].count[op];
                cout.width(0);
            }
            cout << " |" << endl;
        }
    }
}

// make sure all the parameters make sense
void BenchmarkConfig::verifyParameters()
{
    if (duration <= 0 && (warmup * execute == 0))
        argError("d must be positive or you must provide Warmup and "
                 "Xecute parameters");
    if (datasetsize <= 0)
        argError("m must be positive");
    if (threads <= 0)
        argError("p must be positive");
    if (unit_testing != 'l' && unit_testing != 'h' && unit_testing != ' ')
        argError("Invalid unit testing parameter: " + unit_testing);
}

// print parameters if verbosity level permits
void BenchmarkConfig::printConfig()
{
    if (verbosity >= 1) {
        cout << "Bench: use -h for help." << endl;
        cout << bm_name;
        cout << ", d=" << duration << " seconds, m="
             << datasetsize << " elements, " << threads << " thread(s)" << endl;
    }
}
