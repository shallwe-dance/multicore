#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>

#include "commit.h"

// 함수 선언
void commit_prog_1(struct svc_req *rqstp, register SVCXPRT *transp);

typedef struct {
    int id;
    unsigned long prog_number;
    char coord_host[256];
    int fail_on_prepare;
    int fail_on_commit;
    int fail_on_abort;
    int fail_after_prepare;
    int fail_after_commit;
} Config;

Config cfg; // 전역 설정 변수
static char log_file[256];

/* ---------------------------------------------------
   Logging
--------------------------------------------------- */
static void append_log(const char *msg)
{
    int fd = open(log_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    dprintf(fd, "%s\n", msg);
    fsync(fd); // 중요: 크래시 복구를 위해 디스크 동기화
    close(fd);
}

/* ---------------------------------------------------
   Failure simulation (기존 로직 유지)
--------------------------------------------------- */
static void maybe_fail(const char *phase)
{
    if ((cfg.fail_on_prepare && strcmp(phase, "prepare") == 0) || 
        (cfg.fail_on_commit && strcmp(phase, "commit") == 0) ||
        (cfg.fail_on_abort && strcmp(phase, "abort") == 0)) {
            printf("[FAILURE] Simulating crash during %s\n", phase);
            exit(1);
    }

    if ((cfg.fail_after_prepare && strcmp(phase, "after_prepare") == 0) ||
        (cfg.fail_after_commit && strcmp(phase, "after_commit") == 0)) {
            fprintf(stderr, "[TEST] %s 단계 후 강제 크래시 (Fail-After)\n", phase);
            exit(1);
    }
}

// 이전에 Abort 했었는지 확인하는 로직 (간단 구현)
int previously_aborted(int tid) {
    // log_file은 전역 변수로 선언되어 있다고 가정 (기존 코드 유지)
    FILE *fp = fopen(log_file, "r"); 
    if (!fp) return 0; // 로그 파일이 없으면 Abort 기록도 없는 것

    char line[256];
    char search_str[64];
    sprintf(search_str, "%d ABORT", tid); // 검색할 패턴 (예: "100 ABORT")

    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        // 로그 한 줄에 해당 패턴이 있는지 확인
        if (strstr(line, search_str) != NULL) {
            found = 1;
            break;
        }
    }
    fclose(fp);
    return found;
}

/* ---------------------------------------------------
   RPC handler: PREPARE
--------------------------------------------------- */
PrepareResult *prepare_1_svc(TxnID *id_in, struct svc_req *req)
{
    static PrepareResult res;
    memset(&res, 0, sizeof(res));
    int tid = id_in->txn_id;

    // 1. maybe_fail("prepare")
    maybe_fail("prepare");

    // 2. if local log shows previous decision == ABORT: return VOTE_ABORT
    if (previously_aborted(tid)) {
        printf("[Participant] Txn %d was previously ABORTED. Voting NO.\n", tid);
        res.ok = 0;             // VOTE_ABORT
        res.info = "Previously Aborted"; 
        return &res;
    }

    // 3. if can_commit(data) == TRUE: (여기서는 무조건 TRUE로 가정)
    int can_commit = 1; 

    if (can_commit) {
        // write_log("PREPARED", transaction_id)
        char buf[128];
        sprintf(buf, "%d PREPARED YES", tid);
        append_log(buf);
        printf("%d PREPARED YES", tid);

        // maybe_fail("after_prepare")
        maybe_fail("after_prepare");

        // return VOTE_COMMIT
        res.ok = 1;
        res.info = "YES";
    } 
    else { 
        // 4. else: (can_commit == FALSE 인 경우)
        
        // write_log("ABORT", transaction_id)
        char buf[128];
        sprintf(buf, "%d ABORT", tid); // 로컬에서 ABORT 결정했음을 기록
        append_log(buf);

        // return VOTE_ABORT
        res.ok = 0;
        res.info = "NO";
    }

    return &res;
}

/* ---------------------------------------------------
   RPC handler: COMMIT
--------------------------------------------------- */
int *commit_1_svc(TxnID *id_in, struct svc_req *req)
{
    static int result = 1;

    printf("[Participant %d] COMMIT 요청 (TxnID: %d)\n", cfg.id, id_in->txn_id);

    maybe_fail("commit");

    char buf[128];
    sprintf(buf, "%d COMMITTED", id_in->txn_id);
    printf("[participant %d] COMMITTED\n", cfg.id);
    append_log(buf);

    maybe_fail("after_commit");

    return &result;
}

/* ---------------------------------------------------
   RPC handler: ABORT
--------------------------------------------------- */
int *abort_1_svc(TxnID *id_in, struct svc_req *req)
{
    static int result = 1;

    maybe_fail("abort");

    char buf[128];
    sprintf(buf, "%d ABORTED", id_in->txn_id);
    printf("[participant %d] ABORTED\n", cfg.id);
    append_log(buf);


    return &result;
}

/* ----------------------------------------------------------------
   RPC Dispatcher (commit_svc.c 대체, Segfault 방지용 핵심 코드)
---------------------------------------------------------------- */
void commit_prog_1(struct svc_req *rqstp, register SVCXPRT *transp)
{
    union {
        TxnID prepare_1_arg;
        TxnID commit_1_arg;
        TxnID abort_1_arg;
    } argument;
    char *result;
    xdrproc_t _xdr_argument, _xdr_result;
    char *(*local)(char *, struct svc_req *);

    switch (rqstp->rq_proc) {
    case NULLPROC:
        (void) svc_sendreply (transp, (xdrproc_t) xdr_void, (char *)NULL);
        return;

    case PREPARE:
        _xdr_argument = (xdrproc_t) xdr_TxnID;
        _xdr_result = (xdrproc_t) xdr_PrepareResult;
        local = (char *(*)(char *, struct svc_req *)) prepare_1_svc;
        break;

    case COMMIT:
        _xdr_argument = (xdrproc_t) xdr_TxnID;
        _xdr_result = (xdrproc_t) xdr_int;
        local = (char *(*)(char *, struct svc_req *)) commit_1_svc;
        break;

    case ABORT:
        _xdr_argument = (xdrproc_t) xdr_TxnID;
        _xdr_result = (xdrproc_t) xdr_int;
        local = (char *(*)(char *, struct svc_req *)) abort_1_svc;
        break;

    default:
        svcerr_noproc (transp);
        return;
    }
    
    memset ((char *)&argument, 0, sizeof (argument));
    if (!svc_getargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
        svcerr_decode (transp);
        return;
    }
    
    result = (*local)((char *)&argument, rqstp);
    
    if (result != NULL && !svc_sendreply(transp, (xdrproc_t) _xdr_result, result)) {
        svcerr_systemerr (transp);
    }
    
    if (!svc_freeargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
        fprintf (stderr, "unable to free arguments\n");
        exit (1);
    }
    return;
}

static void alarm_handler(int signum)
{
    if (signum == SIGALRM) {
        printf("\n[Participant %d] 30 second passed!!\n", cfg.id);
        svc_exit(); 
    }
}

/* ---------------------------------------------------
   Main Function
--------------------------------------------------- */
int main(int argc, char *argv[]) {

    // 종료 타이머 설정
    //if (signal(SIGALRM, alarm_handler) == SIG_ERR) {
        //perror("signal failed");
        //exit(1);
    //}
    //alarm(30); //30초 설정
    // 기본값 설정
    memset(&cfg, 0, sizeof(cfg));
    cfg.prog_number = 0x20313974; 

    // 인자 파싱 (getopt_long 대신 간단히 파싱)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--id") == 0 && i + 1 < argc) {
            cfg.id = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--prog") == 0 && i + 1 < argc) {
            cfg.prog_number = strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--fail-on-prepare") == 0) cfg.fail_on_prepare = 1;
        else if (strcmp(argv[i], "--fail-on-commit") == 0) cfg.fail_on_commit = 1;
        else if (strcmp(argv[i], "--fail-on-abort") == 0) cfg.fail_on_abort = 1;
        else if (strcmp(argv[i], "--fail-after-prepare") == 0) cfg.fail_after_prepare = 1;
        else if (strcmp(argv[i], "--fail-after-commit") == 0) cfg.fail_after_commit = 1;
    }

    if (cfg.id == 0) {
        fprintf(stderr, "Usage: %s --id <id> ...\n", argv[0]);
        exit(1);
    }

    sprintf(log_file, "participant_%d.log", cfg.id);
    printf("Participant %d (Prog: 0x%lx) Starting...\n", cfg.id, cfg.prog_number);

    // RPC 서버 설정
    register SVCXPRT *transp;

    pmap_unset(cfg.prog_number, COMMIT_VERS);

    transp = svcudp_create(RPC_ANYSOCK);
    if (transp == NULL) {
        fprintf(stderr, "cannot create udp service.\n");
        exit(1);
    }
    if (!svc_register(transp, cfg.prog_number, COMMIT_VERS, commit_prog_1, IPPROTO_UDP)) {
        fprintf(stderr, "unable to register (UDP).\n");
        exit(1);
    }

    transp = svctcp_create(RPC_ANYSOCK, 0, 0);
    if (transp == NULL) {
        fprintf(stderr, "cannot create tcp service.\n");
        exit(1);
    }
    if (!svc_register(transp, cfg.prog_number, COMMIT_VERS, commit_prog_1, IPPROTO_TCP)) {
        fprintf(stderr, "unable to register (TCP).\n");
        exit(1);
    }

    svc_run();
    exit(1);
}
