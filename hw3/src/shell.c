#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <errno.h>
#include "chat.h"
#include <mysql/mysql.h>

#define MAX_CMD_LEN 256
#define MAX_INPUT_LEN 5000
#define NPIPE_SIZE 1024

// --- MySQL 連線設定 ---
#define DB_HOST "localhost"
#define DB_USER "hw3_user"          
#define DB_PASS "kenny179910" 
#define DB_NAME "hw3_db"        

// 全域變數，讓 Signal Handler 讀取公佈欄
ChatState *global_chat_state = NULL;

// --- 處理子行程死亡的 Signal Handler (自動收屍) ---
void sigchld_handler(int sig) {
    pid_t dead_pid;
    int status;
    while ((dead_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (global_chat_state != NULL) {
            for (int i = 0; i < MAX_USERS; i++) {
                if (global_chat_state->users[i].is_active && global_chat_state->users[i].pid == dead_pid) {
                    char log_msg[100];
                    int len = sprintf(log_msg, "[Server Log] User %d (%s) left. Clearing seat.\n", 
                                      global_chat_state->users[i].id, 
                                      global_chat_state->users[i].name);
                    if (write(STDOUT_FILENO, log_msg, len) < 0) { }
                    global_chat_state->users[i].is_active = 0;
                    break;
                }
            }
        }
    }
}

// --- 處理別人傳來訊息的 Signal Handler (門鈴郵差) ---
void sigusr1_handler(int sig) {
    if (global_chat_state != NULL) {
        for (int i = 0; i < MAX_USERS; i++) {
            if (global_chat_state->users[i].is_active && global_chat_state->users[i].pid == getpid()) {
                if (write(STDOUT_FILENO, global_chat_state->users[i].msg_buffer, strlen(global_chat_state->users[i].msg_buffer)) < 0) { }
                global_chat_state->users[i].msg_buffer[0] = '\0';
                break;
            }
        }
    }
}

// --- 處理內建指令 ---
int handle_builtin(char **args) {
    if (args[0] == NULL) return 1;
    if (strcmp(args[0], "quit") == 0) {
        exit(0);
    } else if (strcmp(args[0], "setenv") == 0) {
        if (args[1] && args[2]) setenv(args[1], args[2], 1);
        return 1;
    } else if (strcmp(args[0], "printenv") == 0) {
        if (args[1]) {
            char *val = getenv(args[1]);
            if (val) printf("%s\n", val);
        }
        return 1;
    }
    return 0;
}

// --- 處理登入與註冊流程 ---
// 回傳值：將登入成功的 username 存入 logged_in_username 陣列
void handle_login(int client_fd, char *logged_in_username) {
    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL) {
        fprintf(stderr, "mysql_init() failed\n");
        exit(1);
    }
    // 連線資料庫
    if (mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0) == NULL) {
        fprintf(stderr, "mysql_real_connect() failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        exit(1);
    }

    char input_buf[256];
    char username[MAX_NAME_LEN];
    char password[256];
    char query[1024];

    while (1) {
        if (write(client_fd, "Login: ", 7) < 0) exit(0);
        int n = read(client_fd, input_buf, sizeof(input_buf) - 1);
        if (n <= 0) exit(0); // 斷線
        input_buf[n] = '\0';
        input_buf[strcspn(input_buf, "\r\n")] = 0;
        strcpy(username, input_buf);

        if (write(client_fd, "Password: ", 10) < 0) exit(0);
        n = read(client_fd, input_buf, sizeof(input_buf) - 1);
        if (n <= 0) exit(0);
        input_buf[n] = '\0';
        input_buf[strcspn(input_buf, "\r\n")] = 0;
        strcpy(password, input_buf);

        sprintf(query, "SELECT password FROM Users WHERE username='%s'", username);
        if (mysql_query(conn, query)) continue; 

        MYSQL_RES *res = mysql_store_result(conn);
        if (res == NULL) continue;

        int num_rows = mysql_num_rows(res);
        
        if (num_rows > 0) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (strcmp(row[0], password) == 0) {
                // 登入成功
                strcpy(logged_in_username, username);
                mysql_free_result(res);
                break; 
            } else {
                if (write(client_fd, "Password error !\n", 17) < 0) exit(0);
            }
        } else {
            if (write(client_fd, "User not found !\n", 17) < 0) exit(0);
            if (write(client_fd, "Create account or login again ? <1/2> : ", 40) < 0) exit(0);
            
            
            n = read(client_fd, input_buf, sizeof(input_buf) - 1);
            if (n <= 0) exit(0);
            input_buf[n] = '\0';
            input_buf[strcspn(input_buf, "\r\n")] = 0;

            if (strcmp(input_buf, "1") == 0) {
                while(1) {
                    if (write(client_fd, "your user name: ", 16) < 0) exit(0);
                    n = read(client_fd, input_buf, sizeof(input_buf) - 1);
                    if (n <= 0) exit(0);
                    input_buf[n] = '\0';
                    input_buf[strcspn(input_buf, "\r\n")] = 0;
                    char new_user[MAX_NAME_LEN];
                    strcpy(new_user, input_buf);

                    if (write(client_fd, "your password: ", 15) < 0) exit(0);
                    n = read(client_fd, input_buf, sizeof(input_buf) - 1);
                    if (n <= 0) exit(0);
                    input_buf[n] = '\0';
                    input_buf[strcspn(input_buf, "\r\n")] = 0;
                    char new_pass[256];
                    strcpy(new_pass, input_buf);

                    sprintf(query, "SELECT * FROM Users WHERE username='%s'", new_user);
                    mysql_query(conn, query);
                    MYSQL_RES *check_res = mysql_store_result(conn);
                    if (mysql_num_rows(check_res) > 0) {
                        if (write(client_fd, "User name already exist !\n", 26) < 0) exit(0);
                        mysql_free_result(check_res);
                        continue; 
                    }
                    mysql_free_result(check_res);

                    sprintf(query, "INSERT INTO Users (username, password) VALUES ('%s', '%s')", new_user, new_pass);
                    if (mysql_query(conn, query) == 0) {
                         if (write(client_fd, "Create success !\n", 17) < 0) exit(0);
                         break; 
                    } else {
                         if (write(client_fd, "Create failed.\n", 15) < 0) exit(0);
                         break;
                    }
                }
            }
        }
        mysql_free_result(res);
    }
    mysql_close(conn);
}

int main(int argc, char *argv[]) {
    int port = 8888;
    if (argc == 2) port = atoi(argv[1]);

    setenv("PATH", "bin:.", 1);

    int master_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (master_socket < 0) { perror("socket failed"); exit(EXIT_FAILURE); }

    int opt = 1;
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed"); exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(port);       

    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed"); exit(EXIT_FAILURE);
    }

    if (listen(master_socket, 5) < 0) {
        perror("listen failed"); exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", port);
    
    // 建立共享記憶體 (公佈欄)
    int shm_id = shmget(SHM_KEY, sizeof(ChatState), IPC_CREAT | 0666);
    if (shm_id < 0) { perror("shmget failed"); exit(EXIT_FAILURE); }
    ChatState *chat_state = (ChatState *)shmat(shm_id, NULL, 0);
    global_chat_state = chat_state; 
    
    for (int i = 0; i < MAX_USERS; i++) {
        chat_state->users[i].is_active = 0;
    }

    // 註冊 SIGCHLD 訊號處理 (即時收屍)
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; 
    if (sigaction(SIGCHLD, &sa, NULL) == -1) { perror("sigaction"); exit(1); }
    
    while (1) {
        int client_socket;
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        
        client_socket = accept(master_socket, (struct sockaddr *)&client_addr, &addrlen);
        if (client_socket < 0) continue; 
        
        int user_idx = -1;
        for (int i = 0; i < MAX_USERS; i++) {
            if (chat_state->users[i].is_active == 0) {
                user_idx = i; 
                break;
            }
        }
        
        if (user_idx == -1) {
            printf("[Server Log] Chat room is full. Connection rejected.\n");
            char *msg = "Sorry, chat room is full.\n";
            if (write(client_socket, msg, strlen(msg)) < 0) { }
            close(client_socket);
            continue;
        }
        
        int user_id = user_idx + 1; 
        char ip_port[50];
        sprintf(ip_port, "%s:%d", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        printf("[Server Log] User %d connected from %s\n", user_id, ip_port);

        pid_t pid = fork();
        
        if (pid < 0) {
            perror("fork failed"); close(client_socket);
        } else if (pid == 0) {
            // --- 服務生子行程 ---
            close(master_socket); 

            char logged_in_username[MAX_NAME_LEN];
            
            // 呼叫登入流程 (會卡在這裡直到成功登入)
            handle_login(client_socket, logged_in_username);
            
            // --- 登入成功後，再進行 I/O 導向 ---
            dup2(client_socket, STDIN_FILENO);  
            dup2(client_socket, STDOUT_FILENO); 
            dup2(client_socket, STDERR_FILENO); 
            close(client_socket);               

            // 將環境變數寫入
            char id_str[10];
            sprintf(id_str, "%d", user_id);
            setenv("MY_USER_ID", id_str, 1);
            setenv("MY_USERNAME", logged_in_username, 1);

            // 去公佈欄更新自己的名字
            if (global_chat_state != NULL) {
                 for (int i = 0; i < MAX_USERS; i++) {
                     if (global_chat_state->users[i].is_active && global_chat_state->users[i].pid == getpid()) {
                         snprintf(global_chat_state->users[i].name, MAX_NAME_LEN, "%s", logged_in_username);
                         break;
                     }
                 }
            }

            // 註冊 SIGUSR1，準備接收別人傳來的訊息
            struct sigaction sa_usr;
            sa_usr.sa_handler = sigusr1_handler;
            sigemptyset(&sa_usr.sa_mask);
            sa_usr.sa_flags = 0; 
            sigaction(SIGUSR1, &sa_usr, NULL);
            
            int npipe_table[NPIPE_SIZE][2];
            int line_count = 0;
            for (int i = 0; i < NPIPE_SIZE; i++) {
                npipe_table[i][0] = -1;
                npipe_table[i][1] = -1;
            }

            char input[MAX_INPUT_LEN];

            while (1) {
                while (waitpid(-1, NULL, WNOHANG) > 0);

                // 印出包含帳號的 prompt
                printf("%s%% ", logged_in_username);
                fflush(stdout);

                if (!fgets(input, sizeof(input), stdin)) {
                    if (errno == EINTR) {
                        clearerr(stdin); 
                        continue;        
                    }
                    break; 
                } 

                input[strcspn(input, "\r\n")] = 0;

                char *args[100];
                int argc = 0;
                char *token = strtok(input, " ");
                while (token != NULL) {
                    args[argc++] = token;
                    token = strtok(NULL, " ");
                }
                args[argc] = NULL;

                if (argc == 0) continue;

                line_count++;
                int cmd_start = 0;
                int prev_fd = -1;
                int current_line_idx = line_count % NPIPE_SIZE;
                
                if (npipe_table[current_line_idx][0] != -1) {
                    prev_fd = npipe_table[current_line_idx][0];
                    if (npipe_table[current_line_idx][1] != -1) {
                        close(npipe_table[current_line_idx][1]);
                        npipe_table[current_line_idx][1] = -1;
                    }
                }

                int unknown_cmd_occurred = 0;
                pid_t pids[100]; 
                int pid_count = 0;

                for (int i = 0; i <= argc; i++) {
                    int is_last_cmd = (i == argc);  
                    int is_numbered_pipe = 0;       
                    int n_pipe_target = 0;          
                    
                    if (is_last_cmd || strcmp(args[i], "|") == 0 || (args[i][0] == '|' && strlen(args[i]) > 1)) {
                        if (!is_last_cmd && args[i][0] == '|' && strlen(args[i]) > 1) {
                            is_numbered_pipe = 1;
                            n_pipe_target = atoi(&args[i][1]);
                        }

                        args[i] = NULL;  
                        char **current_cmd = &args[cmd_start];  
                        if (current_cmd[0] == NULL) break;
                        
                        if (handle_builtin(current_cmd)) {
                            cmd_start = i + 1; continue; 
                        }

                        int fd[2];
                        if (!is_last_cmd && !is_numbered_pipe) {
                            if (pipe(fd) < 0) perror("pipe error");
                        }
                        
                        if (is_numbered_pipe) {
                            int target_idx = (line_count + n_pipe_target) % NPIPE_SIZE;
                            if (npipe_table[target_idx][0] == -1) {
                                if (pipe(npipe_table[target_idx]) < 0) perror("pipe error");
                            }
                        }

                        pid_t cmd_pid = fork();
                        if (cmd_pid < 0) {
                            perror("fork error");
                        } else if (cmd_pid == 0) {
                            if (prev_fd != -1) {
                                dup2(prev_fd, STDIN_FILENO); close(prev_fd);
                            }
                            if (is_numbered_pipe) {
                                int target_idx = (line_count + n_pipe_target) % NPIPE_SIZE;
                                dup2(npipe_table[target_idx][1], STDOUT_FILENO);
                                for(int k=0; k<NPIPE_SIZE; k++){
                                    if(npipe_table[k][0] != -1) close(npipe_table[k][0]);
                                    if(npipe_table[k][1] != -1) close(npipe_table[k][1]);
                                }
                            } else if (!is_last_cmd) {
                                dup2(fd[1], STDOUT_FILENO);
                                close(fd[0]); close(fd[1]);
                            }
                            
                            if (execvp(current_cmd[0], current_cmd) == -1) {
                                fprintf(stderr, "Unknown command: [%s].\n", current_cmd[0]);
                                exit(233); 
                            }
                        } else {
                            pids[pid_count++] = cmd_pid; 
                            if (prev_fd != -1) close(prev_fd); 
                            if (!is_last_cmd && !is_numbered_pipe) {
                                close(fd[1]); prev_fd = fd[0]; 
                            }
                        }
                        cmd_start = i + 1; 
                    }
                }
                
                if (npipe_table[current_line_idx][0] != -1) npipe_table[current_line_idx][0] = -1;

                int wait_for_foreground = 1;
                if (argc > 0 && args[argc-1] != NULL && args[argc-1][0] == '|' && strlen(args[argc-1]) > 1) {
                     wait_for_foreground = 0;
                }

                if (wait_for_foreground) {
                    for (int i = 0; i < pid_count; i++) {
                        int status;
                        waitpid(pids[i], &status, 0);
                        if (WIFEXITED(status) && WEXITSTATUS(status) == 233) {
                            unknown_cmd_occurred = 1;
                        }
                    }
                }

                if (unknown_cmd_occurred) line_count--; 
            } 
            exit(0); 

        } else {
            // --- 櫃台父行程 --- 
            chat_state->users[user_idx].id = user_id;
            chat_state->users[user_idx].is_active = 1;
            chat_state->users[user_idx].pid = pid; 
            strcpy(chat_state->users[user_idx].ip_port, ip_port);
            
            close(client_socket); 
        }
    } 
    return 0;
}
