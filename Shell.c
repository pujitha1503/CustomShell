#include <stdio.h>
#include <string.h>
#include <stdlib.h>   // exit()
#include <unistd.h>   // fork(), getpid(), exec()
#include <sys/wait.h> // wait()
#include <signal.h>   // signal()
#include <fcntl.h>    // close(), open()

#define WORKING_DIR_SIZE 5000    // current working directory size
#define MAX_TOKENS 15            // Max number of strings in command
#define MAX_COMMAND_LENGTH 150   // Max possible length of any command

// Splits a string into tokens based on spaces and returns an array of tokens.
char **custom_split_input(char *input) {
    int token_index = 0;
    char *parsed_token;
    char **tokens = malloc(MAX_TOKENS * sizeof(char *));
    // Split the string using " " as a separator
    // Get the first token
    parsed_token = strtok(input, " ");
    
    // Continue getting tokens while one of the delimiters is present in tokens[]
    while (parsed_token != NULL) {
        tokens[token_index] = parsed_token;
        token_index++;
        parsed_token = strtok(NULL, " ");
    }

    tokens[token_index] = NULL;
    return tokens;
}

// Function to check if a specific keyword exists in the given command arguments.
int containsToken(char **commandArgs, char *token) {
    char **argumentsIterator = commandArgs;

    // Iterate through all strings in 'commandArgs' and check if a string is equal to the keyword
    while (*argumentsIterator) {
        if (strcmp(*argumentsIterator, token) == 0) {
            return 1; // Keyword found in the command arguments
        }
        argumentsIterator++;
    }

    return 0; // Keyword not found in the command arguments
}

// This function breaks down the input string into separate commands or a single command with arguments, based on separators like '&&', '##', '>', '|', or spaces.
int parseInput(char **parsedArgs) {
    // Check if the command is 'exit'
    if (strcmp(parsedArgs[0], "exit") == 0) {
        return 0; // Exit command
    }

    int isParallel = containsToken(parsedArgs, "&&"); // Check if parsedArgs contains "&&"
    if (isParallel) {
        return 1; // Execute commands in parallel
    }

    int isSequential = containsToken(parsedArgs, "##"); // Check if parsedArgs contains "##"
    if (isSequential) {
        return 2; // Execute commands sequentially
    }

    int isRedirection = containsToken(parsedArgs, ">"); // Check if parsedArgs contains ">"
    if (isRedirection) {
        return 3; // Execute command with output redirection
    }

    int isPipe = containsToken(parsedArgs, "|"); // Check if parsedArgs contains "|"
    if (isPipe) {
        return 4; // Execute command with pipe
    }

    return 5; // Basic single command
}

// Change the current working directory based on the provided path.
void changeWorkingDirectory(char **args) {
    // Change the working directory to the specified path
    const char *path = args[1];
    if (chdir(path) == -1) {
        printf("Shell: Incorrect command\n"); // Print an error message if chdir fails
    }
}

// This function executes a command with the provided 'commandArgs'.
void executeCommand(char **commandArgs) {
    // Check if no command is provided
    if (strlen(commandArgs[0]) == 0) {
        return;
    } 
    else if (strcmp(commandArgs[0], "cd") == 0) { // Handle 'cd' command
        changeWorkingDirectory(commandArgs);
    } else if (strcmp(commandArgs[0], "exit") == 0) { // Handle 'exit' command
        exit(0);
    } else {
        pid_t childPID = fork(); // Fork a child process
        if (childPID < 0) { // Handle fork failure
            printf("Shell: Incorrect command\n");
        } else if (childPID == 0) { // Child process
            // Signal handling
            // Restore default behavior for Ctrl-C and Ctrl-Z
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            if (execvp(commandArgs[0], commandArgs) < 0) { // Execute the command
                printf("Shell: Incorrect command\n");
            }
            exit(0);
        } else { // Parent process
            wait(NULL); // Wait for the child process to complete
            return;
        }
    }
}

// This function executes multiple commands in parallel
void executeParallelCommands(char **commandTokens) {
    // Create a copy of 'commandTokens'
    char **tempTokens = commandTokens;
    // Store all commands to be executed separately
    char *allCommands[MAX_TOKENS][MAX_COMMAND_LENGTH];
    int commandIndex = 0, tokenIndex = 0, commandCount = 1;
    // To store the process ID after forking
    pid_t processID = 1;
    int status; // For waitpid function

    // Traverse through commandTokens and store them in 'tempTokens' to execute commands smoothly
    while (*tempTokens) {
        // If it's not "&&," it is still part of the previous command
        if (*tempTokens && (strcmp(*tempTokens, "&&") != 0)) {
            allCommands[commandIndex][tokenIndex++] = *tempTokens;
        } else {
            // If symbol is "&&," the previous command has ended, so prepare to store a new command
            allCommands[commandIndex++][tokenIndex] = NULL;
            tokenIndex = 0;
            commandCount++;
        }
        // If *tempTokens is not null, there are more commands to process
        if (*tempTokens) {
            *tempTokens++;
        }
    }

    allCommands[commandIndex][tokenIndex] = NULL;
    allCommands[commandIndex + 1][0] = NULL;

    // Traverse through all commands
    // The loop should only execute for the parent process, so we apply the condition pid != 0
    for (int i = 0; i < commandCount && processID != 0; ++i) {
        processID = fork(); // Fork a child process
        if (processID < 0) {  // If fork fails
            printf("Shell: Incorrect command\n");
            exit(1);
        } else if (processID == 0) { // If fork succeeds (child process)
            // Signal handling
            // Restore the default behavior for Ctrl-C and Ctrl-Z signals
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            // Execute the command
            execvp(allCommands[i][0], allCommands[i]);
        }
    }

    // Wait until all fork executions are complete
    while (commandCount > 0) {
        waitpid(-1, &status, WUNTRACED);
        commandCount--;
    }
}

// This function executes multiple commands sequentially
void executeSequentialCommands(char **commandTokens) {
    // Create a copy of 'commandTokens'
    char **tempTokens = commandTokens;

    // Iterate through the command tokens
    while (*tempTokens) {
        char **command = malloc(MAX_TOKENS * sizeof(char *));
        int index = 0; // Index for command
        command[0] = NULL;
        // Prepare storage for command to be executed
        
        // Traverse through tokens and store values in 'command' until encountering "##" or the end
        while (*tempTokens && (strcmp(*tempTokens, "##") != 0)) {
            command[index] = *tempTokens++;
            index++;
        }

        // Execute the command obtained from the above traversal
        executeCommand(command);

        // Check if tempTokens is not null and proceed accordingly
        if (*tempTokens) {
            *tempTokens++;
        }
    }
}

void executeCommandRedirection(char **commandArgs) {
    // this runs one command with output of it redirected to user specified output file
    char **args = commandArgs;

    // For ensuring no problem while executing execvp() function
    args[1] = NULL;

    // when output file name is empty
    if (strlen(commandArgs[2]) == 0) {
        printf("Shell: Incorrect command\n");
    } else {
        pid_t child_pid = fork(); // create a child process
        if (child_pid < 0) {    // forking unsuccessful
            printf("Shell: Incorrect command\n");
            return;
        } else if (child_pid == 0) { // fork successful - child
            // Restoring default behaviour for Ctrl-C and Ctrl-Z
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            // redirecting stdout
            close(STDOUT_FILENO);
            open(commandArgs[2], O_CREAT | O_WRONLY | O_APPEND);
            // executing command
            execvp(args[0], args);
            return;
        } else {
            // Waiting till parent process completes
            wait(NULL);
            return;
        }
    }
}

void executePipes(char **args) {
    int numPipes = 0;
    int i = 0;
    while (args[i]) {
        if (strcmp(args[i], "|") == 0) {
            numPipes++;
        }
        i++;
    }

    // Initialize an array to store file descriptors for pipes
    int pipefds[2 * numPipes];

    for (int j = 0; j < numPipes; j++) {
        if (pipe(pipefds + j * 2) == -1) {
            perror("pipe");
            exit(1);
        }
    }

    int commandStart = 0;
    int commandEnd = 0;
    int pipeIndex = 0;

    for (i = 0; args[i]; i++) {
        if (strcmp(args[i], "|") == 0 || args[i + 1] == NULL) {
            commandEnd = (args[i + 1] == NULL) ? i : i - 1;
            int pid = fork();
            if (pid == 0) {
                // Child process

                // Close write end of the previous pipe
                if (pipeIndex > 0) {
                    close(pipefds[(pipeIndex - 1) * 2 + 1]);
                }

                // Redirect standard input from the previous pipe
                if (commandStart > 0) {
                    dup2(pipefds[(pipeIndex - 1) * 2], STDIN_FILENO);
                }

                // Close read end of the current pipe
                close(pipefds[pipeIndex * 2]);

                // Redirect standard output to the current pipe
                if (args[i + 1] != NULL) {
                    dup2(pipefds[pipeIndex * 2 + 1], STDOUT_FILENO);
                }

                // Execute the command
                char **command = args + commandStart;
                command[commandEnd - commandStart + 1] = NULL;
                execvp(command[0], command);
                perror("execvp");
                exit(1);
            } else if (pid > 0) {
                // Parent process

                // Close the write end of the current pipe
                close(pipefds[pipeIndex * 2 + 1]);

                // Move to the next command
                commandStart = i + 1;
                pipeIndex++;
            } else {
                perror("fork");
                exit(1);
            }
        }
    }

    // Close all pipes
    for (int j = 0; j < numPipes; j++) {
        close(pipefds[j * 2]);
        close(pipefds[j * 2 + 1]);
    }

    // Wait for all child processes to finish
    for (int j = 0; j < numPipes + 1; j++) {
        wait(NULL);
    }
}

int main() {    
    // Ignoring Signals(Ctrl+C,Ctrl+Z)
    signal(SIGTSTP, SIG_IGN);
    signal(SIGINT, SIG_IGN); 
    int cmdType = 0;          // To keep track of the command type to be executed
    size_t inputSize = 0;
    char currentDir[WORKING_DIR_SIZE];  // Variable to store the current directory
    char *userInput = NULL;   // Pointer used by the getline function to read input lines; if it's initially null, the function dynamically allocates memory to store the input line.

    while (1) {
        cmdType = -1;
        char **args = NULL; // variable that store strings separately from command
        // we invoke getcwd to get current directory
        getcwd(currentDir, WORKING_DIR_SIZE);
        printf("%s$", currentDir);

        // by 'getline()' taking input
        getline(&userInput, &inputSize, stdin);
        // "Retrieve the content enclosed by newline ('\n')
        userInput = strsep(&userInput, "\n");
        // if no command is given
        if (strlen(userInput) == 0) {
            continue;
        }

        // Extracting tokens surrounded by delimiter " "
        args = custom_split_input(userInput);

        // in sequential execution when the first command is empty
        if (strcmp(args[0], "##") == 0) {
            continue;
        }

        // Parse the input to check whether the command is a single word or has &&, ##, >, |
        cmdType = parseInput(args);

        if (cmdType == 0) {
            printf("Exiting shell...\n");
            break;
        }
        
        if (cmdType == 1) {
            executeParallelCommands(args); // when the user wants to execute multiple commands in parallel (commands separated by &&)
        }
        
        else if (cmdType == 2) {
            executeSequentialCommands(args); // when the user wants to execute multiple commands in sequential manner (commands separated by ##)
        }
       
        else if (cmdType == 3) {
            executeCommandRedirection(args); // if the user wants to redirect the output of a single command to an output file mentioned
        }
        
        else if (cmdType == 4) {
            executePipes(args); // if the user wants to execute commands with pipes
        }
        
        else {
            executeCommand(args); // when a single command executed
        }
    }
    // Shell over
    return 0;
}
