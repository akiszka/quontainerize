#ifndef CONTAINER_HPP
#define CONTAINER_HPP

#include <vector>
#include <random>

class Container {
    pid_t pid;
    
    static int internal_exec(void *args);

    struct CLONE_ARGS_t {
	std::string executable_name;
	int port;
    } clone_args;
    typedef struct CLONE_ARGS_t clone_args_t;
public:
    Container(std::string _executable_name, int _port);

    void wait();
    void run();
};

class ContainerSpace {
    std::vector<int> generated_ports;
    std::vector<Container> containers;

    std::default_random_engine random_engine;
    std::uniform_int_distribution<int>* random_port_distribution;

    int port_range_start, port_range_end;
    
    int generate_port();
public:
    ContainerSpace(int _port_range_start, int _port_range_end);
    ~ContainerSpace();
    
    void wait();
    void run_containers(const std::string directory);
};

#endif // CONTAINER_HPP
