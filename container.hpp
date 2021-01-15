#ifndef CONTAINER_HPP
#define CONTAINER_HPP

#include <string>
#include <vector>
#include <sys/types.h>
#include <unistd.h>

class Container {
    pid_t pid;
    
    static int internal_exec(void *args);

    struct CLONE_ARGS_t {
	std::vector<std::string> command;
	std::string image_path;
	uid_t euid;
	gid_t egid;
	bool mount;
    } clone_args;
    typedef struct CLONE_ARGS_t clone_args_t;
public:
    Container(std::vector<std::string> command, std::string image_path, bool mount);

    void wait();
    void run();
};

#endif // CONTAINER_HPP
