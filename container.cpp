#include <algorithm> // std::find
#include <iostream> // std::cout, std::cerr
#include <filesystem> // std::filesystem
#include <fstream> // std::ifstream
#include <map> // std::map

namespace fs = std::filesystem;

#include <sched.h> // clone
#include <sys/types.h> // waitpid
#include <sys/wait.h> // waitpid, SIGCHILD
#include <unistd.h> // execv, chdir

#include "container.hpp"

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

Container::Container(std::string manifest_name, int port) {
    std::map<std::string, std::string> configuration;
    
    std::ifstream manifest(manifest_name);
    if (manifest.is_open()) {
        std::string line;
        while(getline(manifest, line)){
            line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
            if(line[0] == '#' || line.empty())
		continue;
            auto delimiterPos = line.find("=");
            auto name = line.substr(0, delimiterPos);
            auto value = line.substr(delimiterPos + 1);
	    configuration[name] = value;
        }
        
    } else {
        std::cerr << "Couldn't open manifest file: " << manifest_name << ".\n";
	exit(EXIT_FAILURE);
    }
    
    clone_args.executable_name = configuration["executable"];
    clone_args.directory_path = configuration["dirname"];
    clone_args.port = port;
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

int Container::internal_exec(void* args) {
    clone_args_t* _clone_args = (clone_args_t*) args;
    // unmount all
    // chroot (bind mount/pivot root dance)
    // mount /proc (make /dev?)
    // remove capabilities? or switch user

    if (chroot(_clone_args->directory_path.c_str()) != 0) {
	std::cerr << "Cannnot chroot!" << std::endl;
	exit(EXIT_FAILURE);
    }

    if (chdir("/") != 0) {
	std::cerr << "Cannot change directory to new root!" << std::endl;
	exit(EXIT_FAILURE);
    }
    
    const char* name = _clone_args->executable_name.c_str();
    const std::string port_str = std::to_string(_clone_args->port);
    
    char* const newargv[] = { (char*) port_str.c_str(), (char*) NULL };
    
    execv(name, newargv);
    std::cerr << "Starting a container failed." << std::endl;
    exit(EXIT_FAILURE);
    
    return 0;
}

void Container::wait() {
    waitpid(pid, NULL, 0);
}
