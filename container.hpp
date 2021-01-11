#ifndef CONTAINER_HPP
#define CONTAINER_HPP

#include <string>

class Container {
    pid_t pid;
    
    static int internal_exec(void *args);

    struct CLONE_ARGS_t {
	std::string executable_name;
	std::string directory_path;
	std::string args;
	bool mount;
	int port;
    } clone_args;
    typedef struct CLONE_ARGS_t clone_args_t;
public:
    Container(std::string manifest_name, int port);

    void wait();
    void run();
};

#endif // CONTAINER_HPP
