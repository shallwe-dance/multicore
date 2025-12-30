#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include "commit.h"

#define FAIL_AFTER_PREPARE 0
#define FAIL_AFTER_COMMIT 1
#define MAX_PARTICIPANTS 10

typedef struct {
    int id;
    unsigned long prog_number;
    char coord_host[256];
    int fail_on_prepare;
    int fail_on_commit;
    int fail_on_abort;
    int fail_after_prepare;
    int fail_after_commit;
    char *conf_file;
} Config;

void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s --conf <file> [OPTIONS]\n"
        "Options:\n"
        " --conf <file>      Participant configuration file (REQUIRED)\n"
        " --id <n>           Participant or coordinator ID\n"
        " --prog <hex|dec>   RPC program number (e.g., 0x20000001)\n"
        " --coord-host <host> Coordinator hostname (default: localhost)\n"
        " --fail-on-prepare  Simulate crash/failure during prepare phase\n"
        " --fail-on-commit   Simulate crash/failure during commit phase\n"
        " --fail-on-abort    Simulate crash/failure during abort phase\n"
        " --fail-after-prepare Simulate crash/failure after prepare phase\n"
        " --fail-after-commit  Simulate crash/failure after commit phase\n"
        " -h, --help         Show this help message\n",
        prog
    );
}

void parse_args(int argc, char *argv[], Config *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->prog_number = 0x20000001; // default program number
    strcpy(cfg->coord_host, "localhost"); // default coordinator host
    cfg->conf_file = NULL; // [추가됨]

    static struct option long_opts[] = {
        {"id", required_argument, 0, 'i'},
        {"prog", required_argument, 0, 'p'},
        {"coord-host", required_argument, 0, 'c'}, // [수정됨] 'c'에 long opt 추가
        {"conf", required_argument, 0, 'f'},     // [추가됨] --conf
        {"fail-on-prepare", no_argument, 0, 1},
        {"fail-on-commit", no_argument, 0, 2},
        {"fail-on-abort", no_argument, 0, 3},
        {"fail-after-prepare", no_argument, 0, 4},
        {"fail-after-commit", no_argument, 0, 5},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

	int opt, opt_index = 0;
    // [수정됨] "c:" -> "i:p:c:f:h" ('f:' 추가)
    while ((opt = getopt_long(argc, argv, "i:p:c:f:h", long_opts, &opt_index)) != -1) {
        switch (opt) {
            case 'i':
                cfg->id = atoi(optarg);
                break;
            case 'p':
                cfg->prog_number = strtoul(optarg, NULL, 0); // supports 0x or decimal
                break;
            case 'c':
                strncpy(cfg->coord_host, optarg, sizeof(cfg->coord_host) - 1);
                cfg->coord_host[sizeof(cfg->coord_host) - 1] = '\0';
                break;
            case 'f': // [추가됨]
                cfg->conf_file = optarg;
                break;
            case 1:
                cfg->fail_on_prepare = 1;
                break;
            case 2:
                cfg->fail_on_commit = 1;
                break;
            case 3:
                cfg->fail_on_abort = 1;
                break;
            case 4:
                cfg->fail_after_prepare = 1;
                break;
            case 5:
                cfg->fail_after_commit = 1;
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                exit(opt == 'h' ? 0 : 1);
        }
    }
}

typedef struct participant {
    char *host;
    u_long prog;
    CLIENT *clnt;
} Participant;

int num_participants=0;
Participant participants[MAX_PARTICIPANTS];
char log_filename[256];

void append_log(const char *msg) {
    int fd = open(log_filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("로그 파일 열기 실패");
        return;
    }
	char buf[512];
	int len = snprintf(buf, sizeof(buf), "%s\n", msg);

    write(fd, buf, len);
    fsync(fd); // 중요: 디스크에 즉시 기록
    close(fd);
}


void load_participants(const char *filename) {
	FILE *fp = fopen(filename, "r");
	if (!fp) {
		fprintf(stderr, "load_participants: no such filename");
		exit(1);
	}

	char host[256];
	unsigned long prog;

	while (fscanf(fp, "%s %lx", host, &prog) == 2) {
        if (num_participants >= MAX_PARTICIPANTS) break;

        participants[num_participants].host = strdup(host);
        participants[num_participants].prog = prog;

        // RPC 클라이언트 생성
        participants[num_participants].clnt = clnt_create(
            participants[num_participants].host,
            participants[num_participants].prog,
            COMMIT_VERS,
            "udp" // 또는 "udp"
        );

        if (participants[num_participants].clnt == NULL) {
            // clnt_pcreateerror(participants[num_participants].host);
            fprintf(stderr, "%s (프로그램 0x%lx)에 연결 실패\n",
                    participants[num_participants].host,
                    participants[num_participants].prog);
            // 프로젝트 요구사항에 따라 여기서 종료하거나 무시할 수 있음
        } else {
             printf("%s (0x%lx) 연결 성공\n", host, prog);
        }
        num_participants++;
    }

	fclose(fp);
}

void send_decision(TxnID *txn, int decision) {
    for (int i = 0; i < num_participants; i++) {
        if (participants[i].clnt == NULL) {
            // 연결 끊겼으면 재연결 시도
            participants[i].clnt = clnt_create(participants[i].host, participants[i].prog, COMMIT_VERS, "tcp");
        }
        
        if (participants[i].clnt) {
            if (decision == COMMIT) {
                commit_1(txn, participants[i].clnt);
                printf("[Coord] %s 에게 COMMIT 전송\n", participants[i].host);
            } else {
                abort_1(txn, participants[i].clnt);
                printf("[Coord] %s 에게 ABORT 전송\n", participants[i].host);
            }
        }
    }
}

int perform_recovery() {
    FILE *fp = fopen(log_filename, "r");
    if (!fp) return 0;

    char line[256];
    int last_txn_id = -1;
    int decision = 0; // 0:미결정, 1:COMMIT, 2:ABORT
    int finished = 0; // END_2PC 기록 여부

    // 로그 파싱: 마지막 트랜잭션의 상태 확인
    while (fgets(line, sizeof(line), fp)) {
        int id;
        if (sscanf(line, "START_2PC %d", &id) == 1) {
            last_txn_id = id;
            decision = 0;
            finished = 0;
        } else if (sscanf(line, "COMMIT %d", &id) == 1) {
            if (id == last_txn_id) decision = COMMIT;
        } else if (sscanf(line, "ABORT %d", &id) == 1) {
            if (id == last_txn_id) decision = ABORT;
        } else if (sscanf(line, "END_2PC %d", &id) == 1) {
            if (id == last_txn_id) finished = 1;
        }
    }
    fclose(fp);

    // 복구할 트랜잭션이 없거나 이미 종료된 경우
    if (last_txn_id == -1 || finished) {
        return 0;
    }

    printf("[Coord] 복구 모드 시작: 미완료 트랜잭션 %d 발견\n", last_txn_id);

    TxnID txn;
    txn.txn_id = last_txn_id;
    char log_buf[128];

	if (decision == COMMIT) {
        // 이미 COMMIT 로그가 있으면 계속 진행
        printf("[Coord] Found COMMIT in log. Re-sending COMMIT.\n");
        send_decision(&txn, COMMIT);
    } else if (decision == ABORT) {
        printf("[Coord] Found ABORT in log. Re-sending ABORT.\n");
        send_decision(&txn, ABORT);
    } else {
        // [중요] START는 있는데 결정(COMMIT/ABORT)이 없는 경우 -> ABORT 결정
        printf("[Coord] No decision in log (Crashed before commit). Deciding ABORT.\n");
        
        sprintf(log_buf, "ABORT %d", last_txn_id);
        append_log(log_buf);
        
        send_decision(&txn, ABORT);
    }

    sprintf(log_buf, "END_2PC %d", last_txn_id);
    append_log(log_buf);
    exit(0);
    
    return 1; // 복구 수행함
}

int main(int argc, char *argv[]) {
	Config cfg;
	parse_args(argc, argv, &cfg);

	printf("[DEBUG] cfg.fail_after_prepare=%d, cfg.fail_after_commit=%d\n",
       cfg.fail_after_prepare, cfg.fail_after_commit);
fflush(stdout);

	sprintf(log_filename, "coordinator_%d.log", cfg.id);
	load_participants(cfg.conf_file);

	perform_recovery();

	// 3. 트랜잭션 시작
    TxnID txn;
    txn.txn_id = (int)time(NULL);

    printf("[Coord] 트랜잭션 %d 시작 (Phase 1: PREPARE)\n", txn.txn_id);
    char log_buf[128];
    sprintf(log_buf, "START_2PC %d", txn.txn_id);
    append_log(log_buf);

    // --- Phase 1: PREPARE ---
    int all_yes = 1;

    for (int i = 0; i < num_participants; i++) {
        if (participants[i].clnt == NULL) {
            all_yes = 0; // 연결 안 된 참여자가 있으면 ABORT
            continue;
        }

        PrepareResult *res = prepare_1(&txn, participants[i].clnt);

        if (res == NULL) {
            //fprintf(stderr, "[Coord] %s 응답 없음 (Timeout/Error)\n", participants[i].host);
            all_yes = 0;
        } else if (res->ok == 0) {
            printf("[Coord] %s 투표: NO (Reason: %s)\n", participants[i].host, res->info);
            all_yes = 0;
        } else {
            printf("[Coord] %s 투표: YES\n", participants[i].host);
        }
    }
	
	if (all_yes) {
        if (cfg.fail_after_prepare) {
            fprintf(stderr, "[TEST] Crashing BEFORE writing COMMIT log... (Force Abort Scenario)\n");
            exit(1); 
        }

        // COMMIT 결정 및 로그 기록
        printf("[COORD] COMMIT %d\n", txn.txn_id);
        sprintf(log_buf, "COMMIT %d", txn.txn_id);
        append_log(log_buf); // 영구 기록

        // [Crash Test] fail-after-commit (로그 기록 직후 크래시)
        if (cfg.fail_after_commit) {
            fprintf(stderr, "[TEST] Crashing AFTER writing COMMIT log...\n");
            exit(1);
        }

        // Phase 2: COMMIT 메시지 전송
        send_decision(&txn, COMMIT);

    } else {
        // ABORT 결정
        printf("[Coord] 결정: ABORT\n");
        
        // Abort 결정 시에도 fail_after_prepare는 로그 기록 전에 죽어야 함
        if (cfg.fail_after_prepare) {
             fprintf(stderr, "[TEST] Crashing BEFORE writing ABORT log...\n");
             exit(1);
        }

        sprintf(log_buf, "ABORT %d", txn.txn_id);
        append_log(log_buf); 

        send_decision(&txn, ABORT);
    }

    sprintf(log_buf, "END_2PC %d", txn.txn_id);
    append_log(log_buf);

    return 0;
}

