#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>

#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

using namespace std;

map<string, string> ALIASES;

// divide a string str, de acordo com o delimitador d, retorna um vector de strings
vector<string> split(string str, char d = ' ')
{
    vector<string> result;
    stringstream ss(str);
    string token;

    if (d != ' ')
    {
        while (getline(ss, token, d))
            if (!token.empty())
                result.push_back(token);
    }
    else
    {
        while (ss >> token)
            if (!token.empty())
                result.push_back(token);
    }

    return result;
}

// retorna TRUE se str contem x, FALSE se não
bool contains(string str, string x)
{
    return str.find(x) != string::npos;
}

// imprime o prompt no terminal
void print_prompt()
{
    char username[100];
    char curr_dir[100];
    getlogin_r(username, 100);
    getcwd(curr_dir, 100);
    cout << "BRsh-" << string(username) << "-" << string(curr_dir) << "->";
}

// fecha todos os fds da matriz fd
void close_all_pipes(int fd[][2], int size)
{
    for (int i = 0; i < size; i++)
    {
        close(fd[i][0]);
        close(fd[i][1]);
    }
}

// executa comandos, com ou sem pipes
void execute_pipes(string command)
{
    vector<string> parts = split(command, '|');

    vector<int> pids(parts.size());

    int fd[parts.size() - 1][2];
    // read = fd[0]
    // write = fd[1]

    bool should_pipe = parts.size() > 1;

    // inicializando os pipes
    for (unsigned int i = 0; i < parts.size() - 1; i++)
        pipe(fd[i]);

    int used_pipe = 0;

    for (unsigned int i = 0; i < parts.size(); i++)
    {
        // processando a string do comando atual
        vector<string> arr = split(parts[i], ' ');
        const char *command_argv[arr.size() + 1];
        for (unsigned int j = 0; j < arr.size(); j++)
            command_argv[j] = arr[j].c_str();
        command_argv[arr.size()] = NULL;

        if (arr[0] == "cd")
        {
            if (chdir(arr[1].c_str()) == -1)
                cout << "Erro - Diretório não encontrado" << endl;
            break;
        }

        int pid = fork();
        pids.push_back(pid);
        if (pid == 0)
        {
            if (should_pipe)
            {
                // primeiro processo
                if (i == 0)
                    dup2(fd[used_pipe][1], STDOUT_FILENO);

                // ultimo processo
                else if (i == parts.size() - 1)
                    dup2(fd[used_pipe][0], STDIN_FILENO);

                // processos do meio
                else
                {
                    dup2(fd[used_pipe][0], STDIN_FILENO);
                    dup2(fd[used_pipe + 1][1], STDOUT_FILENO);
                }

                close_all_pipes(fd, parts.size() - 1);
            }
            if (execvp(command_argv[0], (char *const *)command_argv) == -1)
                cout << "Erro - Comando não encontrado - " << command_argv[0] << endl;
        }

        used_pipe = i;
    }

    if (should_pipe)
        close_all_pipes(fd, parts.size() - 1);

    for (int pid : pids)
        waitpid(pid, NULL, 0);
}

int main()
{
    string command = "";
    print_prompt();
    getline(cin, command);

    while (!(command == "sair" || command == "exit"))
    {
        execute_pipes(command);
        print_prompt();
        getline(cin, command);
    }

    return 0;
}