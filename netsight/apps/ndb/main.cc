#include <vector>
#include <string>
#include <pthread.h>

#include "api.hh"
#include "ndb.hh"
#include "types.hh"
#include "helper.hh"
#include "postcard.hh"
#include "packet.hh"

using namespace std;

int main(int argc, char **argv)
{
    NDB &ndb = NDB::get_instance();
    ndb.start();
    ndb.interact();
    return 0;
}
