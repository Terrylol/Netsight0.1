/*
 * Copyright 2014, Stanford University. This file is licensed under Apache 2.0,
 * as described in included LICENSE.txt.
 *
 * Author: nikhilh@cs.stanford.com (Nikhil Handigol)
 *         jvimal@stanford.edu (Vimal Jeyakumar)
 *         brandonh@cs.stanford.edu (Brandon Heller)
 */

#include "tut/tut.hpp"
#include "tut/tut_reporter.hpp"
#include <iostream>

using namespace std;

namespace tut
{
    test_runner_singleton runner;
}

int main(int argc, const char* argv[])
{
    tut::reporter reporter;
    tut::runner.get().set_callback(&reporter);
    tut::runner.get().run_tests();

    return !reporter.all_ok();;
}
