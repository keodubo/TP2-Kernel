#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
LOG_DIR="$REPO_ROOT/.mem-test-logs"
mkdir -p "$LOG_DIR"

REQUIRED_CMDS=("qemu-system-x86_64" "expect" "grep" "awk" "sed")

missing=()
for cmd in "${REQUIRED_CMDS[@]}"; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        missing+=("$cmd")
    fi
done

if ((${#missing[@]} > 0)); then
    echo "[mem-tests] Missing tools: ${missing[*]}" >&2
    echo "Install the missing commands and re-run." >&2
    exit 1
fi

cleanup_qemu() {
    if [[ -n ${QEMU_PID:-} ]]; then
        if kill -0 "$QEMU_PID" 2>/dev/null; then
            kill "$QEMU_PID" 2>/dev/null || true
            sleep 1
            kill -9 "$QEMU_PID" 2>/dev/null || true
        fi
    fi
}

trap cleanup_qemu EXIT

build_world() {
    local mode=$1
    echo "[mem-tests] Building ($mode)" >&2
    (cd "$REPO_ROOT" && make clean >/dev/null 2>&1 || true)
    if [[ "$mode" == "buddy" ]]; then
        (cd "$REPO_ROOT" && make buddy)
    else
        (cd "$REPO_ROOT" && make)
    fi
}

run_suite() {
    local mode=$1
    local logfile="$LOG_DIR/mem_${mode}.log"
    rm -f "$logfile"

    echo "[mem-tests] Booting ($mode)" >&2
    LOGFILE="$logfile" expect <<'EOEXP'
    log_user 1
    log_file $env(LOGFILE)
    set timeout 180
    spawn qemu-system-x86_64 -hda Image/x64BareBonesImage.qcow2 -m 512 -nographic
    expect "$ "
    send "mem\r"
    expect "$ "
    send "mem -v\r"
    expect "$ "
    send "exit\r"
    expect {
        eof {}
        timeout {
            send "\001x"
            expect eof
        }
    }
EOEXP

    echo "[mem-tests] Captured log at $logfile" >&2
}

strip_digits() {
    local value="$1"
    if [[ "$value" != *.* ]]; then
        printf '%s00' "$value"
        return
    fi
    local int_part="${value%%.*}"
    local frac_part="${value#*.}"
    if [[ "$int_part" == "$value" ]]; then
        int_part="0"
        frac_part="0"
    fi
    frac_part="${frac_part:0:2}"
    if ((${#frac_part} == 1)); then
        frac_part="${frac_part}0"
    elif ((${#frac_part} == 0)); then
        frac_part="00"
    fi
    printf '%s%s' "$int_part" "$frac_part"
}

check_simple() {
    local log="$1"
    echo "[mem-tests] Validating simple allocator" >&2
    local allocator=$(grep -m1 '^allocator:' "$log" | awk '{print $2}')
    if [[ "$allocator" != "simple" ]]; then
        echo "simple allocator expected, saw '$allocator'" >&2
        return 1
    fi
    local heap=$(grep -m1 '^heap:' "$log" | awk '{print $2}')
    local used=$(grep -m1 '^used:' "$log" | awk '{print $2}')
    local free=$(grep -m1 '^free:' "$log" | awk '{print $2}')
    local largest=$(grep -m1 '^largest:' "$log" | awk '{print $2}')
    local frag=$(grep -m1 '^frag:' "$log" | awk '{print $2}')
    if [[ -z "$heap" || -z "$used" || -z "$free" ]]; then
        echo "missing counters in simple log" >&2
        return 1
    fi
    local total=$((used + free))
    local diff=$((heap > total ? heap - total : total - heap))
    local tolerance=$((heap / 4 + 1))
    if ((diff > tolerance)); then
        echo "simple invariance violated: heap=$heap used=$used free=$free" >&2
        return 1
    fi
    if ((largest > free)); then
        echo "largest block exceeds total free (largest=$largest free=$free)" >&2
        return 1
    fi
    if [[ -n "$frag" && "$free" != "0" ]]; then
        local wasted=$((free > largest ? free - largest : 0))
        local scaled=$(((wasted * 100 + free / 2) / free))
        local reported=$(strip_digits "$frag")
        if ((scaled != 10#$reported)); then
            echo "fragmentation mismatch (calc=$scaled reported=$reported)" >&2
            return 1
        fi
    fi
    if ! grep -q '^freelist (up to' "$log"; then
        echo "freelist header missing in verbose output" >&2
        return 1
    fi
}

check_buddy() {
    local log="$1"
    echo "[mem-tests] Validating buddy allocator" >&2
    local allocator=$(grep -m1 '^allocator:' "$log" | awk '{print $2}')
    if [[ "$allocator" != "buddy" ]]; then
        echo "buddy allocator expected, saw '$allocator'" >&2
        return 1
    fi
    local heap=$(grep -m1 '^heap:' "$log" | awk '{print $2}')
    local used=$(grep -m1 '^used:' "$log" | awk '{print $2}')
    local free=$(grep -m1 '^free:' "$log" | awk '{print $2}')
    local largest=$(grep -m1 '^largest:' "$log" | awk '{print $2}')
    local frag=$(grep -m1 '^frag:' "$log" | awk '{print $2}')
    if [[ -z "$heap" || -z "$used" || -z "$free" ]]; then
        echo "missing counters in buddy log" >&2
        return 1
    fi
    local total=$((used + free))
    local diff=$((heap > total ? heap - total : total - heap))
    local tolerance=4096
    if ((diff > tolerance)); then
        echo "buddy invariance violated: heap=$heap used=$used free=$free" >&2
        return 1
    fi
    if ((largest > free)); then
        echo "largest block exceeds total free (largest=$largest free=$free)" >&2
        return 1
    fi
    local computed=0
    while read -r line; do
        if [[ "$line" =~ ^[[:space:]]*([0-9]+)[[:space:]]+([0-9]+)[[:space:]]+([0-9]+) ]]; then
            local order_size=${BASH_REMATCH[2]}
            local order_count=${BASH_REMATCH[3]}
            computed=$((computed + order_size * order_count))
        fi
    done < <(awk '/^order  size/{flag=1;next}/^  [0-9]/{if(flag) print $0}/^$/{flag=0}' "$log")
    if ((computed != free)); then
        echo "buddy free list mismatch (sum=$computed free=$free)" >&2
        return 1
    fi
    if [[ -n "$frag" && "$free" != "0" ]]; then
        local wasted=$((free > largest ? free - largest : 0))
        local scaled=$(((wasted * 100 + free / 2) / free))
        local reported=$(strip_digits "$frag")
        if ((scaled != 10#$reported)); then
            echo "buddy fragmentation mismatch (calc=$scaled reported=$reported)" >&2
            return 1
        fi
    fi
}

main() {
    build_world simple
    run_suite simple
    check_simple "$LOG_DIR/mem_simple.log"

    build_world buddy
    run_suite buddy
    check_buddy "$LOG_DIR/mem_buddy.log"

    echo "[mem-tests] All checks passed" >&2
}

main "$@"
