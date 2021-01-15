#include <algorithm> // std::find
#include <iostream> // std::cout, std::cerr
#include <filesystem> // std::filesystem
#include <sstream> // std::stringstream

#include <sched.h> // clone
#include <sys/types.h> // waitpid, gete{uid,gid}
#include <sys/wait.h> // waitpid, SIGCHIL
#include <sys/mount.h> // mount, umonut
#include <fcntl.h> // openat, AT_FWCWD
#include <unistd.h> // execve, chdir, mkdtemp, gete{uid,gid}, sethostname
#include <sys/syscall.h> // For SYS_xxx definitions
#include <cstdlib> // mkdtemp, system
#include "container.hpp"

Container::Container(std::vector<std::string> command, std::string image_path, bool mount) {    
    clone_args.command = command;
    clone_args.image_path = image_path;
    clone_args.mount = mount;
    clone_args.euid = geteuid();
    clone_args.egid = getegid();
}

void Container::run() {
    char* stack = static_cast<char*>(malloc(4096)) + 4096;
    int namespaces = CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWNS | CLONE_NEWUSER;// | CLONE_NEWNET
    pid_t p = clone(internal_exec, stack, SIGCHLD|namespaces, &clone_args);
    
    if (p == -1) {
	std::cerr << "Error while cloning the container." << std::endl;
        exit(EXIT_FAILURE);
    }
    
    std::cout << "Spawning a container (pid: " << p << ")." << std::endl;

    pid = p;
}

int Container::internal_exec(void* args) {
    clone_args_t* _clone_args = (clone_args_t*) args;
    
    // map the current user and group to root
    {
	std::string uid_map = "0 " + std::to_string(_clone_args->euid) + " 1";
	std::string gid_map = "0 " + std::to_string(_clone_args->egid) + " 1";

	int fd = openat(AT_FDCWD, "/proc/self/uid_map", O_WRONLY);
	if (write(fd, uid_map.data(), uid_map.length()) != uid_map.length()) {
	    std::cerr << "Cannnot set uid map!" << std::endl;
	    exit(EXIT_FAILURE);
	}
	close(fd);

	fd = openat(AT_FDCWD, "/proc/self/setgroups", O_WRONLY);
	if (write(fd, "deny", 4) != 4) {
	    std::cerr << "Cannnot set gid map (stage 1)!" << std::endl;
	    exit(EXIT_FAILURE);
	}
	close(fd);
	
	fd = openat(AT_FDCWD, "/proc/self/gid_map", O_WRONLY);
	if (write(fd, gid_map.data(), gid_map.length()) != gid_map.length()) {
	    std::cerr << "Cannnot set gid map (stage 2)!" << std::endl;
	    exit(EXIT_FAILURE);
	}
	close(fd);
    }

    // first, a temp directory will be created. then the container "image" will be mounted there using fuse-overlayfs. this directory the container's rootfs.
    {
	std::string upperdir_template = std::filesystem::temp_directory_path() / "quont_runXXXXXX";
	std::string workdir_template = std::filesystem::temp_directory_path() / "quont_workXXXXXX";
	
	char* upperdir = mkdtemp(upperdir_template.data());
	char* workdir = mkdtemp(workdir_template.data());
	
	std::string srcdir = _clone_args->image_path;

	if (nullptr == upperdir || nullptr == workdir) {
	    std::cerr << "Cannot create a temporary directory." << std::endl;
	    exit(EXIT_FAILURE);
	}

	std::string mount_data = "lowerdir=" + srcdir + ",upperdir=" + upperdir + ",workdir=" + workdir;
	std::string command = "bash -c '";
	command += "fuse-overlayfs -o " + mount_data + " " + upperdir + "'";

	if (system(command.data()) != 0) { // this is disgusting, i'm probably going to hell for this
	    std::cerr << "Cannot mount read-only volume." << std::endl;
	    exit(EXIT_FAILURE);
	}
	
	if (chdir(upperdir) != 0) {
	    std::cerr << "Cannot change directory to new root." << std::endl;
	    exit(EXIT_FAILURE);
	}
    }

    // mount /proc if it was requested
    // TODO: mount /dev and /sys (possibly filtered)
    if (_clone_args->mount) {
	if (mount("proc", "./proc", "proc", 0, NULL) != 0) {
	    std::cerr << "Cannot mount filesystems." << std::endl;
	    exit(EXIT_FAILURE);
	}
    }

    // change the fs root to the current directory (this prevents chroot escape, see: pivot_root(2) manpage)
    if (syscall(SYS_pivot_root, ".", ".") != 0 ||
	umount2(".", MNT_DETACH) != 0) {
	std::cerr << "Cannnot pivot root!" << std::endl;
	exit(EXIT_FAILURE);
    }
    
    if (chroot(".") != 0) {
	std::cerr << "Cannnot chroot!" << std::endl;
	exit(EXIT_FAILURE);
    }

    // set the hostname
    {
	std::string hostname = "qont";
	sethostname(hostname.data(), hostname.length());
    }
    
    // finally, run the desired program within the container
    {
	std::vector<char*> argv;

	// convert the passed argv from std::string's to char*'s
	for (auto& arg : _clone_args->command) {
	    argv.push_back(arg.data());
	}
	argv.push_back(nullptr);

	std::stringstream executable_stream(_clone_args->command[0]);
	std::string executable;
	executable_stream >> executable;
	
	char* const env[] = { nullptr };

	execve(executable.data(), argv.data(), env);
	std::cerr << "Starting the contained program failed." << std::endl;
    }
    return 0;
}

void Container::wait() {
    waitpid(pid, NULL, 0);
}
