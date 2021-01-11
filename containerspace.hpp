#ifndef CONTAINERSPACE_HPP
#define CONTAINERSPACE_HPP

#include <string>
#include <vector>
#include <random>

#include "container.hpp"

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

#endif // CONTAINERSPACE_HPP
