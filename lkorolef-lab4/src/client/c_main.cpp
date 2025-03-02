#include "../../include/client/client.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <memory>
#include <sstream>
#include <cstring>  // Required for strdup() and free()
#include <future>   // For std::promise and std::future

void client_thread(std::shared_ptr<Client::client_info> client_profile, std::promise<int> result_promise) {
    int status = 0; // Default to success
    try {
        Client client;
        client.socket_init();
        // std::cout << "Created thread and socket for " << client_profile->ip_str // debug
        //           << ":" << client_profile->port_num << std::endl;

        // Capture the return status of client_communicate
        status = client.client_communicate(client_profile->ip_str, client_profile->port_num,
                                           client_profile->winsz, client_profile->mss,
                                           client_profile->input_file, client_profile->output_file);

        if (status != 0) {  // If client_communicate reports failure
            std::cerr << "Thread failed with error code " << status << " on server "
                      << client_profile->ip_str << ":" << client_profile->port_num << std::endl;
        }
    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << " on server "
                  << client_profile->ip_str << ":" << client_profile->port_num << std::endl;
        status = 99;  // Assign a specific failure code for exceptions
    }

    free(client_profile->ip_str);  // Free allocated memory

    result_promise.set_value(status);  // Return status to main
}

int main(int argc, char* argv[]) {
    if (argc != 7) {
        std::cerr << "Incorrect number of arguments" << std::endl;
        return 1;
    }

    int servn = std::stoi(argv[1]);
    std::string config_file = argv[2];
    int mss = std::stoi(argv[3]);
    uint32_t winsz = static_cast<uint32_t>(std::stoi(argv[4]));
    std::string input_file = argv[5];
    std::string output_file = argv[6];

    if (mss < 0 || winsz <= 0 || servn <= 0) {
        std::cerr << "Incorrect argument values" << std::endl;
        return 1;
    }

    // Read server addresses from config file
    std::vector<std::pair<std::string, int>> servers;
    std::ifstream infile(config_file);
    std::string line;
    if(!infile.is_open()){
        std::cerr<<"Config file does not exist"<<std::endl;
        return 1;
    }

    while(std::getline(infile, line)){
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string ip;
        int port;
        
        if(!(iss >> ip >> port)){
            std::cerr << "Invalid line in config file: " << line << std::endl;
            continue;
        }

        servers.emplace_back(ip, port);
    }

    if(static_cast<int>(servers.size()) < servn){ //read up to max size of servers.size()
        servn = servers.size();
    }

    // Create threads and track results
    std::vector<std::thread> threads;
    std::vector<std::future<int>> results;

    for(int i = 0; i < servn; ++i){
        auto client_profile = std::make_shared<Client::client_info>();
        client_profile->ip_str = strdup(servers[i].first.c_str()); // allocate memory for char*
        if (!client_profile->ip_str) {
            std::cerr << "Memory allocation failed for IP string." << std::endl;
            return 1;
        }

        client_profile->port_num = servers[i].second;
        client_profile->winsz = winsz;
        client_profile->mss = mss;
        client_profile->input_file = input_file;
        client_profile->output_file = output_file;

        // Create a promise and future to track the thread's result
        std::promise<int> result_promise;
        results.push_back(result_promise.get_future());

        // Launch the thread
        threads.emplace_back(client_thread, client_profile, std::move(result_promise));
    }

    // print thread IDs debugging
    // for(size_t i = 0; i < threads.size(); ++i){
    //     std::cout << "Thread " << i << " id: " << threads[i].get_id() << std::endl;
    // }

    // Join threads
    for (auto &t : threads) {
        t.join();
    }

    // Check all return codes
    bool success = true;
    for (size_t i = 0; i < results.size(); ++i) {
        int status = results[i].get();  // Retrieve the return value of the thread
        if (status != 0) {
            //std::cerr << "Thread " << i << " failed with error code: " << status << std::endl;
            success = false;
        } else {
            //std::cout << "Thread " << i << " completed successfully." << std::endl;
            continue;
        }
    }

    if (success) {
        //std::cout << "All threads completed successfully." << std::endl;
        return 0;
    } else {
        std::cerr << "One or more threads encountered errors." << std::endl;
        return 6;
    }
}


