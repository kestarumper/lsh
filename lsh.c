#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>
#include <unistd.h>
#include <signal.h>

#define LSH_BUFSIZE 512
#define LSH_TOKEN_DELIMITERS " \t\r\n"
#define READ_FD 0
#define WRITE_FD 1

char ** lsh_split(char *, char *);

void ctrl_c_handler(int dummy)
{
  printf("\n");
  printf("> ");
  fflush(STDIN_FILENO);
}

int lsh_inbuilt_cd(char ** args) {
  if(args[1] == NULL) {
    fprintf(stderr, "lsh: cd expected a parameter\n");
  } else {
    if(chdir(args[1]) != 0) {
      perror("lsh");
    }
  }
  return EXIT_SUCCESS;
}

int lsh_inbuilt_exit(char ** args) {
  exit(EXIT_SUCCESS);
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

pid_t spawn_process(char ** args, int in, int out)
{
  pid_t pid;
  int status;

  pid = fork();
  if(pid == 0) {
  // fprintf(stdout, "%s: IN[%i] --> OUT[%i]\n", args[0], in, out);
    if(in != READ_FD) {
      dup2(in, STDIN_FILENO);
      close(in);
    }
    if(out != WRITE_FD) {
      dup2(out, STDOUT_FILENO);
      close(out);
    }

    if(execvp(args[0], args) == -1) {
      fprintf(stderr, "lsh:spawn:execvp = %s\n", args[0]);
    }

    exit(EXIT_FAILURE);
  }

  return pid;
}

int lsh_launch(char ** commands, int n, int run_in_bg)
{
  char ** com_args;

  pid_t pid;
  pid_t wpid;
  int status = EXIT_SUCCESS;

  int i = 0;

  // com_args = lsh_split(commands[0], LSH_TOKEN_DELIMITERS);


  int fd[2];
  int in = READ_FD;
  for (i = 0; i < n - 1; ++i)
  {
    com_args = lsh_split(commands[i], LSH_TOKEN_DELIMITERS);

    if(pipe(fd)) {
      perror("lsh:launch:pipe");
      return 1;
    }

    // printf("%i zapisuje do %i\n", fd[WRITE_FD], fd[READ_FD]);

    pid = spawn_process(com_args, in, fd[WRITE_FD]);
    close(fd[WRITE_FD]);

    in = fd[READ_FD];
  }

  // Kod ktory zabral mi 8h mojego zycia
  // dup2 byl wykonywany rowniez w funkcji spawn_process
  // co prowadzilo do... nie wiem czego, ale chyba zamykalo pipe'a
  // z ktorego potem szly same EOF'y
  // if (in != READ_FD) {
  //   dup2(in, STDIN_FILENO);
  // }

  com_args = lsh_split(commands[i], LSH_TOKEN_DELIMITERS);


  // Check if not empty
  if (com_args[0] == NULL) {
    // An empty command was entered.
    return EXIT_SUCCESS;
  }

  for (i = 0; i < lsh_inbuilt_functions_size; i++) {
    if (strcmp(com_args[0], lsh_inbuilt_names[i]) == 0) {
      return (*lsh_inbuilt_functions[i])(com_args);
    }
  }
  
  pid = spawn_process(com_args, in, WRITE_FD);

  // wait (or dont) for process
  if(!run_in_bg) {
    do {
      wpid = waitpid(pid, &status, WUNTRACED);
    } while(!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
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
      printf("\nBye :)\n");
      exit(EXIT_SUCCESS);
    } else
     if(c == '\n' || c == EOF) {
      buffer[position] = '\0';
      return buffer;
    } else {
      buffer[position] = c;
    }

    position++;
  }
}

char ** lsh_split(char * line, char * delims)
{
  int bufsize = LSH_BUFSIZE;
  char * token;
  char ** tokens = malloc(sizeof(char*) * bufsize);
  char ** tokens_temp;

  if(!tokens) {
    perror("lsh:split:malloc");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, delims);
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

    token = strtok(NULL, delims);
  }
  // add NULL at the end to signal last element
  tokens[pos] = NULL;

  return tokens;
}

// ls -l / | head | sort
char ** lsh_pipes(char * line) 
{
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
  char ** commands;
  int status;
  int comnum;
  int run_in_bg;
  char * ampersand_pos = NULL;

  do {
    run_in_bg = 0;

    printf("> ");

    line = lsh_readline();

    if(ampersand_pos = strchr(line, '&')) {
      *ampersand_pos = '\0';
      run_in_bg = 1;
    }

    commands = lsh_pipes(line);

    for(comnum = 0; commands[comnum] != NULL; comnum++) {
      // printf("#%i %s\n", comnum, commands[comnum]);
    }

    status = lsh_launch(commands, comnum, run_in_bg);

  } while(1);
}

int main() 
{
  printf("LSH Alpha\n");

  signal(SIGINT, ctrl_c_handler);

  lsh_loop();

  return 0;
}