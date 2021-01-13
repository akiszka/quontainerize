#ifndef CONTAINER_HPP
#define CONTAINER_HPP

#include <string>
#include <sys/types.h>
#include <unistd.h>

class Container {
    pid_t pid;
    
    static int internal_exec(void *args);

    struct CLONE_ARGS_t {
	std::string executable_name;
	std::string directory_path;
	std::string args;
	int port;
	uid_t euid;
	gid_t egid;
	bool mount;
	bool direct;
    } clone_args;
    typedef struct CLONE_ARGS_t clone_args_t;
public:
    Container(std::string manifest_name, int port);

    void wait();
    void run();
};

#endif // CONTAINER_HPP
