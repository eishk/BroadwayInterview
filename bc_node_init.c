
int main(int argc, const char *argv[]) {
    
    signal(SIGINT, graceful_shutdown);

    
    beaten = 0;

    //Create id
    srand(time(NULL));
    our_id = rand();

    //Initialize Crypto
    OpenSSL_add_all_algorithms();
    OpenSSL_add_all_ciphers();
    ERR_load_crypto_strings();

    //Load defaults
    // SERVER 1 EX: "tcp://172.16.3.221:8080"
    // SERVER 3 EX: "tcp://172.16.3.244:8080"
    strcpy(our_ip, "tcp://172.16.3.221:8080");
    strcpy(node_list_file, "node.cfg");
    strcpy(pri_file, "pri_0.pem");
    strcpy(pub_file, "pub_0.pem");
    strcpy(our_chain_file, "chain_0.lampcoins");

    foreign_chains = dict_create();
    other_nodes = list_create();
    out_sockets = dict_create();

    printf("Foreign chains dictionary, list of other nodes, and out sockets dictionary
    were created successfully\n");


    int chain_good = read_chain_from_file(our_chain, our_chain_file);
    if(!chain_good) {
        our_chain = createBlockchain();
    }
    
    //Read in node_list
    int success = read_node_list();
    if(success) {
        printf("All good.\n");
    }
    else {
        printf("Please provide a proper node list file.\n");
        return 0;
    }
    
    int keys_good = read_keys(&our_keys,pri_file, pub_file);
    if(keys_good == 0){
        RSA_free(our_keys);
        our_keys = NULL;

    }
    create_keys(&our_keys,&pri_key,&pub_key);

    if(keys_good) {
        printf("Read Keypair:\n\n");
    }
    else {
        write_keys(&our_keys,pri_file,pub_file);
        printf("Created Keypair (Now Saved):\n\n");
    }

    strip_pub_key(stripped_pub_key, pub_key);
    printf("%s%s\n\n", pri_key, pub_key);

     //Create worker threads
    pthread_mutex_init(&our_mutex, NULL);
    pthread_create(&inbound_network_thread, NULL, in_server, NULL);
    pthread_create(&outbound_network_thread, NULL, out_server, NULL);
    pthread_create(&inbound_executor_thread, NULL, inbound_executor, NULL);
    close_threads = 0;


    //Create list of outbound msgs & add our ip to be sent to all nodes
    outbound_msg_queue = list_create();

    //Send out our existence
    li_foreach(other_nodes,announce_existence, NULL);
    last_ping = time(NULL);

    //Reset node earnings
    node_earnings = 0;

    //Create execution queue
    inbound_msg_queue = list_create();


    //Begin mining
    mine();

    return 0;

}



//Inbound thread function - receives messages and adds them to execution queue
void* in_server() {

    // For Leslie Lamport
    printf("lampcoin: Server v2.3\n");
    printf("Node IP: %s\n", our_ip);
    printf("Other node List: %s\n", node_list_file);
    printf("Pri Key File: %s\n", pri_file);
    printf("Pub Key File: %s\n", pub_file);
    printf("Chain File: %s\n", our_chain_file);


    sock_in = nn_socket (AF_SP, NN_PULL);
    assert (sock_in >= 0);


    // Bind to IP
    assert (nn_bind (sock_in, our_ip) >= 0);
    int timeout = 50;
    // timeout is set to 50 milliseconds currently
    assert (nn_setsockopt (sock_in, NN_SOL_SOCKET, NN_RCVTIMEO, &timeout, sizeof (timeout)) >= 0);

    printf("Inbound Socket Ready and Successfully Created.\n");

    char buf[MESSAGE_LENGTH];

    while(true) {
        int bytes = nn_recv(sock_in, buf, sizeof(buf), 0);
        
        pthread_mutex_lock(&our_mutex);
        if(bytes > 0) {
            buf[bytes] = 0;
            printf("\nReceived %d bytes: \"%s\"\n", bytes, buf);

            li_append(inbound_msg_queue,buf,bytes);
        }
        if(close_threads) {
                pthread_mutex_unlock(&our_mutex);            
                return 0;
        }
        pthread_mutex_unlock(&our_mutex);            
    }

    return 0;
}


