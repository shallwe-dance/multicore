빌드 방법
1. make clean
2. make all (warning 무시)

실험 방법
1. test 내 각 테스트 파일 실행
2. 실행 30초 후 종료

logs폴더에 저장되는 동작
- coordinator가 클라이언트 연결에 성공했을 때
- coordinator를 통해 수집된 각 participant의 투표 정보
- coordinator가 participant에 보낸 결정
- 복구 시작 및 복구 결과

test1 결과
s20313974@swin:/home/s20313974/mcc/2020313974_proj3$ cat logs/test1/*
[DEBUG] cfg.fail_after_prepare=0, cfg.fail_after_commit=0
127.0.0.1 (0x20313974) 연결 성공
127.0.0.1 (0x30313974) 연결 성공
127.0.0.1 (0x40313974) 연결 성공
[Coord] 트랜잭션 1763647999 시작 (Phase 1: PREPARE)
[Coord] 127.0.0.1 투표: YES
[Coord] 127.0.0.1 투표: YES
[Coord] 127.0.0.1 투표: YES
[COORD] COMMIT 1763647999
[Coord] 127.0.0.1 에게 COMMIT 전송
[Coord] 127.0.0.1 에게 COMMIT 전송
[Coord] 127.0.0.1 에게 COMMIT 전송

test1 persistent log 결과
s20313974@swin:/home/s20313974/mcc/2020313974_proj3$ cat *.log
START_2PC 1763647999
COMMIT 1763647999
END_2PC 1763647999
1763647999 PREPARED YES
1763647999 COMMITTED
1763647999 PREPARED YES
1763647999 COMMITTED
1763647999 PREPARED YES
1763647999 COMMITTED


test2 결과
s20313974@swin:/home/s20313974/mcc/2020313974_proj3$ cat logs/test2/*
[DEBUG] cfg.fail_after_prepare=0, cfg.fail_after_commit=0
127.0.0.1 (0x20313974) 연결 성공
127.0.0.1 (0x30313974) 연결 성공
127.0.0.1 (0x40313974) 연결 성공
[Coord] 트랜잭션 1763648144 시작 (Phase 1: PREPARE)
[Coord] 127.0.0.1 투표: YES
[Coord] 127.0.0.1 투표: YES
[Coord] 결정: ABORT
[Coord] 127.0.0.1 에게 ABORT 전송
[Coord] 127.0.0.1 에게 ABORT 전송
[Coord] 127.0.0.1 에게 ABORT 전송
Participant 2 (Prog: 0x30313974) Starting...
[FAILURE] Simulating crash during prepare

test2 persistent log 결과
s20313974@swin:/home/s20313974/mcc/2020313974_proj3$ cat *.log
START_2PC 1763648144
ABORT 1763648144
END_2PC 1763648144
1763648144 PREPARED YES
1763648144 ABORTED
1763648144 PREPARED YES
1763648144 ABORTED

test3 결과
s20313974@swin:/home/s20313974/mcc/2020313974_proj3$ cat logs/recovery_test/*
[DEBUG] cfg.fail_after_prepare=0, cfg.fail_after_commit=0
127.0.0.1 (0x20313974) 연결 성공
127.0.0.1 (0x30313974) 연결 성공
127.0.0.1 (0x40313974) 연결 성공
[Coord] 복구 모드 시작: 미완료 트랜잭션 1763648211 발견
[Coord] No decision in log (Crashed before commit). Deciding ABORT.
[Coord] 127.0.0.1 에게 ABORT 전송
[Coord] 127.0.0.1 에게 ABORT 전송
[Coord] 127.0.0.1 에게 ABORT 전송

test3 persistent log 결과
s20313974@swin:/home/s20313974/mcc/2020313974_proj3$ cat *.log
START_2PC 1763648211
ABORT 1763648211
END_2PC 1763648211
1763648211 PREPARED YES
1763648211 ABORTED
1763648211 PREPARED YES
1763648211 ABORTED
1763648211 PREPARED YES
1763648211 ABORTED

test4 결과
s20313974@swin:/home/s20313974/mcc/2020313974_proj3$ cat logs/recovery_test/*
[DEBUG] cfg.fail_after_prepare=0, cfg.fail_after_commit=0
127.0.0.1 (0x20313974) 연결 성공
127.0.0.1 (0x30313974) 연결 성공
127.0.0.1 (0x40313974) 연결 성공
[Coord] 복구 모드 시작: 미완료 트랜잭션 1763648315 발견
[Coord] Found COMMIT in log. Re-sending COMMIT.
[Coord] 127.0.0.1 에게 COMMIT 전송
[Coord] 127.0.0.1 에게 COMMIT 전송
[Coord] 127.0.0.1 에게 COMMIT 전송

test4 persistent log 결과
s20313974@swin:/home/s20313974/mcc/2020313974_proj3$ cat *.log
START_2PC 1763648315
COMMIT 1763648315
END_2PC 1763648315
1763648315 PREPARED YES
1763648315 COMMITTED
1763648315 PREPARED YES
1763648315 COMMITTED
1763648315 PREPARED YES
1763648315 COMMITTED

