#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>
#include <unistd.h>
#include <signal.h>

#define LSH_BUFSIZE 512
#define LSH_TOKEN_DELIMITERS " \t\r\n"

void ctrl_c_handler(int dummy)
{
  printf("\n");
}

int lsh_inbuilt_cd(char ** args) {
  if(args[1] == NULL) {
    fprintf(stderr, "lsh: cd expected a parameter\n");
  } else {
    if(chdir(args[1]) != 0) {
      perror("lsh");
    }
  }
  return 1;
}

int lsh_inbuilt_exit(char ** args) {
  return EXIT_SUCCESS;
}

int (*lsh_inbuilt_functions[])(char **) = {
  &lsh_inbuilt_cd,
  &lsh_inbuilt_exit
};

char * lsh_inbuilt_names[] = {
  "cd",
  "exit"
};

int lsh_inbuilt_functions_size = (sizeof(lsh_inbuilt_names) / sizeof(char*));

int lsh_exe(char ** args)
{
  pid_t pid;
  pid_t wpid;
  int status;
  int run_in_bg = 0;

  int argc = 0;
  for(; args[argc] != NULL; argc++) {
    if(strcmp(args[argc], "&") == 0) {
      args[argc] = NULL;
      run_in_bg = 1;
      break;
    }
  }

  pid = fork();

  if(run_in_bg) {
    if(pid == 0) {
      if(!fork()) {
        // SECOND CHILD PROCESS
        printf("+[PID: %i] %s\n", getpid(), args[0]);
        if(execvp(args[0], args) == -1) {
          perror("lsh:exe:execvp");
        }
        exit(EXIT_FAILURE);
      }
      // First child exits immediately
      exit(EXIT_SUCCESS);
    } else {
      do {
        wpid = waitpid(pid, &status, WUNTRACED);
      } while(!WIFEXITED(status) && !WIFSIGNALED(status));
    }
  } else {
    if(pid == 0) {
      // CHILD PROCESS
      if(execvp(args[0], args) == -1) {
        perror("lsh:exe:execvp");
      }
      exit(EXIT_FAILURE);
    } else if(pid < 0) {
      perror("lsh:exe:fork");
      exit(EXIT_FAILURE);
    } else {
      do {
        wpid = waitpid(pid, &status, WUNTRACED);
      } while(!WIFEXITED(status) && !WIFSIGNALED(status));
    }
  }

  return 1;
}

int lsh_launch(char ** args)
{
  int i;

  if (args[0] == NULL) {
    // An empty command was entered.
    return 1;
  }

  for (i = 0; i < lsh_inbuilt_functions_size; i++) {
    if (strcmp(args[0], lsh_inbuilt_names[i]) == 0) {
      return (*lsh_inbuilt_functions[i])(args);
    }
  }

  

  return lsh_exe(args);
}

char * lsh_readline()
{
  int bufsize = LSH_BUFSIZE;
  char * buffer = malloc(sizeof(char) * bufsize);
  int c;
  int position = 0;

  if(!buffer) {
    perror("lsh:readline:malloc");
    exit(EXIT_FAILURE);
  }

  while(1) {
    c = getchar();

    if(position == 0 && c == EOF) {
      printf("\n");
      exit(EXIT_SUCCESS);
    } else if(c == '\n' || c == EOF) {
      buffer[position] = '\0';
      return buffer;
    } else {
      buffer[position] = c;
    }

    position++;
  }
}

char ** lsh_split(char * line)
{
  int bufsize = LSH_BUFSIZE;
  char * token;
  char ** tokens = malloc(sizeof(char*) * bufsize);
  char ** tokens_temp;

  if(!tokens) {
    perror("lsh:split:malloc");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, LSH_TOKEN_DELIMITERS);
  int pos = 0;
  while(token != NULL) {
    tokens[pos] = token;
    pos++;

    if(pos >= bufsize) {
      tokens_temp = tokens;
      bufsize += LSH_BUFSIZE;
      tokens = realloc(tokens, sizeof(char*) * bufsize);

      if(!tokens) {
        perror("lsh:split:realloc");
        free(tokens_temp);
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, LSH_TOKEN_DELIMITERS);
  }
  // add NULL at the end to signal last element
  tokens[pos] = NULL;

  return tokens;
}


// ls -l / | head | sort
char ** lsh_pipes(char * line) {
  int bufsize = LSH_BUFSIZE;
  char * token;
  char ** tokens = malloc(sizeof(char*) * bufsize);
  char ** tokens_temp;

  if(!tokens) {
    perror("lsh:pipes:malloc");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, "|");
  int pos = 0;
  while(token != NULL) {
    tokens[pos] = token;
    pos++;

    if(pos >= bufsize) {
      tokens_temp = tokens;
      bufsize += LSH_BUFSIZE;
      tokens = realloc(tokens, sizeof(char*) * bufsize);

      if(!tokens) {
        perror("lsh:pipes:realloc");
        free(tokens_temp);
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, "|");
  }
  // add NULL at the end to signal last element
  tokens[pos] = NULL;

  return tokens;
}

void lsh_loop()
{
  char * line;
  char ** args;
  int status;

  do {
    printf("> ");

    line = lsh_readline();
    args = lsh_split(line);
    status = lsh_launch(args);

  } while(status);
}

int main() 
{
  printf("LSH Shell\n");

  signal(SIGINT, ctrl_c_handler);

  lsh_loop();

  return 0;
}