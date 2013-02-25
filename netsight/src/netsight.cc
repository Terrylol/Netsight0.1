/*
 * Source: http://www.tcpdump.org/sniffex.c
 * */

#include <cstdio>
#include <vector>
#include <iostream>
#include <sys/time.h>
#include <pcap.h>
#include <pthread.h>
#include "netsight.hh"
#include "path_table.hh"
#include "filter/regexp.hh"

using namespace std;

vector<string> regexes;
vector<PacketHistoryFilter> filters;
PostcardList stage;
PathTable path_table;
pthread_mutex_t stage_lock;
pthread_cond_t round_cond;

void
interact()
{
    string input;
    while(true)
    {
        cout << "Enter a PHF to add or \"exit\" to exit.";
        cout << ">>";
        cin >> input;
        if(input == "exit")
            return;
        regexes.push_back(input);
        filters.push_back(PacketHistoryFilter(input.c_str()));
    }
}

int 
main(int argc, char **argv)
{
    pthread_t postcard_t;
    pthread_t history_t;

    /* Initialize mutexes and condition variables */
    pthread_mutex_init(&stage_lock, NULL);
    pthread_cond_init(&round_cond, NULL);

    /* Start round timers */
    struct itimerval round_timer;
    round_timer.it_interval.tv_usec = (int)(ROUND_LENGTH % 1000) * 1000;
    round_timer.it_interval.tv_sec = (int) (ROUND_LENGTH/1000);
    round_timer.it_value.tv_usec = (int)(ROUND_LENGTH % 1000) * 1000;
    round_timer.it_value.tv_sec = (int) (ROUND_LENGTH/1000);
    setitimer (ITIMER_REAL, &round_timer, NULL);

    /* Start postcard_worker thread */
    pthread_create(&postcard_t, NULL, &run_postcard_worker, NULL);

    /* Start history worker thread */
    pthread_create(&history_t, NULL, &run_history_worker, NULL);

    /* Go into interactive mode */
    interact();
    cout << "Bye!";
    return 0;
}
