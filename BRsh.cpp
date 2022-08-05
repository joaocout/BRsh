#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

using namespace std;

// divide a string str, de acordo com o delimitador d, retorna um vector de strings
vector<string> split(string str, char d)
{
    vector<string> result;
    stringstream ss(str);
    string token;

    while (getline(ss, token, d))
    {
        result.push_back(token);
    }
    return result;
}

// retorna TRUE se str contem x, FALSE se não
bool contains(string str, string x)
{
    return str.find(x) != string::npos;
}

int main()
{
    string command = "";

    cout << "Bem-vindo ao BRshell!" << endl;
    getline(cin, command);

    while (!(command == "sair" || command == "exit"))
    {

        vector<string> arr = split(command, ' ');
        const char *command_argv[arr.size() + 1];

        for (unsigned int i = 0; i < arr.size(); i++)
        {
            command_argv[i] = arr[i].c_str();
        }
        command_argv[arr.size()] = NULL;

        if (command.size())
        {

            int pid = fork();
            if (pid == 0)
            {

                int ret = execvp(command_argv[0], (char *const *)command_argv);
                if (ret == -1)
                {
                    cout << "Comando não encontrado" << endl;
                    return 1;
                }
            }

            else
            {
                wait(NULL);
            }
        }

        getline(cin, command);
    }

    return 0;
}