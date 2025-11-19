#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <queue>
#include <string>
#include <cstdlib>
#include "semaphore_class.h"

using namespace std;

// ======================================================
// Thread-Safe Print Queue (Fixes ALL Jumbled Output)
// ======================================================

queue<string> printQueue;
pthread_mutex_t printQueueLock = PTHREAD_MUTEX_INITIALIZER;
Semaphore printAvailable(0);

void enqueuePrint(const string &msg) {
    pthread_mutex_lock(&printQueueLock);
    printQueue.push(msg);
    pthread_mutex_unlock(&printQueueLock);
    printAvailable.signal();
}

void *printThread(void *arg) {
    while (true) {
        printAvailable.wait();
        pthread_mutex_lock(&printQueueLock);
        if (!printQueue.empty()) {
            string msg = printQueue.front();
            printQueue.pop();
            pthread_mutex_unlock(&printQueueLock);
            cout << msg << endl;
        } else {
            pthread_mutex_unlock(&printQueueLock);
        }
    }
    return nullptr;
}

// ======================================================
// Lightswitch Helper (Downey)
// ======================================================

class Lightswitch {
public:
    Lightswitch() : counter(0), mutex(1) {}
    void lock(Semaphore &sem) {
        mutex.wait();
        counter++;
        if (counter == 1) sem.wait();
        mutex.signal();
    }
    void unlock(Semaphore &sem) {
        mutex.wait();
        counter--;
        if (counter == 0) sem.signal();
        mutex.signal();
    }
private:
    int counter;
    Semaphore mutex;
};

// ======================================================
// Problem 1 — No-Starve Readers/Writers
// ======================================================

const int NUM_READERS = 5;
const int NUM_WRITERS = 5;

Semaphore rw_turnstile_1(1);
Semaphore rw_roomEmpty_1(1);
Lightswitch rw_readSwitch_1;

void *reader_no_starve(void *arg) {
    int id = (long)arg;

    for (int i = 0; i < 5; i++) {
        rw_turnstile_1.wait();
        rw_turnstile_1.signal();

        rw_readSwitch_1.lock(rw_roomEmpty_1);
        enqueuePrint("Reader " + to_string(id) + ": Reading.");
        usleep(100000);
        rw_readSwitch_1.unlock(rw_roomEmpty_1);

        usleep(150000);
    }
    return nullptr;
}

void *writer_no_starve(void *arg) {
    int id = (long)arg;

    for (int i = 0; i < 5; i++) {
        rw_turnstile_1.wait();
        rw_roomEmpty_1.wait();

        enqueuePrint("Writer " + to_string(id) + ": Writing.");
        usleep(150000);

        rw_roomEmpty_1.signal();
        rw_turnstile_1.signal();

        usleep(200000);
    }
    return nullptr;
}

void run_problem1() {
    enqueuePrint("=== Problem 1: No-starve Readers–Writers ===");

    pthread_t r[NUM_READERS], w[NUM_WRITERS];

    for (long i = 1; i <= NUM_READERS; i++)
        pthread_create(&r[i-1], NULL, reader_no_starve, (void*)i);

    for (long i = 1; i <= NUM_WRITERS; i++)
        pthread_create(&w[i-1], NULL, writer_no_starve, (void*)i);

    for (int i = 0; i < NUM_READERS; i++) pthread_join(r[i], NULL);
    for (int i = 0; i < NUM_WRITERS; i++) pthread_join(w[i], NULL);
}

// ======================================================
// Problem 2 — Writer-Priority Readers/Writers
// ======================================================

Semaphore rw_roomEmpty_2(1);
Semaphore rw_readTry_2(1);
Lightswitch rw_readSwitch_2;
Lightswitch rw_writeSwitch_2;

void *reader_writer_priority(void *arg) {
    int id = (long)arg;

    for (int i = 0; i < 5; i++) {
        rw_readTry_2.wait();
        rw_readSwitch_2.lock(rw_roomEmpty_2);
        rw_readTry_2.signal();

        enqueuePrint("Reader " + to_string(id) + ": Reading.");
        usleep(100000);

        rw_readSwitch_2.unlock(rw_roomEmpty_2);
        usleep(150000);
    }

    return nullptr;
}

void *writer_writer_priority(void *arg) {
    int id = (long)arg;

    for (int i = 0; i < 5; i++) {
        rw_writeSwitch_2.lock(rw_readTry_2);
        rw_roomEmpty_2.wait();

        enqueuePrint("Writer " + to_string(id) + ": Writing.");
        usleep(150000);

        rw_roomEmpty_2.signal();
        rw_writeSwitch_2.unlock(rw_readTry_2);

        usleep(200000);
    }
    return nullptr;
}

void run_problem2() {
    enqueuePrint("=== Problem 2: Writer-priority Readers–Writers ===");

    pthread_t r[NUM_READERS], w[NUM_WRITERS];

    for (long i = 1; i <= NUM_READERS; i++)
        pthread_create(&r[i-1], NULL, reader_writer_priority, (void*)i);

    for (long i = 1; i <= NUM_WRITERS; i++)
        pthread_create(&w[i-1], NULL, writer_writer_priority, (void*)i);

    for (int i = 0; i < NUM_READERS; i++) pthread_join(r[i], NULL);
    for (int i = 0; i < NUM_WRITERS; i++) pthread_join(w[i], NULL);
}

// ======================================================
// Problem 3 — Dining Philosophers (Limit 4)
// ======================================================

const int NUM_PHIL = 5;
Semaphore table_limit(4);
Semaphore forks[NUM_PHIL] = {
    Semaphore(1), Semaphore(1), Semaphore(1), Semaphore(1), Semaphore(1)
};

struct PhilArgs {
    int id;
    int solution;
};

void think(int id) {
    enqueuePrint("Philosopher " + to_string(id) + ": Thinking.");
    usleep(120000);
}

void eat(int id) {
    enqueuePrint("Philosopher " + to_string(id) + ": Eating.");
    usleep(120000);
}

void *philosopher1(void *arg) {
    int id = ((PhilArgs*)arg)->id;

    int left = id - 1;
    int right = id % NUM_PHIL;

    for (int i = 0; i < 5; i++) {
        think(id);

        table_limit.wait();
        forks[left].wait();
        forks[right].wait();

        eat(id);

        forks[right].signal();
        forks[left].signal();
        table_limit.signal();
    }
    return nullptr;
}

void run_problem3() {
    enqueuePrint("=== Problem 3: Dining Philosophers – Solution #1 ===");

    pthread_t t[NUM_PHIL];
    PhilArgs args[NUM_PHIL];

    for (int i = 0; i < NUM_PHIL; i++) {
        args[i] = { i + 1, 1 };
        pthread_create(&t[i], NULL, philosopher1, &args[i]);
    }

    for (int i = 0; i < NUM_PHIL; i++) pthread_join(t[i], NULL);
}

// ======================================================
// Problem 4 — Dining Philosophers (Odd/Even)
// ======================================================

void *philosopher2(void *arg) {
    int id = ((PhilArgs*)arg)->id;

    int left = id - 1;
    int right = id % NUM_PHIL;

    for (int i = 0; i < 5; i++) {
        think(id);

        if (id % 2 == 0) {
            forks[right].wait();
            forks[left].wait();
        } else {
            forks[left].wait();
            forks[right].wait();
        }

        eat(id);

        forks[left].signal();
        forks[right].signal();
    }
    return nullptr;
}

void run_problem4() {
    enqueuePrint("=== Problem 4: Dining Philosophers – Solution #2 ===");

    pthread_t t[NUM_PHIL];
    PhilArgs args[NUM_PHIL];

    for (int i = 0; i < NUM_PHIL; i++) {
        args[i] = { i + 1, 2 };
        pthread_create(&t[i], NULL, philosopher2, &args[i]);
    }

    for (int i = 0; i < NUM_PHIL; i++) pthread_join(t[i], NULL);
}

// ======================================================
// main()
// ======================================================

int main(int argc, char *argv[]) {
    if (argc != 2) {
        cerr << "Usage: ./cse4001_sync <problem #>\n";
        return 1;
    }

    pthread_t printer;
    pthread_create(&printer, NULL, printThread, NULL);

    int prob = atoi(argv[1]);

    switch (prob) {
        case 1: run_problem1(); break;
        case 2: run_problem2(); break;
        case 3: run_problem3(); break;
        case 4: run_problem4(); break;
        default:
            cerr << "Invalid problem number." << endl;
            return 1;
    }

    sleep(2); // let print queue flush
    return 0;
}
