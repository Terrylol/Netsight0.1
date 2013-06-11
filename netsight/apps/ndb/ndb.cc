#include "ndb.hh"


NDB::NDB(string host):
    context(1),
    netsight_host(host)
{ 
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
}

/* signal handler */
void 
NDB::sig_handler(int signum)
{
    NDB &n = NDB::get_instance();
    switch(signum) {
        case SIGINT:
            n.cleanup();
            DBG("Bye!\n");
            exit(EXIT_SUCCESS);
            break;
        default:
            DBG("Unknexpected signal: %d\n", signum);
    }
}

void *
NDB::control_channel_thread(void *args)
{
    NDB &n = NDB::get_instance();

    // ZMQ socket (should be private to thread)
    zmq::socket_t req_sock(n.context, ZMQ_DEALER);
    // Set random ID to the socket
    string req_sock_id = s_set_id(req_sock);
    DBG("Setting the client id as \"%s\"\n", req_sock_id.c_str());
    //  Configure socket to not wait at close time
    int linger = 0;
    req_sock.setsockopt (ZMQ_LINGER, &linger, sizeof(linger));

    stringstream req_ss;
    req_ss << "tcp://" << n.netsight_host << ":" << NETSIGHT_CONTROL_PORT;
    req_sock.connect(req_ss.str().c_str());

    zmq::pollitem_t items [] = {
        { req_sock, 0, ZMQ_POLLIN, 0 },
    };
    while(!(n.s_interrupted)) {
        pthread_testcancel();

        struct timeval start_t, end_t;
        gettimeofday(&start_t, NULL);

        // Poll with a timeout of HEARTBEAT_INTERVAL
        zmq::poll (&items[0], 1, HEARTBEAT_INTERVAL * 1000);

        if(items[0].revents & ZMQ_POLLIN) {
            string message_str = s_recv(req_sock);
            assert(message_str.empty());
            message_str = s_recv(req_sock);

            stringstream ss(message_str);
            picojson::value message_j;
            ss >> message_j;
            MessageType msg_type = (MessageType) message_j.get("type").get<double>();
            picojson::value msg_data = message_j.get("data");

            if(msg_type == ECHO_REPLY) {
                DBG("Received ECHO_REPLY message with data: %s\n", msg_data.serialize().c_str());
            }
            else if(msg_type == ADD_FILTER_REPLY) {
                DBG("Received ADD_FILTER_REPLY message with data: %s\n", msg_data.serialize().c_str());
            }
            else if(msg_type == DELETE_FILTER_REPLY) {
                DBG("Received DELETE_FILTER_REPLY message with data: %s\n", msg_data.serialize().c_str());
            }
            else if(msg_type == GET_FILTERS_REPLY) {
                DBG("Received GET_FILTERS_REPLY message with data: %s\n", msg_data.serialize().c_str());
                picojson::array &filters_list = msg_data.get<picojson::array>();
                for (picojson::array::iterator iter = filters_list.begin(); iter != filters_list.end(); ++iter) {
                    string f = (*iter).get<string>();
                    print_color(ANSI_COLOR_BLUE, "\t\"%s\"\n", f.c_str());
                }
            }
            else {
                ERR("Unexpected message type: %d\n", msg_type);
            }
        }

        /* Send any outstanding messages */
        if(n.ctrl_cmd_queue.size() > 0) {
            pthread_mutex_lock(&n.ctrl_cmd_lock);
            for(int i = 0; i < n.ctrl_cmd_queue.size(); i++) {
                string &cmd = n.ctrl_cmd_queue[i][0];
                string &arg = n.ctrl_cmd_queue[i][1];
                if(cmd == "add") {
                    DBG("Adding filter: \"%s\"\n", arg.c_str());
                    add_filter_noblock(arg, req_sock);
                }
                else if(cmd == "del") {
                    DBG("Deleting filter: \"%s\"\n", arg.c_str());
                    delete_filter_noblock(arg, req_sock);
                }
                else if(cmd == "list") {
                    DBG("Listing filters...\n");
                    get_filters_noblock(req_sock);
                }
            }
            n.ctrl_cmd_queue.clear();
            pthread_mutex_unlock(&n.ctrl_cmd_lock);
        }

        // Sleep for the remainder of the period
        gettimeofday(&end_t, NULL);
        double sleep_time = HEARTBEAT_INTERVAL - diff_time_ms(end_t, start_t);
        if(sleep_time > 0) {
            s_sleep((int)sleep_time);
        }

        // Send out an ECHO_REQUEST
        stringstream ss;
        ss << time(NULL);
        EchoRequestMessage msg(ss.str());
        DBG("Sending ECHO_REQUEST with timestamp %s\n", ss.str().c_str());

        // req_sock is a dealer, send an empty string first, followed by the
        // real message
        bool ret = s_sendmore(req_sock, "");
        ret = s_send(req_sock, msg.serialize());
    }
}

void *
NDB::history_channel_thread(void *args)
{
    NDB &n = NDB::get_instance();

    // ZMQ socket (should be private to thread)
    zmq::socket_t sub_sock(n.context, ZMQ_SUB);
    //  Configure socket to not wait at close time
    int linger = 0;
    sub_sock.setsockopt (ZMQ_LINGER, &linger, sizeof(linger));

    stringstream sub_ss;
    sub_ss << "tcp://" << n.netsight_host << ":" << NETSIGHT_HISTORY_PORT;
    sub_sock.connect(sub_ss.str().c_str());

    zmq::pollitem_t items [] = {
        { sub_sock , 0, ZMQ_POLLIN, 0 },
    };
    while(!(n.s_interrupted)) {
        pthread_testcancel();

        // Poll with a timeout of HEARTBEAT_INTERVAL
        zmq::poll (&items[0], 1, HEARTBEAT_INTERVAL * 1000);

        if(items[0].revents & ZMQ_POLLIN) {
            string phf = s_recv(sub_sock);
            string history_str = s_recv(sub_sock);
            stringstream history_ss(history_str);
            picojson::value history_j;
            history_ss >> history_j;
            PostcardList *pl = PostcardList::decode_json(history_j);
            print_color(ANSI_COLOR_GREEN, "%s\n", phf.c_str());
            print_color(ANSI_COLOR_MAGENTA, "%s\n\n", history_str.c_str());
            print_color(ANSI_COLOR_BLUE, "%s\n\n", pl->str().c_str());
        }

        /* Subscribe/Unsubscribe any outstanding filters */
        if(n.hist_cmd_queue.size() > 0) {
            pthread_mutex_lock(&n.hist_cmd_lock);
            for(int i = 0; i < n.hist_cmd_queue.size(); i++) {
                string &cmd = n.hist_cmd_queue[i][0];
                string &arg = n.hist_cmd_queue[i][1];
                if(cmd == "add") {
                    subscribe_filter(arg, sub_sock);
                }
                else if(cmd == "del") {
                    unsubscribe_filter(arg, sub_sock);
                }
                else {
                    ERR("Unexpected command: %s\n", cmd.c_str());
                }
            }
            n.hist_cmd_queue.clear();
            pthread_mutex_unlock(&n.hist_cmd_lock);
        }
    }
}

void 
NDB::cleanup()
{
    void* ret = NULL;
    pthread_cancel(control_t);
    pthread_cancel(history_t);
    pthread_join(control_t, &ret);
    pthread_join(history_t, &ret);

    DBG("\nCleanup complete.\n");
}

void 
NDB::start()
{
    /* block out these signals 
     * any newly created threads will inherit this signal mask
     * */
    pthread_sigmask(SIG_BLOCK, &sigset, NULL);

    pthread_create(&control_t, NULL, &NDB::control_channel_thread, NULL);
    pthread_create(&history_t, NULL, &NDB::history_channel_thread, NULL);

    /* Unblock the signals */
    pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);

    /* create a signal handler */
    signal(SIGINT, sig_handler);

    interact();
}

void 
NDB::interact()
{
    cout << "Enter a PHF to add or \"exit\" to exit.";
    while(!s_interrupted)
    {
        cout << ">>";
        string input;
        cin.clear();
        cin.sync();
        if (!getline(cin, input)) {
            ERR("Error: Reading user input\n");
        }
        DBG("Got input\n");
        size_t command_pos = input.find(' ');
        string command = input.substr(0, command_pos);
        string arg = "";
        if(command == "add" || command == "del") {
            if(command_pos == string::npos) {
                ERR("Incomplete command: \"%s\"\n", input.c_str());
                continue;
            }
            arg = input.substr(command_pos + 1);
            int last_arg_char = arg.size() - 1;
            if((arg[0] == '\"' && arg[last_arg_char] == '\"') 
                    || (arg[0] == '\'' && arg[last_arg_char] == '\'')) {
                arg = arg.substr(1, arg.size() - 2);
            }
            /*else {
                ERR("Command error: \"%s\"\n", input);
                ERR("Could not match quotes for argument: \"%s\"\n", arg);
                continue;
            }*/
        }
        else if(command == "list") {
            arg = "";
        }
        else if(command == "help") {
            printf("Available commands: \n\
                        add \"<PHF>\"   \n\
                        del \"<PHF>\"   \n\
                        list            \n\
                        exit            \n\
                        help            \n\
                    ");
            continue;
        }
        else if(command == "exit") {
            s_interrupted = true;
            cleanup();
            continue;
        }
        else if(input == "") {
            continue;
        }
        else {
            ERR("Unrecognized command: \"%s\"\n", input.c_str());
            continue;
        }

        /* Add the new parsed command to ctrl_cmd_queue */
        vector<string> v;
        v.push_back(command);
        v.push_back(arg);
        pthread_mutex_lock(&ctrl_cmd_lock);
        ctrl_cmd_queue.push_back(v);
        pthread_mutex_unlock(&ctrl_cmd_lock);

        /* Add the new parsed command to hist_cmd_queue */
        if(command == "add" || command == "del") {
            pthread_mutex_lock(&hist_cmd_lock);
            hist_cmd_queue.push_back(v);
            pthread_mutex_unlock(&hist_cmd_lock);
        }
    }
}

