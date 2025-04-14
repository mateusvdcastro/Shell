#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

void init_operands(char *command, char *argv[]);
int find_operand(char *command);
void execute_command(char *command);
void pipe_(char *command);
void execute_background(char *comando);
int exec_command(char *command, char *argv[]);
void or_(char *command, char *argv[]);
void and_(char *command, char *argv[]);
void read_command(char *command, char *argv[]);


int main(int argc, char *argv[]){
	int status;
	pid_t pid;
	char *command;

	while(1){

		printf("myshell> ");

		command = (char *)malloc(sizeof(char)*100);
    
		read_command(command, argv);

    if(argv[0] != NULL){ //Adicionado para casos de comando com operadores

      //Verifica se o comando digitado é "exit". Se for, o loop é interrompido e o programa termina.
      if(strcmp(argv[0], "exit") == 0){
        break;
      }
		}
	
    free(command);
	}

	printf("Obrigado por visitar a minha shell : ) \n");
	free(command);
	return 0;    

}

void read_command(char *command, char *argv[]){
		// (Re)inicialize argv com NULL
		for (int i = 0; i < 100; i++) {
				argv[i] = NULL;
		}

		if (fgets(command, 100, stdin) == NULL) {
				printf("Error reading command\n");
		}
		command[strlen(command)-1] = '\0';

		// Verifica se há operadores condicionais na entrada
		int operand = find_operand(command);
		if (operand != -1) {
				init_operands(command, argv); // Chama apenas se houver operadores
				return; // Retorna após tratar os operadores
		}
		
		exec_command(command, argv);
		return;
	}


// Encontra o operador e retorna um número equivalente ao operador
int find_operand(char *command){
		int i = 0;
		while(command[i] != '\0'){
				if(command[i] == '|' && command [i+1]!='|' && command [0]!= '|'){ //pipe
					pipe_(command);
						return -2;

				} if(command[i] == '|' && command [i+1]=='|' && command [0]!= '|'){ //OU
							return -3 ;
					}  

				if(command[i] == '&' && strlen(command) - 1 == i && command [0]!= '&'){ //background
						return -4;
				}
			if(command[i] == '&' && command [i+1]=='&' && command [0]!= '&'){ //E
					return -5 ;
			}  
				i++;
		}

		return -1;
}


//chama a função de cada operador
void init_operands(char *command, char *argv[]){

		int operand = find_operand(command);
		int background = 0;

		// Verifica se é pipe
		if (operand == -2){
				pipe_(command);
				return;
		}

		// Verifica se é OU (||)
		if (operand == -3){
				or_(command, argv);
				return;
		}

	// Verifica se é & 
	if (operand == -4){ 
			execute_background(command);
			return;
	}

	// Verifica se é E (&&)
	if (operand == -5){ 
			and_(command, argv);
			return;
	}
}


void pipe_(char *command) {

    int fd_in = STDIN_FILENO;
    int fd_out = STDOUT_FILENO;

    //printf("Comando original: %s\n", command);
    char *commands[100]; // Array de ponteiros para armazenar os comandos
    int num_commands = 0; // Número de comandos
    char *token = strtok(command, "|"); // extrai o primeiro comando
    //printf("token: %s\n", token);


    while (token != NULL) { 
        commands[num_commands++] = token; // Armazena o token extraido no array de ponteiros e aumenta o numero de comandos
        token = strtok(NULL, "|"); // strtok continua a partir do ponto onde parou na última chamada
    }

    int pipes[num_commands - 1][2]; // matriz de inteiro de pipes, -1 por que o ultimo comando não precisa de um pipe, e duas colunas, para a extremidade de leitura/escrita

    // Criação dos pipes, estamos solicitando ao sistema operacional que crie um novo pipe e nos retorne os descritores de arquivo associados a ele. Porém, nesse momento, o pipe é apenas um canal de comunicação vazio, sem dados.
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    //printf("Pipes criados com sucesso.\n");

  // cria um processo filho para cada comando na sequência
    for (int i = 0; i < num_commands; i++) {
        pid_t pid;
        pid = fork();

        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) { // Código do filho
          // Configurar entrada e saída
          if (i == 0) // Se for o primeiro comando
              dup2(pipes[i][1], STDOUT_FILENO); // Redirecionar saída para o pipe
          else if (i == num_commands - 1) // Se for o último comando
              dup2(pipes[i - 1][0], STDIN_FILENO); // Redirecionar entrada para o pipe
          else {
              dup2(pipes[i][1], STDOUT_FILENO); // Redirecionar saída para o pipe
              dup2(pipes[i - 1][0], STDIN_FILENO); // Redirecionar entrada para o pipe anterior
          }
            // Fechar todos os pipes
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }


          char *command_args[100];
          char *commands_with_quotes[100];
          char *token_aux;
          char *token = strtok(commands[i], " ");
          int arg_index = 0;
          int in_quotes = 0; // Flag para indicar se estamos dentro de aspas duplas

          while (token != NULL) {
              // Verifica se o token começa com aspas duplas
              if (token[0] == '"') {
                  // Remove as aspas duplas do início
                  memmove(token, token + 1, strlen(token));

                  // Verifica se as aspas duplas estão no mesmo token
                  if (token[strlen(token) - 1] == '"') {
                      token[strlen(token) - 1] = '\0'; // Remove as aspas duplas do final

                      command_args[arg_index++] = token; // Adiciona o token sem as aspas ao array
                  } else {
                      // Se as aspas duplas não estiverem no mesmo token, marcamos que estamos dentro de aspas
                      in_quotes = 1;
                      command_args[arg_index++] = token;

                      token = strtok(NULL, " ");
                  }
              } else if (in_quotes) {
                  // Se estamos dentro de aspas, concatenamos o token com o anterior
                  printf("token em quote: %s\n", token);
                  strcat(command_args[arg_index - 1], " "); // Adiciona um espaço
                  strcat(command_args[arg_index - 1], token); // Concatena o token
                  // Verifica se este token termina com aspas duplas
                  if (token[strlen(token) - 1] == '"') {
                      in_quotes = 0; // Marcamos que saímos de aspas
                  }
              } else {
                  // Se não estamos dentro de aspas, basta adicionar o token
                  command_args[arg_index++] = token;
              }
              token = strtok(NULL, " ");
          }

          command_args[arg_index] = NULL;

          execvp(command_args[0], command_args);
          perror("execvp");
          exit(EXIT_FAILURE);
        }
    }

    // Processo pai
    // Fechar todos os pipes (evitar vazamentos de recursos)
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    // Esperar pelos filhos
    for (int i = 0; i < num_commands; i++) {
        //printf("Esperando pelo processo filho [%d].\n", i);
        wait(NULL);
    }
}

// Executa o processo de background
void execute_background(char *comando) {
		char *arg[100];
		char *token;
		int num_arg = 0;
	// Remove o '&' do final do comando que rodará em background
		comando[strlen(comando) - 1] = '\0';

		// Divide o comando em argumentos
		token = strtok(comando, " ");
		while (token != NULL) {
			arg[num_arg++] = token;
			token = strtok(NULL, " ");
		} 
		arg[num_arg] = NULL;

		pid_t pid;
		pid = fork(); //cria o processo que vai ser executado em background

	if (pid == 0) {   // Processo filho
			execvp(arg[0], arg);
			perror("execvp");
			exit(EXIT_FAILURE);
	} else if (pid < 0) { // Erro ao criar processo filho
			perror("fork");
	} else {     //processo pai
		/* Caso o comando deva ser efetuado em background
		o parametro WNOHANG faz com que o pai nao fique preso
		esperando o filho terminar. Da mesma forma, o filho nao vira
		zumbi, ja que a conexao entre pai e filho se mantem */
		waitpid(-1, NULL, WNOHANG);
		printf("Processo %d esta executando em background\n", pid);
	}
	return; 
}

// Função para executar um comando usando fork e exec
int exec_command(char *command, char *argv[]) {
	pid_t pid;
	char *token;
	int status;
	//char *args[100];
	
	int num_args = 0;
	token = strtok(command, " ");
	while(token != NULL){
			argv[num_args] = token;
			num_args++;
			token = strtok(NULL, " ");
	}
	argv[num_args] = NULL;

	// Cria um processo filho
	pid =fork();
	if (pid < 0) { //erro no fork
			perror("fork");
			exit(1);
	}
	if (pid != 0){
		// Código do processo pai, suspenso esperando o filho terminar 
		waitpid(pid, &status, 0);
	} else {
			/* Código do processo filho */
			execvp(argv[0], argv); // substitui o programa atual pelo comando fornecido pelo usuário
			perror("execvp");
			exit(1);
	} 
	return status;
}

// Função para executar uma operação lógica 'OU' entre dois comandos
void or_(char *command, char *argv[]){

	// Faz uma cópia do comando original
	char *copia_command = strdup(command);

	// Divide o comando em tokens usando "||"
	char *token1 = strtok(command, "||");
	char *token2 = strtok(NULL, "&|");
  
	int tamanho_token = strlen(token2);

	// Declara uma variável para armazenar o resultado dos comandos
	char *resultado;

	// Verifica se o primeiro comando falha
	if (exec_command(token1, argv) != 0) {
		// Verifica se o segundo comando falha
		if (exec_command(token2, argv) != 0){
			resultado = "false ";
		}else{
			resultado = "true ";
		}
	}else{
		resultado = "true ";
	}

	// Encontra a posição do segundo token na cópia do comando original
	char *pos = strstr(copia_command, token2);

	// Aloca memória para o novo comando e o resto do comando
	char *novoComando = (char *)malloc(strlen(resultado) + strlen(pos + tamanho_token) + 1);
	// Verifica se a alocação de memória foi bem-sucedida
	if (novoComando == NULL) {
			printf("Erro ao alocar memória.\n");
			// Libera a memória alocada para a cópia do comando original
			free(copia_command);
			return;
	}

	// Constrói o novo comando
	strcpy(novoComando, resultado);
	strcat(novoComando, pos + tamanho_token);

	// Atualiza o comando original com o novo comando
	command = novoComando;
	// Inicializa os operandos do novo comando
	init_operands(command, argv);

	// Libera a memória alocada para a cópia do comando original e o novo comando
	free(copia_command);
	free(novoComando);
}

// Função para executar uma operação lógica 'E' entre dois comandos
void and_(char *command, char *argv[]) {

	// Faz uma cópia do comando original
	char *copia_command = strdup(command);

	// Divide o comando em tokens usando "&&"
	char *token1 = strtok(command, "&&");
	char *token2 = strtok(NULL, "&|");
	int tamanho_token = strlen(token2);

	// Declara uma variável para armazenar o resultado dos comandos
	char *resultado;

	// Verifica se o primeiro comando é bem-sucedido
	if (exec_command(token1, argv) == 0) {
		// Verifica se o segundo comando é bem-sucedido
		if (exec_command(token2, argv) == 0) {
			resultado = "true ";
		} else {
			resultado = "false ";
		}
	} else {
		// Se o primeiro comando falhou
		resultado = "false ";
	}

	// Encontra a posição do segundo token na cópia do comando original
	char *pos = strstr(copia_command, token2);

	// Aloca memória para o novo comando e o resto do comando
	char *novoComando = (char *)malloc(strlen(resultado) + strlen(pos + tamanho_token) + 1);
	// Verifica se a alocação de memória foi bem-sucedida
	if (novoComando == NULL) {
			printf("Erro ao alocar memória.\n");
			// Libera a memória alocada para a cópia do comando original
			free(copia_command);
			return;
	}

	// Constrói o novo comando
	strcpy(novoComando, resultado);
	strcat(novoComando, pos + tamanho_token);

	// Atualiza o comando original com o novo comando
	command = novoComando;
	// Inicializa os operandos do novo comando
	init_operands(command, argv);

	// Libera a memória alocada para a cópia do comando original e o novo comando
	free(copia_command);
	free(novoComando);
}




