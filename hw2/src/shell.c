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

#define MAX_CMD_LEN 256
#define MAX_INPUT_LEN 5000
#define NPIPE_SIZE 1024

// 宣告一個全域變數指標，讓 Signal Handler 也能讀到公佈欄
ChatState *global_chat_state = NULL;

// --- 新增：處理子行程死亡的 Signal Handler ---
void sigchld_handler(int sig) {
    pid_t dead_pid;
    int status;
    // WNOHANG 代表不卡住，有死掉的子行程就抓出來
    while ((dead_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (global_chat_state != NULL) {
            // 掃描公佈欄，找出是哪個客人的服務生死掉了
            for (int i = 0; i < MAX_USERS; i++) {
                if (global_chat_state->users[i].is_active && global_chat_state->users[i].pid == dead_pid) {
                    // 使用 write 代替 printf，因為在 signal handler 裡用 printf 可能會發生死結
                    char log_msg[100];
                    int len = sprintf(log_msg, "[Server Log] User %d left. Clearing seat.\n", global_chat_state->users[i].id);
                    if (write(STDOUT_FILENO, log_msg, len) < 0) {
                        // 就算寫入失敗我們也不做任何事
                    }
                    
                    // 把座位清空還給系統
                    global_chat_state->users[i].is_active = 0;
                    break;
                }
            }
        }
    }
}

// --- 新增：處理別人傳來訊息的 Signal Handler ---
void sigusr1_handler(int sig) {
    if (global_chat_state != NULL) {
        for (int i = 0; i < MAX_USERS; i++) {
            // 找到自己
            if (global_chat_state->users[i].is_active && global_chat_state->users[i].pid == getpid()) {
                // 把信箱裡的信印到自己的螢幕上 (STDOUT 已經被導向到 Socket 了)
                if (write(STDOUT_FILENO, global_chat_state->users[i].msg_buffer, strlen(global_chat_state->users[i].msg_buffer)) < 0) {
                  // 在 Signal Handler 裡面就算寫入失敗我們也不能做什麼，所以這裡留空即可
                }
                // 清空信箱
                global_chat_state->users[i].msg_buffer[0] = '\0';
                break;
            }
        }
    }
}

// 處理內建指令
int handle_builtin(char **args) {
    if (args[0] == NULL) return 1;

    if (strcmp(args[0], "quit") == 0) {
        exit(0);
    } else if (strcmp(args[0], "setenv") == 0) {
        if (args[1] && args[2]) {
            setenv(args[1], args[2], 1);
        }
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

int main(int argc, char *argv[]) {
    int port = 8888;
    if (argc == 2) {
        port = atoi(argv[1]);
    }

    setenv("PATH", "bin:.", 1);

    int master_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (master_socket < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(port);       

    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(master_socket, 5) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", port);
    
    // --- 建立共享記憶體 (公佈欄) ---
    int shm_id = shmget(SHM_KEY, sizeof(ChatState), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget failed");
        exit(EXIT_FAILURE);
    }
    ChatState *chat_state = (ChatState *)shmat(shm_id, NULL, 0);
    global_chat_state = chat_state; // 存一份到全域變數給 Handler 用
    
    // 初始化公佈欄
    for (int i = 0; i < MAX_USERS; i++) {
        chat_state->users[i].is_active = 0;
    }

    // --- 新增：註冊 SIGCHLD 訊號處理，實現即時收屍 ---
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; // 確保 accept 如果被中斷會自動重新執行
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    
    while (1) {
        
        int client_socket;
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        
        client_socket = accept(master_socket, (struct sockaddr *)&client_addr, &addrlen);
        if (client_socket < 0) {
            perror("accept failed");
            continue; 
        }
        
        // 幫新客人找空位與配發 ID
        int user_idx = -1;
        for (int i = 0; i < MAX_USERS; i++) {
            if (chat_state->users[i].is_active == 0) {
                user_idx = i; 
                break;
            }
        }
        
        if (user_idx == -1) {
            printf("[Server Log] Chat room is full. Connection rejected.\n");
            // 由於還沒 fork，直接向 Client 印出客滿訊息
            char *msg = "Sorry, chat room is full.\n";
            if (write(client_socket, msg, strlen(msg)) < 0) {
              // 敷衍編譯器
            }
            close(client_socket);
            continue;
        }
        
        int user_id = user_idx + 1; 
        char ip_port[50];
        sprintf(ip_port, "%s:%d", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        printf("[Server Log] User %d login from %s\n", user_id, ip_port);

        pid_t pid = fork();
        
        if (pid < 0) {
            perror("fork failed");
            close(client_socket);
        } else if (pid == 0) {
            // --- 服務生子行程 ---
            close(master_socket); 

            dup2(client_socket, STDIN_FILENO);  
            dup2(client_socket, STDOUT_FILENO); 
            dup2(client_socket, STDERR_FILENO); 
            close(client_socket);               

            // 將自己的 user_id 存入環境變數，讓外部程式知道我是誰
            char id_str[10];
            sprintf(id_str, "%d", user_id);
            setenv("MY_USER_ID", id_str, 1);
            
            // --- 新增：註冊 SIGUSR1，準備接收別人傳來的訊息 ---
            struct sigaction sa_usr;
            sa_usr.sa_handler = sigusr1_handler;
            sigemptyset(&sa_usr.sa_mask);
            sa_usr.sa_flags = 0; // 注意：這裡不要設 SA_RESTART
            sigaction(SIGUSR1, &sa_usr, NULL);
            
            int npipe_table[NPIPE_SIZE][2];
            int line_count = 0;
            for (int i = 0; i < NPIPE_SIZE; i++) {
                npipe_table[i][0] = -1;
                npipe_table[i][1] = -1;
            }

            char input[MAX_INPUT_LEN];

            while (1) {
                // 清理背景指令 (管線指令) 的殭屍行程
                while (waitpid(-1, NULL, WNOHANG) > 0);

                printf("%% ");
                fflush(stdout);

                if (!fgets(input, sizeof(input), stdin)) {
                    // 如果是因為收到 Signal (門鈴) 而被打斷
                    if (errno == EINTR) {
                        clearerr(stdin); // 清除 stdin 的錯誤狀態
                        continue;        // 重新回到迴圈開頭 (會重新印出 % 提示字元)
                    }
                    break; // 真的斷線了才離開
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
                            cmd_start = i + 1;
                            continue; 
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
                                dup2(prev_fd, STDIN_FILENO);
                                close(prev_fd);
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
                                close(fd[0]); 
                                close(fd[1]);
                            }
                            
                            if (execvp(current_cmd[0], current_cmd) == -1) {
                                fprintf(stderr, "Unknown command: [%s].\n", current_cmd[0]);
                                exit(233); 
                            }
                        } else {
                            pids[pid_count++] = cmd_pid; 
                            
                            if (prev_fd != -1) close(prev_fd); 
                            
                            if (!is_last_cmd && !is_numbered_pipe) {
                                close(fd[1]); 
                                prev_fd = fd[0]; 
                            }
                        }
                        cmd_start = i + 1; 
                    }
                }
                
                if (npipe_table[current_line_idx][0] != -1) {
                    npipe_table[current_line_idx][0] = -1;
                }

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

                if (unknown_cmd_occurred) {
                    line_count--; 
                }
            } 
            
            // 服務生下班：離開迴圈後，一定要結束這個行程
            exit(0); 

        } else {
            // --- 櫃台父行程 --- 
            // 將客人的詳細資料寫入公佈欄
            chat_state->users[user_idx].id = user_id;
            chat_state->users[user_idx].is_active = 1;
            chat_state->users[user_idx].pid = pid; // 記錄服務生的 PID，讓 Handler 收屍時知道是誰
            strcpy(chat_state->users[user_idx].name, "(no name)"); 
            strcpy(chat_state->users[user_idx].ip_port, ip_port);
            
            close(client_socket); 
        }
    } 

    return 0;
}
