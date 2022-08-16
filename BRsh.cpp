#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <queue>
#include <stack>
#include <fstream>

#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

using namespace std;

// used for storing aliases read from the .BRshrc file
map<string, string> ALIASES;

// used for keeping track of background processes
map<int, int> JOBS;

// stores the last 7 commands
queue<string> HISTORY;

// should be incremented when new background processes are created
int JOBS_COUNT;

// divides the string "str", according to the delimiter "d", returns a vector<string>
// if no delimiter is provided, the string is divided ignoring whitespaces, "\t", "\n", etc
vector<string> split(string str, string d = "")
{
    vector<string> result;

    if (d != "")
    {
        char *token = strtok((char *)str.c_str(), d.c_str());
        while (token != NULL)
        {
            result.push_back(string(token));
            token = strtok(NULL, d.c_str());
        }
    }
    else
    {
        stringstream ss(str);
        string token;
        while (ss >> token)
            if (!token.empty())
                result.push_back(token);
    }

    return result;
}

// returns true if str contains x, false otherwise
bool contains(string str, string x)
{
    return str.find(x) != string::npos;
}

// closes all file descriptors of the matrix fd
void close_all_pipes(int fd[][2], int size)
{
    for (int i = 0; i < size; i++)
    {
        close(fd[i][0]);
        close(fd[i][1]);
    }
}

// implements the "cd" command
void cd(string path)
{
    if (chdir(path.c_str()) == -1)
        cout << "Erro - Diretório não encontrado" << endl;
}

// implements the "version" command
void ver()
{
    cout << "BRsh 0.1.0 - Atualizado em: 08/08/2022 - Autor: João Coutinho" << endl;
}

// implements the "history" command
void history()
{
    queue<string> tmp_q = HISTORY;
    stack<string> tmp_s;

    // inverting the queue by using a stack
    while (!tmp_q.empty())
    {
        tmp_s.push(tmp_q.front());
        tmp_q.pop();
    }

    while (!tmp_s.empty())
    {
        cout << tmp_s.top() << endl;
        tmp_s.pop();
    }
}

// checks for any completed background processes
void check_bg()
{
    for (auto it = JOBS.begin(); it != JOBS.end();)
    {
        int status;
        int ret = waitpid(it->first, &status, WNOHANG);
        if (ret == 0 || ret == -1)
        {
            it++;
            return;
        }
        else if (WIFEXITED(status))
        {
            cout << "Processo em background [" << JOBS[ret] << "]"
                 << " [executado]" << endl;
            it = JOBS.erase(it);
        }
    }
}

// executes commands, with or without pipes
void execute_pipes(string line)
{
    vector<string> parts = split(line, "|");

    vector<int> pids(parts.size());

    int fd[parts.size() - 1][2];
    // read = fd[0]
    // write = fd[1]

    bool should_pipe = parts.size() > 1;
    bool bg = false;

    // initializing necessary pipes
    for (unsigned int i = 0; i < parts.size() - 1; i++)
        pipe(fd[i]);

    int used_pipe = 0;

    for (unsigned int i = 0; i < parts.size(); i++)
    {
        string command = parts[i]; // parts of the command (pipes)
        string in_file = "";
        string out_file = "";
        bool should_append = false;

        // if the last char is a &, this commands should be ran in bg
        // we assume there aren't trailing whitespace to the right
        if (command[command.size() - 1] == '&')
        {
            bg = true;
            command.erase(command.end() - 1);
        }

        // processing the ">>", ">", "<" strings (output and input redirects)
        vector<string> temp;
        if (contains(command, ">>"))
        {
            vector<string> arr = split(command, ">>");
            out_file = split(arr[arr.size() - 1])[0];
            command = arr[0];
            should_append = true;
        }
        else if (contains(command, ">"))
        {
            vector<string> arr = split(command, ">");
            out_file = split(arr[arr.size() - 1])[0];
            command = arr[0];
        }
        else if (contains(command, "<"))
        {
            vector<string> arr = split(command, "<");
            in_file = split(arr[arr.size() - 1])[0];
            command = arr[0];
        }

        // processing the next command to be executed
        vector<string> arr = split(command);

        if (arr[0] == "cd" || ALIASES[arr[0]] == "cd")
        {
            if (arr.size() >= 2)
                cd(arr[1]);
            return;
        }

        if (arr[0] == "ver")
        {
            ver();
            return;
        }

        if (arr[0] == "historico")
        {
            history();
            return;
        }

        int pid = fork();
        if (pid > 0)
            pids[i] = pid;
        else if (pid == 0)
        {
            // if an output file was passed, we should redirect the output, closing STDOUT
            if (out_file.size())
            {
                auto mode = should_append ? O_APPEND : O_TRUNC;
                int fd_out = open(out_file.c_str(), O_WRONLY | mode | O_CREAT, 0777);
                if (fd_out == -1)
                {
                    cout << "Erro - Abrindo o arquivo - " << out_file << endl;
                    exit(1);
                }
                else
                {
                    dup2(fd_out, STDOUT_FILENO);
                    close(fd_out);
                }
            }

            // if an input file was passed, we should redirect the input, closing STDIN
            if (in_file.size())
            {
                int fd_in = open(in_file.c_str(), O_RDONLY, 0777);
                if (fd_in == -1)
                {
                    cout << "Erro - Abrindo o arquivo - " << in_file << endl;
                    exit(1);
                }
                else
                {
                    dup2(fd_in, STDIN_FILENO);
                    close(fd_in);
                }
            }

            // if we should pipe
            if (should_pipe)
            {
                // for the first process, we only need to redirect the output into the pipe
                if (i == 0)
                    dup2(fd[used_pipe][1], STDOUT_FILENO);

                // for the last process, we only need to redirect the input, reading from the pipe
                else if (i == parts.size() - 1)
                    dup2(fd[used_pipe][0], STDIN_FILENO);

                // for the middle processes, if there are any
                // we should read from the current pipe, and write into the next one
                else
                {
                    dup2(fd[used_pipe][0], STDIN_FILENO);
                    dup2(fd[used_pipe + 1][1], STDOUT_FILENO);
                }

                close_all_pipes(fd, parts.size() - 1);
            }

            // converting the vector<string> to a null terminated char[][]
            const char *command_argv[arr.size() + 1];
            for (unsigned int j = 0; j < arr.size(); j++)
            {
                // tratando aliases
                if (ALIASES[arr[j]] != "")
                    command_argv[j] = ALIASES[arr[j]].c_str();
                else
                    command_argv[j] = arr[j].c_str();
            }
            command_argv[arr.size()] = NULL;

            if (execvp(command_argv[0], (char *const *)command_argv) == -1)
            {
                cout << "Erro - Comando não encontrado - " << command_argv[0] << endl;
                exit(1);
            }
        }

        used_pipe = i;
    }

    if (should_pipe)
        close_all_pipes(fd, parts.size() - 1);

    for (unsigned int i = 0; i < pids.size(); i++)
    {
        // if this command should be put on background
        if (bg && i == pids.size() - 1)
        {
            JOBS[pids[i]] = JOBS_COUNT;
            cout << "Processo em background [" << JOBS[pids[i]] << "]" << endl;
            waitpid(-1, NULL, WNOHANG);
            JOBS_COUNT++;
        }
        else
            waitpid(pids[i], NULL, 0);
    }
}

void read_aliases()
{
    ifstream file(".BRshrc");
    string line;
    while (getline(file, line))
    {
        vector<string> arr = split(line);
        if (arr[0] == "alias")
        {
            // removing the quotes
            string command = split(arr[1], "\"")[0];
            string new_command = split(arr[2], "\"")[0];
            ALIASES[new_command] = command;
        }
    }
    file.close();
}

// prints the prompt into the terminal
void print_prompt()
{
    char username[100];
    char curr_dir[100];
    getlogin_r(username, 100);
    getcwd(curr_dir, 100);
    cout << "BRsh-" << string(username) << "-" << string(curr_dir) << "->";
}

int main(int argc, char *argv[])
{
    read_aliases();

    // if we're getting file from the argv
    // we should read the commands from a file, and the shell shouldn't be run in interactive mode
    bool should_read_from_file = argc == 2;
    vector<string> file_lines;

    if (should_read_from_file)
    {
        string file_name = string(argv[1]);
        ifstream file(file_name);
        string line;

        while (getline(file, line))
        {
            // if the line isn't a command or isn't empty
            if (line[0] != '#' && line.size())
                file_lines.push_back(line);
        }

        file.close();
    }

    if (!should_read_from_file)
    {
        string command = "";
        print_prompt();
        getline(cin, command);

        while (!(command == "sair" || command == "exit"))
        {
            // we only need to process commands that aren't empty
            if (split(command).size())
            {
                if (HISTORY.size() == 7)
                    HISTORY.pop();
                HISTORY.push(command);
                check_bg();
                execute_pipes(command);
            }
            print_prompt();
            getline(cin, command);
        }
    }
    else
    {
        for (string line : file_lines)
        {
            if (line.size() && split(line).size())
            {
                if (HISTORY.size() == 7)
                    HISTORY.pop();
                HISTORY.push(line);
                check_bg();
                execute_pipes(line);
            }
        }
    }

    return 0;
}