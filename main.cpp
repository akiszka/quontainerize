#include <iostream> // std::cout

#include "containerspace.hpp"
#include "config.hpp"

int main(int argc, char** argv) {
    Configuration config("containers.cnf");

    std::cout << "The container engine is running on ports from " << config["range_begin"] << " to " << config["range_end"] << "." << std::endl;
    
    ContainerSpace containers(config["range_begin"], config["range_end"]);
    containers.run_containers("./containers");
    containers.wait();
    
    return 0;
}
