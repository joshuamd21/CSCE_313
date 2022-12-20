#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>

#include <vector>
#include <string>

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define YELLOW "\033[1;33m"
#define BLUE "\033[1;34m"
#define WHITE "\033[1;37m"
#define NC "\033[0m"

using namespace std;

int kbhit(void)
{

    static bool initflag = false;
    static const int STDIN = 0;

    if (!initflag)
    {
        // Use termios to turn off line buffering
        struct termios term;
        tcgetattr(STDIN, &term);
        term.c_lflag &= ~ICANON;
        tcsetattr(STDIN, TCSANOW, &term);
        setbuf(stdin, NULL);
        initflag = true;
    }

    int nbbytes;
    ioctl(STDIN, FIONREAD, &nbbytes); // 0 is STDIN
    return nbbytes;
}

string solveExpansion(string input)
{
    if (input.find('$') != string::npos && input.find('(', input.find('$')) == input.find('$') + 1)
    {
        int sign = input.find('$');
        string expansion = input.substr(sign + 2, input.find(')', sign) - sign - 2);
        string solved = solveExpansion(expansion);
        input.replace(sign, input.find(')', sign), solved);
    }

    string output;

    if (input == "exit")
    { // print exit message and break out of infinite loop
        cout << RED << "Now exiting shell..." << endl
             << "Goodbye" << NC << endl;
        return "";
    }

    if (input == "")
    { // make sure the loop keeps going if input is empty
        return "";
    }

    // get tokenized commands from user input
    Tokenizer token(input);
    if (token.hasError())
    { // continue to next prompt if input had an error
        return "";
    }

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
            // checking for error
            if (token.hasError())
            {
                return "";
            }
            // checking for change directory command
            if (token.commands.at(0)->args[0] == "cd" || token.commands.at(0)->args[0] == "mkdir")
            {
                return "";
            }
            // In child, redirect output to write end of pipe
            dup2(pipefd[1], 1);
            // checking for first input having an input file
            if (i == 0 && token.commands.at(0)->hasInput())
            {
                int fd = open(token.commands.at(i)->in_file.c_str(), O_RDONLY);
                dup2(fd, 0);
            }

            // Close the read end of the pipe on the child side.
            close(pipefd[0]);

            // storing command into valid format
            vector<string> args = token.commands.at(i)->args;

            // checking for change directory c
            char **cmd = new char *[args.size()];
            for (long unsigned int i = 0; i < args.size(); i++)
            {
                cmd[i] = (char *)args.at(i).c_str();
            }

            // executing command
            execvp(cmd[0], cmd);
            delete[] cmd;
            return "";
        }
        else
        {
            // redirect the Shell(Parent's) input to the read end of the pipe
            dup2(pipefd[0], 0);
            // Close the write end of the pipe
            close(pipefd[1]);

            // wait for process
            waitpid(pid, 0, 0);
            if (i == token.commands.size() - 1)
            {
                getline(cin, output);
            }
        }
    }
    return output;
}

void changeDir(Tokenizer &token, vector<string> &last_dirs)
{
    // creating a temp holder for current directory
    char *temp = new char[100];
    getcwd(temp, 100);

    // string for holding the directory that should be moved to
    string dir = token.commands.at(0)->args[1];

    if (token.commands.at(0)->args[1] == "-" && last_dirs.size() > 0) // checking for last directory argument
    {
        dir = last_dirs.at(last_dirs.size() - 1);
    }
    else if (token.commands.at(0)->args[1] == "--" && last_dirs.size() > 1) // checking for two directories ago argument
    {
        dir = last_dirs.at(0);
    }

    // changing to directory
    chdir(dir.c_str());

    // pushing current directory onto vector
    last_dirs.push_back(temp);
    if (last_dirs.size() > 2) // checking to see if a directory needs to be removed from vector
    {
        last_dirs.erase(last_dirs.begin());
    }
    delete[] temp;
}
string getinput(vector<string> last_commands, char *curr_dir, string time)
{
    long unsigned int command = last_commands.size();
    string input;
    char c = ' ';
    bool one = false;
    bool two = false;

    while (true)
    {
        while (!kbhit())
        {
            sleep(0.1);
        }
        c = getchar();
        if (c == '')
        {
            one = true;
            continue;
        }
        else if (c == '[' && one)
        {
            two = true;
            continue;
        }
        else if (c == 'A' && one && two)
        {
            if (command == 0)
            {
                command += 1;
            }
            command -= 1;
            input = last_commands.at(command);
            cout << "\r\033[K"; // getting rid of current line

            // replaceing line with these outputs
            cout << YELLOW << time << curr_dir << "$" << NC << " " << flush;
            cout << last_commands.at(command) << flush;
            continue;
        }
        else if (c == 'B' && one && two)
        {
            if (command + 1 >= last_commands.size())
            {
                command += 1;
                input = "";
                cout << "\r\033[K"; // getting rid of current line

                // replaceing line with these outputs
                cout << YELLOW << time << curr_dir << "$" << NC << " " << flush;
            }
            else
            {
                command += 1;
                input = last_commands.at(command);
                cout << "\r\033[K"; // getting rid of current line

                // replaceing line with these outputs
                cout << YELLOW << time << curr_dir << "$" << NC << " " << flush;
                cout << last_commands.at(command) << flush;
            }
            continue;
        }
        if (c == '\n')
        {
            break;
        }
        if (c == '')
        {
            if (input.size() == 0)
            {
                continue;
            }
            input.pop_back();
            cout << "\r\033[K"; // getting rid of current line

            // replaceing line with these outputs
            cout << YELLOW << time << curr_dir << "$" << NC << " " << flush;
            cout << input << flush;
        }
        else
        {
            input += c;
        }
    }
    return input;
}
int main()
{
    // save original standard input and standard output
    int stdin = dup(0);
    int stdout = dup(1);

    // list of background processes
    vector<int> pids;

    // last directories
    vector<string> last_dirs;

    // last commands
    vector<string> last_commands;
    for (;;)
    {
        // checking on background processes
        for (int i = pids.size() - 1; i >= 0; i--)
        {
            if (waitpid(pids.at(i), 0, WNOHANG) > 0)
            {
                pids.pop_back();
            }
        }
        //  need date/time, username, and absolute path to current dir

        // date/time
        time_t curr_time;
        time(&curr_time);
        string time = ctime(&curr_time);
        time.erase(time.size() - 1);

        // absolute directory
        char *curr_dir = new char[256];
        getcwd(curr_dir, 256);

        cout << YELLOW << time << curr_dir << "$" << NC << " " << flush;
        // deleting current directory holder

        // get user inputted command
        string input = getinput(last_commands, curr_dir, time);
        delete[] curr_dir;

        // checking input for sign expansion
        if (input.find('$') != string::npos && input.find('(', input.find('$')) == input.find('$') + 1)
        {
            int sign = input.find('$');
            string expansion = input.substr(sign + 2, input.find(')', sign) - sign - 2);
            string solved = solveExpansion(expansion);
            input.replace(sign, input.find(')', sign) - sign + 1, solved);
        }
        if (input == "exit")
        { // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl
                 << "Goodbye" << NC << endl;
            break;
        }

        if (input == "")
        { // make sure the loop keeps going if input is empty
            continue;
        }

        // get tokenized commands from user input
        Tokenizer token(input);
        last_commands.push_back(input);
        if (token.hasError())
        { // continue to next prompt if input had an error
            continue;
        }

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
                // checking for error
                if (token.hasError())
                {
                    return 0;
                }
                // checking for change directory command
                if (token.commands.at(0)->args[0] == "cd" || token.commands.at(0)->args[0] == "mkdir")
                {
                    return 0;
                }
                // In child, redirect output to write end of pipe
                if (i < token.commands.size() - 1)
                    dup2(pipefd[1], 1);
                else if (token.commands.at(token.commands.size() - 1)->hasOutput())
                {
                    int fd = open(token.commands.at(i)->out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                    dup2(fd, 1);
                }
                // checking for first input having an input file
                if (i == 0 && token.commands.at(0)->hasInput())
                {
                    int fd = open(token.commands.at(i)->in_file.c_str(), O_RDONLY);
                    dup2(fd, 0);
                }

                // Close the read end of the pipe on the child side.
                close(pipefd[0]);

                // storing command into valid format
                vector<string> args = token.commands.at(i)->args;

                // checking for change directory c
                char **cmd = new char *[args.size()];
                for (long unsigned int i = 0; i < args.size(); i++)
                {
                    cmd[i] = (char *)args.at(i).c_str();
                }

                // executing command
                execvp(cmd[0], cmd);
                delete[] cmd;
                return 0;
            }
            else
            {
                // redirect the Shell(Parent's) input to the read end of the pipe
                dup2(pipefd[0], 0);
                // Close the write end of the pipe
                close(pipefd[1]);

                // checking if the command was cd
                if (token.commands.at(0)->args[0] == "cd")
                {
                    changeDir(token, last_dirs);
                }
                else if (token.commands.at(0)->args[0] == "mkdir") // checking if the command was make directory
                {
                    mkdir(token.commands.at(0)->args[1].c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
                }

                // wait until the last command finishes only if not background process
                if (token.commands.at(0)->isBackground())
                {
                    pids.push_back(pid);
                }
                else
                {
                    waitpid(pid, 0, 0);
                }
            }
        }

        // Reset the input and output file descriptors of the parent.
        dup2(stdin, 0);
        dup2(stdout, 1);
        // // print out every command token-by-token on individual lines
        // // prints to cerr to avoid influencing autograder
        // for (auto cmd : tknr.commands) {
        //     for (auto str : cmd->args) {
        //         cerr << "|" << str << "| ";
        //     }
        //     if (cmd->hasInput()) {
        //         cerr << "in< " << cmd->in_file << " ";
        //     }
        //     if (cmd->hasOutput()) {
        //         cerr << "out> " << cmd->out_file << " ";
        //     }
        //     cerr << endl;
        // }

        // fork to create child
        /*
        pid_t pid = fork();
        if (pid < 0)
        { // error check
            perror("fork");
            exit(2);
        }

        if (pid == 0)
        { // if child, exec to run command
            // run single commands with no arguments
            char *args[] = {(char *)tknr.commands.at(0)->args.at(0).c_str(), nullptr};

            if (execvp(args[0], args) < 0)
            { // error check
                perror("execvp");
                exit(2);
            }
        }
        else
        { // if parent, wait for child to finish
            int status = 0;
            waitpid(pid, &status, 0);
            if (status > 1)
            { // exit if child didn't exec properly
                exit(status);
            }
        }*/
    }
}
