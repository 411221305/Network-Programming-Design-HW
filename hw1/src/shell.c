#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_CMD_LEN 256
#define MAX_INPUT_LEN 5000
#define NPIPE_SIZE 1024

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

int main() {
    char input[MAX_INPUT_LEN];
    setenv("PATH", "bin:.", 1);

    int npipe_table[NPIPE_SIZE][2];
    int line_count = 0;

    for (int i = 0; i < NPIPE_SIZE; i++) {
        npipe_table[i][0] = -1;
        npipe_table[i][1] = -1;
    }

    while (1) {
        // --- 新增：清理背景執行留下的殭屍行程 (Zombie Process) ---
        while (waitpid(-1, NULL, WNOHANG) > 0);

        printf("%% ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;

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
        // [0] 是讀取端   [1] 是寫入端
        int current_line_idx = line_count % NPIPE_SIZE;
        if (npipe_table[current_line_idx][0] != -1) {
            prev_fd = npipe_table[current_line_idx][0];
            if (npipe_table[current_line_idx][1] != -1) {
                close(npipe_table[current_line_idx][1]);
                npipe_table[current_line_idx][1] = -1;
            }
        }

        int unknown_cmd_occurred = 0;
        pid_t pids[100]; // 記錄這一行所有 fork 出來的 PID
        int pid_count = 0;

        for (int i = 0; i <= argc; i++) {
            int is_last_cmd = (i == argc);  // 是否已經讀到陣列最後了？
            int is_numbered_pipe = 0;       // 這個斷點是 |N 嗎？
            int n_pipe_target = 0;          // 如果是 |N，那個 N 是多少？
            
            // 什麼情況下算是一個「斷點」（一個指令區塊的結束）？
            // 1. 到達句尾 (is_last_cmd)
            // 2. 遇到一般管線 "|"
            // 3. 遇到編號管線 "|N" (長度大於 1 且開頭是 '|')
            if (is_last_cmd || strcmp(args[i], "|") == 0 || (args[i][0] == '|' && strlen(args[i]) > 1)) {
                
                // 如果斷點是 |N，把那個 N 的數字抓出來
                if (!is_last_cmd && args[i][0] == '|' && strlen(args[i]) > 1) {
                    is_numbered_pipe = 1;
                    n_pipe_target = atoi(&args[i][1]);
                }

                args[i] = NULL;  // 把這個斷點的符號替換成 NULL，這樣字串陣列在這裡就被切斷了
                char **current_cmd = &args[cmd_start];  // current_cmd 現在指向剛切下來的這個獨立小指令

                if (current_cmd[0] == NULL) break;

                if (handle_builtin(current_cmd)) {
                    cmd_start = i + 1;
                    continue; 
                }

                int fd[2];
                // 情況 A：這是一般管線 `|`
                if (!is_last_cmd && !is_numbered_pipe) {
                    if (pipe(fd) < 0) perror("pipe error");
                }
                // 情況 B：這是跨行管線 `|N`
                // --- 在 fork() 之前就建立 |N 的跨行管線 ---
                if (is_numbered_pipe) {
                    int target_idx = (line_count + n_pipe_target) % NPIPE_SIZE;
                    // 去記憶表的那個格子看看。如果是 -1，代表那邊還沒水管，我們就開一條放進去。
                    if (npipe_table[target_idx][0] == -1) {
                        if (pipe(npipe_table[target_idx]) < 0) perror("pipe error");
                    }
                }

                pid_t pid = fork();
                if (pid < 0) {
                    perror("fork error");
                } else if (pid == 0) {
                    // --- 子行程 ---
                    // 【處理輸入 STDIN】
                    // 如果前面有指令傳資料過來 (prev_fd)，我們用 dup2 把這根水管的讀取端「蓋過去」標準輸入 (STDIN_FILENO = 0)。
                    // 這樣指令如果去讀鍵盤，其實讀到的是水管裡的水。
                    if (prev_fd != -1) {
                        dup2(prev_fd, STDIN_FILENO);
                        close(prev_fd);
                    }
                    // 【處理輸出 STDOUT】
                    if (is_numbered_pipe) {
                        // 如果要丟給未來的行數 (|N)，就把 STDOUT 蓋成記憶表裡的「寫入端」
                        int target_idx = (line_count + n_pipe_target) % NPIPE_SIZE;
                        dup2(npipe_table[target_idx][1], STDOUT_FILENO);
                        // 子行程不需要這些背景水管的控制權，全關掉，否則檔案會被卡住以為還沒寫完
                        for(int k=0; k<NPIPE_SIZE; k++){
                            if(npipe_table[k][0] != -1) close(npipe_table[k][0]);
                            if(npipe_table[k][1] != -1) close(npipe_table[k][1]);
                        }
                    } else if (!is_last_cmd) {
                        // 如果要丟給同一行的下一個指令 (|)，就把 STDOUT 蓋成剛剛開的新水管寫入端
                        dup2(fd[1], STDOUT_FILENO);
                        close(fd[0]); 
                        close(fd[1]);
                    }
                    
                    // 【執行】
                    // 呼叫 execvp。這個子行程會被目前的指令 (例如 ls) 的程式碼覆蓋。
                    // 覆蓋後，ls 繼續執行，但它的輸入/輸出已經被我們導向到水管了！
                    if (execvp(current_cmd[0], current_cmd) == -1) {
                        fprintf(stderr, "Unknown command: [%s].\n", current_cmd[0]);
                        exit(233); // 未知指令的專屬暗號
                    }
                } else {
                    // --- 父行程 ---
                    pids[pid_count++] = pid; // 把剛剛派出去的子行程 ID 記起來，等一下才能追蹤他有沒有死掉
                    
                    // 如果剛剛有把上一輪的水管讀取端給子行程了，父行程這邊就可以關掉了
                    if (prev_fd != -1) close(prev_fd); 
                    
                    // 如果這是一般管線 `|`，父行程把剛剛新開水管的「寫入端」關掉 (因為只有子行程要寫)。
                    // 並且把「讀取端」存進 prev_fd，留給迴圈的「下一個指令」去讀。
                    if (!is_last_cmd && !is_numbered_pipe) {
                        close(fd[1]); 
                        prev_fd = fd[0]; 
                    }
                    // 注意：父行程不能在這裡關閉 |N 的 write 端，必須保留到目標行數到達時才關閉
                }
                
                cmd_start = i + 1; // 把指標移動到下一個斷點的後面，準備處理下一個小指令
            }
        }
        
        // 清理記憶表的讀取端紀錄
        if (npipe_table[current_line_idx][0] != -1) {
            npipe_table[current_line_idx][0] = -1;
        }

        // 如果這行指令是輸出到 |N，它就是背景程序，我們不應該卡住等待它
        int wait_for_foreground = 1;
        if (argc > 0 && args[argc-1] != NULL && args[argc-1][0] == '|' && strlen(args[argc-1]) > 1) {
             wait_for_foreground = 0;
        }

        // --- 等待所有子行程，並捕捉未知指令 ---
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
            line_count--; // 未知指令不計入行數
        }
    }
    return 0;
}
