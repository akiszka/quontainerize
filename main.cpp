#include <fstream> // std::ifstream
#include <iostream> // std::cout, std::cerr
#include <algorithm> // std::remove_if
#include <map> // std::map
#include <random> // random ports
#include <filesystem> // std::filesystem
#include <stdlib.h> // itoa
namespace fs = std::filesystem;

#include <sched.h> // clone
#include <sys/types.h> // waitpid
#include <sys/wait.h> // waitpid, SIGCHILD
#include <unistd.h> // execv

std::map<std::string, int> configuration;
std::string executable_name = "";
int port = 0; // port of the containarized app

int config_read() {
    std::ifstream cFile("containers.cnf");
    if (cFile.is_open()) {
        std::string line;
        while(getline(cFile, line)){
            line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
            if(line[0] == '#' || line.empty())
		continue;
            auto delimiterPos = line.find("=");
            auto name = line.substr(0, delimiterPos);
            auto value = line.substr(delimiterPos + 1);
	    configuration[name] = std::stoi(value);
        }
        
    }
    else {
        std::cerr << "Couldn't open config file for reading.\n";
	return 1;
    }
    return 0;
}


int config_check_range() {
    int range_begin = configuration["range_begin"];
    int range_end = configuration["range_end"];
    int discovery_port = configuration["discovery_port"];

    if (range_begin > range_end) {
	std::cerr << "The port range for containers is incorrect." << std::endl;
	return -1;
    }
    else if (discovery_port >= range_begin && discovery_port <= range_end) {
	std::cerr << "The discovery port must not be in the container port range." << std::endl;
	return -1;
    }
    
    return 0;
}

int container_internal_exec(void *args) {
    // unmount all
    // chroot (bind mount/pivot root dance)
    // mount /proc (make /dev?)
    // remove capabilities? or switch user
    const char* name = executable_name.c_str();
    char* port_buffer = (char*) (std::to_string(port).c_str());
    char* newargv[] = { port_buffer, NULL };
    
    execv(name, newargv);
    std::cerr << "Starting a container failed." << std::endl;
    exit(EXIT_FAILURE);
    
    return 0;
}

pid_t container_run(std::string name, int _port) {
    char* stack = static_cast<char*>(malloc(4096)) + 4096;
    int namespaces = CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWNS;// | CLONE_NEWNET

    executable_name = name;
    port = _port;
    pid_t p = clone(container_internal_exec, stack, SIGCHLD|namespaces, NULL);

    if (p == -1) {
	std::cerr << "Error while cloning the container." << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Spawning a container (pid: " << p << ", executable name: \"" << executable_name << "\")." << std::endl;
    
    return p;
}

void container_wait(pid_t pid) {
    waitpid(pid, NULL, 0);
}

int main(int argc, char** argv) {
    if (config_read() != 0) return EXIT_FAILURE;
    if (config_check_range() != 0) return EXIT_FAILURE;
    // Ports for containers to run on will be generated at random
    std::default_random_engine random_engine;
    std::uniform_int_distribution<int> random_port_distribution(configuration["range_begin"], configuration["range_end"]);
    
    std::cout << "The container engine is running on ports from " << configuration["range_begin"] << " to " << configuration["range_end"] << "." << std::endl;

    // TODO: check if the range is correct
    
    pid_t p = container_run("./containers/masno", random_port_distribution(random_engine));
    container_wait(p);
    return 0;
}
