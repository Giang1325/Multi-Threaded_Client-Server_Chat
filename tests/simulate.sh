#!/usr/bin/env bash
set -u                      # dung bien chua gan gia tri thi bao loi, tranh go nham ten bien

# ----------------------------- Tham so kich ban -----------------------------
MSG_INTERVAL=2              # giay giua 2 tin nhan cua 1 bot
CHURN_PERIOD=5              # chu ky roi/vao lai (giay)
CHURN_PERCENT=10            # % so bot roi/vao lai moi chu ky
SPAWN_GAP=0.02              # nghi giua 2 lan bat bot, tranh day hang doi listen cua server

# ----------------------------- Doc tham so dong lenh ------------------------
usage() { # parse_port
    echo "Cach dung: $0 N T [PORT]   (vi du: $0 50 60)" >&2 # stderr
    exit 1 # fail
}

[ $# -ge 2 ] && [ $# -le 3 ] || usage
N=$1
T=$2
PORT=${3:-9090}

is_pos_int() { [[ $1 =~ ^[1-9][0-9]*$ ]]; }
is_pos_int "$N"    || { echo "Loi: N phai la so nguyen duong" >&2; usage; }
is_pos_int "$T"    || { echo "Loi: T phai la so nguyen duong" >&2; usage; }
is_pos_int "$PORT" && [ "$PORT" -le 65535 ] || { echo "Loi: PORT phai tu 1 den 65535" >&2; usage; }

# ----------------------------- Duong dan -----------------------------------
# Luon chay tu thu muc goc cua repo, du go lenh o dau
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 1

SERVER_BIN=bin/server
RESULT_DIR=tests/results
CSV=$RESULT_DIR/cpu_$N.csv
LOG=$RESULT_DIR/server_$N.log

command -v nc >/dev/null || { echo "Loi: chua co nc (sudo apt install netcat-openbsd)" >&2; exit 1; }

mkdir -p "$RESULT_DIR"

if [ ! -x "$SERVER_BIN" ]; then
    echo "Chua co $SERVER_BIN, dang chay make..."
    make >/dev/null || { echo "Loi: make that bai" >&2; exit 1; }
fi

# ----------------------------- Bien trang thai ------------------------------
SERVER_PID=""
declare -a BOT_PID          # BOT_PID[i] = PID (cung la ma nhom tien trinh) cua bot i

# ----------------------------- Don dep --------------------------------------
# Giet ca nhom tien trinh cua 1 bot (vong gui tin + nc)
kill_bot() {
    local pid=$1
    [ -n "$pid" ] && kill -TERM -- "-$pid" 2>/dev/null
}

cleanup() {
    trap - EXIT INT TERM    # tranh goi cleanup 2 lan
    for i in "${!BOT_PID[@]}"; do
        kill_bot "${BOT_PID[$i]}"
    done
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill -TERM "$SERVER_PID" 2>/dev/null
        wait "$SERVER_PID" 2>/dev/null
    fi
}
trap cleanup EXIT
trap 'echo; echo "Bi ngat (Ctrl+C), dang don dep..."; exit 130' INT TERM

# ----------------------------- Ham do tren /proc ----------------------------
CLK_TCK=$(getconf CLK_TCK)  # so "tick" CPU moi giay (thuong la 100)

# So luong cua server: dong "Threads:" trong /proc/<pid>/status
thread_count() {
    awk '/^Threads:/ { print $2 }' "/proc/$SERVER_PID/status" 2>/dev/null
}

# Tong thoi gian CPU (tinh bang tick) server da dung: utime + stime
# (truong 14 va 15 cua /proc/<pid>/stat; cat bo phan "(ten)" truoc cho an toan)
cpu_ticks() {
    local stat
    stat=$(cat "/proc/$SERVER_PID/stat" 2>/dev/null) || return 1
    stat=${stat##*) }       # bo "pid (server) ", con lai bat dau tu truong 3
    set -- $stat
    echo $(( ${12} + ${13} ))
}

# Thoi diem hien tai, tinh bang giay co phan le (vi du 1731490000.123456789)
now() { date +%s.%N; }

# ----------------------------- Bot (client gia) -----------------------------
# Mot bot = vong lap in "ten" roi cu MSG_INTERVAL giay in 1 tin, dan qua nc.
# Dau ra cua nc dua vao /dev/null: bot VAN DOC moi thu server gui ve.
# (Neu khong doc, bo dem socket day, write() cua server bi chan trong luc
#  giu clients_mutex -> ca server treo.)
start_bot() {
    local i=$1
    local name="bot$i"
    # set -m (job control) chi bat trong luc tao bot: bot duoc dat vao 1 nhom
    # tien trinh rieng. Boc ca duong ong trong ( ... ) de $! la PID cua
    # subshell, cung la ma nhom -> kill -- -PID giet duoc ca vong gui tin lan nc
    set -m
    (
        {
            printf '%s\n' "$name"
            k=0
            while :; do
                sleep "$MSG_INTERVAL"
                k=$((k + 1))
                printf 'tin %d cua %s\n' "$k" "$name"
            done
        } 2>/dev/null | nc 127.0.0.1 "$PORT" >/dev/null 2>&1
    ) &
    BOT_PID[$i]=$!
    set +m
    disown "$!" 2>/dev/null  # bash khong in "Terminated" khi bot bi giet
}

# ============================================================================
# (1) BAT SERVER
# ============================================================================
echo "=== Kiem thu tai: N=$N bot, T=$T giay, cong $PORT ==="

# stdbuf -oL: server in theo tung dong ngay ca khi stdout la file,
# nho vay doc duoc dong "WELCOME" de biet server da listen xong
stdbuf -oL "$SERVER_BIN" "$PORT" >"$LOG" 2>&1 &
SERVER_PID=$!
disown "$SERVER_PID" 2>/dev/null

for _ in $(seq 50); do      # cho toi da 5 giay
    grep -q "WELCOME" "$LOG" 2>/dev/null && break
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "Loi: server khong khoi dong duoc (cong $PORT dang bi dung?)" >&2
        cat "$LOG" >&2
        SERVER_PID=""
        exit 1
    fi
    sleep 0.1
done
grep -q "WELCOME" "$LOG" || { echo "Loi: server khong san sang sau 5 giay" >&2; exit 1; }
echo "(1) Server da chay, PID=$SERVER_PID, so luong=$(thread_count)"

# ============================================================================
# (2) SINH N BOT
# ============================================================================
for i in $(seq 1 "$N"); do
    start_bot "$i"
    sleep "$SPAWN_GAP"
done
sleep 1                     # cho bot cuoi cung ket noi xong
echo "(2) Da bat $N bot, so luong server=$(thread_count) (mong doi $((N + 1)))"

# ============================================================================
# (3)+(4) VONG DO: moi giay ghi 1 dong CSV; cu CHURN_PERIOD giay thi xao tron bot
# ============================================================================
CHURN_COUNT=$(( (N * CHURN_PERCENT + 99) / 100 ))   # lam tron len
[ "$CHURN_COUNT" -lt 1 ] && CHURN_COUNT=1

echo "giay,cpu_phan_tram,so_luong" >"$CSV"

prev_ticks=$(cpu_ticks)
prev_time=$(now)

for sec in $(seq 1 "$T"); do
    sleep 1

    kill -0 "$SERVER_PID" 2>/dev/null || { echo "Loi: server da chet o giay $sec" >&2; exit 1; }

    cur_ticks=$(cpu_ticks)
    cur_time=$(now)
    threads=$(thread_count)

    # %CPU = (so giay CPU da dung) / (so giay thuc da troi qua) * 100
    cpu=$(awk -v dt="$((cur_ticks - prev_ticks))" -v hz="$CLK_TCK" \
              -v t0="$prev_time" -v t1="$cur_time" \
              'BEGIN { printf "%.1f", (dt / hz) / (t1 - t0) * 100 }')

    echo "$sec,$cpu,$threads" >>"$CSV"
    printf '\r(4) Giay %3d/%d  CPU=%6s%%  luong=%4s' "$sec" "$T" "$cpu" "$threads"

    prev_ticks=$cur_ticks
    prev_time=$cur_time

    # (3) Xao tron: chon ngau nhien CHURN_COUNT bot, ngat roi ket noi lai
    if [ $((sec % CHURN_PERIOD)) -eq 0 ] && [ "$sec" -lt "$T" ]; then
        chosen=$(shuf -i 1-"$N" -n "$CHURN_COUNT")
        for i in $chosen; do
            kill_bot "${BOT_PID[$i]}"
        done
        sleep 0.2           # cho server kip go ten cu khoi danh sach (tranh "trung ten")
        for i in $chosen; do
            start_bot "$i"
        done
    fi
done
echo

# ============================================================================
# (5) TAT BOT, KIEM TRA RO RI LUONG, TAT SERVER
# ============================================================================
for i in "${!BOT_PID[@]}"; do
    kill_bot "${BOT_PID[$i]}"
done
BOT_PID=()

# Cho toi da 10 giay de cac luong handle_client thoat het
for _ in $(seq 100); do
    [ "$(thread_count)" = "1" ] && break
    sleep 0.1
done
threads_after=$(thread_count)

kill -TERM "$SERVER_PID" 2>/dev/null
wait "$SERVER_PID" 2>/dev/null
SERVER_PID=""

# ----------------------------- Tom tat -------------------------------------
summary=$(awk -F, 'NR > 1 {
        sum += $2; n++
        if ($2 > maxc) maxc = $2
        if ($3 > maxt) maxt = $3
    }
    END { printf "%.1f %.1f %d", sum / n, maxc, maxt }' "$CSV")
read -r cpu_avg cpu_max threads_max <<<"$summary"

echo "(5) Ket qua -> $CSV"
echo "    CPU trung binh      : $cpu_avg %"
echo "    CPU cao nhat        : $cpu_max %"
echo "    So luong cao nhat   : $threads_max (du kien 1 + N = $((N + 1)))"
echo "    So luong sau khi tat: $threads_after (phai la 1)"

if pgrep -f "$SERVER_BIN $PORT" >/dev/null; then
    echo "    CANH BAO: van con tien trinh server dang chay!"
    exit 1
fi

if [ "$threads_after" = "1" ]; then
    echo "    => KHONG ro ri luong"
else
    echo "    => CO the ro ri luong (con $threads_after luong)"
    exit 1
fi
