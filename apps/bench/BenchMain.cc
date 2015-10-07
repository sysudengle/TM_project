#include <unistd.h>

#include <cstdlib>
#include <pthread.h>
#include <string>
#include <iostream>

#include "Benchmark.h"
#include "Counter.h"

#include "FGL.h"
#include "LinkedList.h"

#include "Hash.h"
#include "CGHash.h"
/*#include "LFUCache.h"
#include "LinkedListRelease.h"
#include "PrivList.h"
#include "RBTree.h"
#include "RBTreeLarge.h"
#include "RandomGraphList.h"
*/
using namespace bench;

using std::cerr;
using std::cout;
using std::endl;
using std::string;

BenchmarkConfig BMCONFIG;

static void usage()
{
    cerr << "Usage: Bench -B benchmark";
    cerr << " [flags]" << endl;
    cerr << "  Benchmarks:" << endl;
    cerr << "    Counter (default)  Shared counter  (TM)" << endl;
    cerr << "    LinkedList         Sorted linked list (TM)" << endl;
  //cerr << "    LinkedListRelease  LinkedList, early release" << endl;
    cerr << "    HashTable          256-bucket hash table (TM)" << endl;
  //cerr << "    RBTree   Red-black tree" << endl;
  //cerr << "    RBTreeLarge        Red-black tree with 4KB nodes" << endl;
  //cerr << "    LFUCache           Web cache simulation" << endl;
  //cerr << "    RandomGraph        Graph with 4 connections per node" << endl;
  //cerr << "    PrivList           Linked list privatization test" << endl;
    cerr << "    FineGrainList      Lock-based sorted linked list (TAS_locks)" << endl;
    cerr << "    CoarseGrainHash    256-bucket hash table, per-node locks (TAS_locks)" << endl;
    cerr << "    FineGrainHash      256-bucket hash table, per-bucket locks (TAS_locks)" << endl;
    cerr << endl;
    cerr << "  Flags:" << endl;
    cerr << "    -d: number of seconds to time (default 5)" << endl;
    cerr << "    -m: number of distinct elements (default 256)" << endl;
    cerr << "    -p: number of threads (default 2)" << endl;
    cerr << endl;
    cerr << "  Other:" << endl;
    cerr << "    -h: print help (this message)" << endl;
    cerr << "    -q: quiet mode" << endl;
    cerr << "    -v: verbose mode" << endl;
    cerr << "    -!: toggle verification at end of benchmark" << endl;
    cerr << "    -1: 80/10/10 lookup/insert/remove breakdown" << endl;
    cerr << "    -2: 33/33/33 lookup/insert/remove breakdown (default)"
         << endl;
    cerr << "    -3: 0/50/50  lookup/insert/remove breakdown" << endl;
    cerr << "    -T:[lh] perform light or heavy unit testing" << endl;
    cerr << "    -W/-X specify warmup and execute numbers" << endl;
    cerr << endl;
}

int main(int argc, char** argv)
{
    int opt;

    // parse the command-line options
    while ((opt = getopt(argc, argv, "B:H:a:d:m:p:hqv!x1234T:W:X:")) != -1)
    {
        switch(opt) {
          case 'B':
            BMCONFIG.bm_name = string(optarg);
            break;
          case 'T':
            BMCONFIG.unit_testing = string(optarg)[0];
            break;
          case 'W':
            BMCONFIG.warmup = atoi(optarg);
            break;
          case 'X':
            BMCONFIG.execute = atoi(optarg);
            break;
          case 'd':
            BMCONFIG.duration = atoi(optarg);
            break;
          case 'm':
            BMCONFIG.datasetsize = atoi(optarg);
            break;
          case 'p':
            BMCONFIG.threads = atoi(optarg);
            break;
          case 'h':
            usage();
            return 0;
          case 'q':
            BMCONFIG.verbosity = 0;
            break;
          case 'v':
            BMCONFIG.verbosity = 2;
            break;
          case '!':
            BMCONFIG.verify = !BMCONFIG.verify;
            break;
          case '1':
            BMCONFIG.lThresh = 24;
            BMCONFIG.iThresh = 27;
            break;
          case '2':
            BMCONFIG.lThresh = 10;
            BMCONFIG.iThresh = 20;
            break;
          case '3':
            BMCONFIG.lThresh = 0;
            BMCONFIG.iThresh = 15;
            break;
	  case '4':
            BMCONFIG.lThresh = 0;
            BMCONFIG.iThresh = 30;
            break;
        }
    }

    // make sure that the parameters all make sense
    BMCONFIG.verifyParameters();

    Benchmark* B = 0;

    if (BMCONFIG.bm_name == "Counter")
        B = new CounterBench();
    else if (BMCONFIG.bm_name == "FineGrainList")
        B = new IntSetBench(new FGL(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "FineGrainHash")
        B = new IntSetBench(new bench::HashTable<FGL>(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "CoarseGrainHash")
        B = new IntSetBench(new CGHash(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "HashTable")
        B = new IntSetBench(new bench::Hash(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "LinkedList")
        B = new IntSetBench(new LinkedList(), BMCONFIG.datasetsize);
    /*else if (BMCONFIG.bm_name == "RandomGraph")
        B = new RGBench<>(new RandomGraph(BMCONFIG.datasetsize),
                          BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "LFUCache")
        B = new LFUTest();
    else if (BMCONFIG.bm_name == "LinkedListRelease")
        B = new IntSetBench(new LinkedListRelease(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "PrivList")
        B = new PrivList(BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "RBTree")
        B = new IntSetBench(new RBTree(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "RBTreeLarge")
        B = new IntSetBench(new RBTreeLarge(), BMCONFIG.datasetsize);*/
    else
        argError("Unrecognized/Unimplemented benchmark name " + BMCONFIG.bm_name);

    // print the configuration for this run of the benchmark
    BMCONFIG.printConfig();

    // either verify the data structure or run a timing experiment
    if (BMCONFIG.unit_testing != ' ') {
        if (B->verify(BMCONFIG.unit_testing == 'l' ? LIGHT : HEAVY))
            cout << "Verification succeeded" << endl;
        else
            cout << "Verification failed" << endl;
    }
    else {
        B->measure_speed();
    }

    return 0;
}
