#include <algorithm> // std::find
#include <iostream> // std::cout, std::cerr
#include <filesystem> // std::filesystem

#include <sched.h> // clone
#include <sys/types.h> // waitpid
#include <sys/wait.h> // waitpid, SIGCHILD
#include <unistd.h> // execv

#include "container.hpp"

namespace fs = std::filesystem;

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
    std::string suffix = ".container";
    
    for (const auto& entry : fs::directory_iterator(directory)) {
	std::string path = entry.path().string();
	if (path.size() >= suffix.size() &&
	    0 == path.compare(path.size()-suffix.size(), suffix.size(), suffix)) {
	    Container container(path, generate_port());
	    containers.emplace_back(container);
	}
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

int Container::internal_exec(void* args) {
    clone_args_t* _clone_args = (clone_args_t*) args;
    // unmount all
    // chroot (bind mount/pivot root dance)
    // mount /proc (make /dev?)
    // remove capabilities? or switch user
    const char* name = _clone_args->executable_name.c_str();
    char* port_buffer = (char*) (std::to_string(_clone_args->port).c_str());
    char* newargv[] = { port_buffer, NULL };
    
    execv(name, newargv);
    std::cerr << "Starting a container failed." << std::endl;
    exit(EXIT_FAILURE);
    
    return 0;
}

Container::Container(std::string _executable_name, int _port) {
    clone_args.executable_name = _executable_name;
    clone_args.port = _port;
}

void Container::run() {
    char* stack = static_cast<char*>(malloc(4096)) + 4096;
    int namespaces = CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWNS;// | CLONE_NEWNET
    
    pid_t p = clone(internal_exec, stack, SIGCHLD|namespaces, &clone_args);
    
    if (p == -1) {
	std::cerr << "Error while cloning the container." << std::endl;
        exit(EXIT_FAILURE);
    }
    
    std::cout << "Spawning a container (pid: " << p << ", executable name: \"" << clone_args.executable_name << "\", port: " << clone_args.port << ")." << std::endl;

    pid = p;
}

void Container::wait() {
    waitpid(pid, NULL, 0);
}
