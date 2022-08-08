// joao pedro assuncao coutinho - 180019813

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

map<string, string> ALIASES;

map<int, int> JOBS;

queue<string> HISTORY;

int JOBS_COUNT;

// divide a string str, de acordo com o delimitador d, retorna um vector de strings
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

// retorna TRUE se str contem x, FALSE se não
bool contains(string str, string x)
{
    return str.find(x) != string::npos;
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

// implementa o comando "cd"
void cd(string path)
{
    if (chdir(path.c_str()) == -1)
        cout << "Erro - Diretório não encontrado" << endl;
}

// implementa o comando "ver"
void ver()
{
    cout << "BRsh 0.1.0 - Atualizado em: 08/08/2022 - Autor: João Coutinho" << endl;
}

// implementa o comando "historico"
void historico()
{
    queue<string> tmp_q = HISTORY;
    stack<string> tmp_s;

    // invertendo a queue usando um stack
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

// verifica se existem processos em background
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

// executa comandos, com ou sem pipes
void execute_pipes(string line)
{
    vector<string> parts = split(line, "|");

    vector<int> pids(parts.size());

    int fd[parts.size() - 1][2];
    // read = fd[0]
    // write = fd[1]

    bool should_pipe = parts.size() > 1;
    bool bg = false;

    // inicializando os pipes
    for (unsigned int i = 0; i < parts.size() - 1; i++)
        pipe(fd[i]);

    int used_pipe = 0;

    for (unsigned int i = 0; i < parts.size(); i++)
    {
        string command = parts[i]; // comandos divididos por |
        string in_file = "";
        string out_file = "";
        bool should_append = false;

        // se o ultimo char for um &
        // assumimos que não haverao espacos em branco à direita
        if (command[command.size() - 1] == '&')
        {
            bg = true;
            command.erase(command.end() - 1);
        }

        // processando as strings >> > <
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

        // processando a string do comando atual
        vector<string> arr = split(command);

        if (arr[0] == "cd" || arr[0] == "muda")
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
            historico();
            return;
        }

        int pid = fork();
        if (pid > 0)
            pids[i] = pid;
        else if (pid == 0)
        {
            // foi passado um arquivo para output
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

            // foi passado um arquivo para input
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

            // foram encontrados caracteres de pipe
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

            // processando vector para poder ser passado ao exec
            const char *command_argv[arr.size() + 1];
            for (unsigned int j = 0; j < arr.size(); j++)
            {
                // tratando aliases
                if (ALIASES.count(arr[j]))
                {
                    command_argv[j] = ALIASES[arr[j]].c_str();
                }
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
        // se nao for necessario background, ou se for, mas nao for a ultima parte do pipe
        if (bg && i == pids.size() - 1)
        {
            JOBS[pids[i]] = JOBS_COUNT;
            cout << "Processo em background [" << JOBS[pids[i]] << "]" << endl;
            waitpid(-1, NULL, WNOHANG);
            JOBS_COUNT++;
        }
        else
        {
            waitpid(pids[i], NULL, 0);
        }
    }
}

void read_aliases()
{
    ifstream file(".BRshrc");
    string line;
    while (getline(file, line))
    {

        // dividindo a string
        vector<string> arr = split(line);
        if (arr[0] == "alias")
        {
            // removendo as ""
            string command = split(arr[1], "\"")[0];
            string new_command = split(arr[2], "\"")[0];
            ALIASES[new_command] = command;
        }
    }
    file.close();
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

int main(int argc, char *argv[])
{
    // lemos os aliases antes de tudo
    read_aliases();

    // caso tenhamos que executar um arquivo
    bool should_read_from_file = argc == 2;
    vector<string> file_lines;

    if (should_read_from_file)
    {
        string file_name = string(argv[1]);
        ifstream file(file_name);
        string line;

        while (getline(file, line))
        {
            // se a linha não for um comentário nem for vazia
            if (line[0] != '#' && line.size())
                file_lines.push_back(line);
        }

        file.close();
    }

    // se um arquivo não foi recebido como argumento
    if (!should_read_from_file)
    {
        string command = "";
        print_prompt();
        getline(cin, command);

        while (!(command == "sair"))
        {
            // somente precisamos processar comandos que não sejam espaços em branco
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