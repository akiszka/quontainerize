#include <iostream> // std::cout, std::cerr
#include <filesystem> // std::filesystem
#include <algorithm> // std::find

namespace fs = std::filesystem;

#include <unistd.h> // chdir

#include "containerspace.hpp"

int ContainerSpace::generate_port() {
    if (port_range_end - port_range_start < generated_ports.size()) {
	std::cerr << "Too many ports generated!" << std::endl;
	exit(EXIT_FAILURE);
    }
    
    int port = (*random_port_distribution)(random_engine);
    while (std::find(generated_ports.begin(), generated_ports.end(), port) != generated_ports.end()) {
	port = (*random_port_distribution)(random_engine);
    }
    
    generated_ports.push_back(port);
    return port;
}

void ContainerSpace::wait() {
    for (auto& container : containers) {
	container.wait();
    }
}

void ContainerSpace::run_containers(const std::string directory) {
    std::string suffix = ".manifest";
    
    for (const auto& entry : fs::directory_iterator(directory)) {
	std::string path = entry.path().string();
	if (path.size() >= suffix.size() &&
	    0 == path.compare(path.size()-suffix.size(), suffix.size(), suffix)) {
	    Container container(path, generate_port());
	    containers.emplace_back(container);
	}
    }

    if (chdir(directory.c_str()) != 0) {
	std::cerr << "Can't change directory to: " << directory << "." << std::endl;
    }
    
    for (auto& container : containers) {
	container.run();
    }
}

ContainerSpace::ContainerSpace(int _port_range_start, int _port_range_end) {
    port_range_start = _port_range_start;
    port_range_end = _port_range_end;
    random_port_distribution = new std::uniform_int_distribution<int> (port_range_start, port_range_end);
}

ContainerSpace::~ContainerSpace() {
    delete random_port_distribution;
}
