#include <fstream>
#include <iostream>
#include <algorithm>
#include <map>
#include <random>
#include <filesystem>
#include <functional> // std::bind
#include <stdlib.h> // itoa
namespace fs = std::filesystem;

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sched.h>

std::map<std::string, int> configuration;
std::string executable_name = "";
int port = 0; // port of the containarized app

int read_config() {
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

int exec_container(void *args) {
    printf("spawning new child: %lu\n", (unsigned long)getpid());
    // unmount all
    // chroot (bind mount/pivot root dance)
    // mount /proc (make /dev?)
    // remove capabilities? or switch user
    const char* name = executable_name.c_str();
    char* port_buffer = (char*) (std::to_string(port).c_str());
    char* newargv[] = { port_buffer, NULL };
    
    execv(name, newargv);
    perror("exec");
    exit(EXIT_FAILURE);
    
    return 0;
}

pid_t run_container(std::string name, int _port) {
    char* stack = static_cast<char*>(malloc(4096)) + 4096;
    int namespaces = CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWNS;// | CLONE_NEWNET

    executable_name = name;
    port = _port;
    pid_t p = clone(exec_container, stack, SIGCHLD|namespaces, NULL);

    if (p == -1) {
        perror("clone");
        exit(1);
    }

    return p;
}

int main(int argc, char** argv) {
    if (read_config() != 0) return 1;

    // Ports for containers to run on will be generated at random
    std::default_random_engine random_engine;
    std::uniform_int_distribution<int> random_port_distribution(configuration["range_begin"], configuration["range_end"]);
    
    std::cout << "The container engine running on ports from " << configuration["range_begin"] << " to " << configuration["range_end"] << std::endl;

    pid_t p = run_container("./containers/masno", random_port_distribution(random_engine));
    printf("child pid: %d\n", p);
    waitpid(p, NULL, 0);
    return 0;
}
