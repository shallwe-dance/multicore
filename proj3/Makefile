RPCGEN=rpcgen
CC=gcc
CFLAGS=-Wall -g

all: coordinator participant

# rpcgen 규칙 (commit_svc.c가 생성되지만 사용하진 않음)
commit.h commit_xdr.c commit_clnt.c commit_svc.c: commit.x
	$(RPCGEN) -C commit.x

# Coordinator 빌드
coordinator: commit_clnt.c commit_xdr.c coordinator.c
	$(CC) $(CFLAGS) -o coordinator coordinator.c commit_clnt.c commit_xdr.c -lnsl -I/usr/include/tirpc -ltirpc

# Participant 빌드 (중요: commit_svc.c 제거됨)
participant: commit_xdr.c participant.c
	$(CC) $(CFLAGS) -o participant participant.c commit_xdr.c -lnsl -I/usr/include/tirpc -ltirpc

clean:
	rm -f coordinator participant *.o commit.h commit_xdr.c commit_clnt.c commit_svc.cA
