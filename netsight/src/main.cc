#include "netsight.hh"

int 
main(int argc, char **argv)
{
    NetSight &netsight = NetSight::get_instance();
    /* Go into interactive mode */
    netsight.interact();
    cout << "Bye!";
    return 0;
}
