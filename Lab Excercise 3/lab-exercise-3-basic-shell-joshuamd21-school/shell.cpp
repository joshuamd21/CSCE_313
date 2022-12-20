/****************
LE2: Basic Shell
****************/
#include <unistd.h>   // pipe, fork, dup2, execvp, close
#include <sys/wait.h> // wait
#include <iostream>
#include "Tokenizer.h"
using namespace std;

int main()
{
    // TODO: insert lab exercise 2 main code here

    // save original standard input and standard output
    int stdin = dup(0);
    int stdout = dup(1);

    string input = "";
    while (true)
    {
        cout << "Provide Commands: ";
        getline(cin, input);

        // if input is exit(case invariant), break out of loop
        if (input.find("exit") < input.size())
        {
            break;
        }

        Tokenizer token(input);

        // for each command in token.commands
        for (long unsigned int i = 0; i < token.commands.size(); i++)
        {
            // Create pipe
            int pipefd[2];
            if (pipe(pipefd) == -1)
            {
                perror("pipe");
                return 0;
            }
            // Create child to run first command
            pid_t pid = fork();
            if (pid < 0)
            {
                cout << "error couldnt start the child process" << endl;
                return 0;
            }

            if (pid == 0)
            {
                // In child, redirect output to write end of pipe
                if (i < token.commands.size() - 1)
                    dup2(pipefd[1], 1);
                // Close the read end of the pipe on the child side.
                close(pipefd[0]);
                // storing command into valid format
                vector<string> args = token.commands.at(i)->args;
                char **cmd = new char *[args.size()]; // need to fix should not be 99 just temp number
                for (long unsigned int i = 0; i < args.size(); i++)
                {
                    cmd[i] = (char *)args.at(i).c_str();
                }

                // executing command
                execvp(cmd[0], cmd);
            }
            else
            {
                // redirect the Shell(Parent's) input to the read end of the pipe
                dup2(pipefd[0], 0);
                // Close the write end of the pipe
                close(pipefd[1]);
                // wait until the last command finishes
                wait(0);
            }
        }
        // Reset the input and output file descriptors of the parent.
        dup2(stdin, 0);
        dup2(stdout, 1);
    }
}