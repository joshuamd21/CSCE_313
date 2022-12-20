/****************
LE2: Introduction to Unnamed Pipes
****************/
#include <unistd.h> // pipe, fork, dup2, execvp, close
#include <iostream>
using namespace std;

int main () {
    // lists all the files in the root directory in the long format
    char* cmd1[] = {(char*) "ls", (char*) "-al", (char*) "/", nullptr};
    // translates all input from lowercase to uppercase
    char* cmd2[] = {(char*) "tr", (char*) "a-z", (char*) "A-Z", nullptr};

    // save original standard input and standard output
    int stdin = dup(0);
    int stdout = dup(1);

    // TODO: add functionality
    // Create pipe
    int pipefd[2];
    if(pipe(pipefd) == -1){
        perror("pipe");
        return 0;
    }

    // Create child to run first command
    pid_t pid = fork();
    if(pid < 0){
        cout << "error couldnt start the child process" << endl;
        return 0;
    }

    // In child, redirect output to write end of pipe
    if(pid == 0){
        dup2(pipefd[1], 1);
        // Close the read end of the pipe on the child side.
        close(pipefd[0]);
        // In child, execute the command
        execvp(cmd1[0],cmd1);
    }

    // Create another child to run second command
    pid_t pid2 = fork();
    if(pid2 < 0){
        cout << "error couldnt start the child process" << endl;
        return 0;
    }

    // In child, redirect input to the read end of the pipe
    if(pid2 == 0){
        dup2(pipefd[0], 0);
        // Close the write end of the pipe on the child side.
        close(pipefd[1]);
        // Execute the second command.
        execvp(cmd2[0],cmd2);
    }

    // Reset the input and output file descriptors of the parent.
    dup2(stdin, 0);
    dup2(stdout, 1);
}
