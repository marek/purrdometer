#!/bin/bash
# btdiag.sh — diagnostic: log HCI traffic (btmon) and, whenever an LE link settles on a short
# supervision timeout, re-issue LE Connection Update with a longer one.
# Usage: sudo ./btdiag.sh [timeout_10ms_units=600] [logfile]
TO=${1:-600}
LOG=${2:-/tmp/bleguard-$(date +%Y%m%d-%H%M%S).btmon.txt}
echo "logging btmon to $LOG; target supervision timeout=$((TO*10))ms"

handle=""; interval=""
stdbuf -oL btmon -T 2>&1 | tee "$LOG" | while IFS= read -r line; do
  case "$line" in
    *"Disconnect Complete"*) echo "$(date +%T) DISCONNECT" ;;
    *"Reason:"*) echo "$(date +%T) $line" ;;
    *"LE Connection Complete"*|*"LE Connection Update Complete"*) want=1 ;;
    *"Handle:"*)
      [[ $want == 1 ]] && handle=$(sed -E 's/.*Handle: ([0-9]+).*/\1/' <<<"$line") ;;
    *"Connection interval:"*)
      [[ $want == 1 ]] && interval=$(sed -E 's/.*\(0x([0-9a-f]+)\).*/\1/' <<<"$line") ;;
    *"Supervision timeout:"*msec*)
      if [[ $want == 1 ]]; then
        want=0
        cur=$(( 16#$(sed -E 's/.*\(0x([0-9a-f]+)\).*/\1/' <<<"$line") ))
        iv=$(( 16#$interval ))
        echo "$(date +%T) link handle=$handle interval=$((iv*125/100))ms timeout=$((cur*10))ms"
        if (( cur < TO )); then
          echo "$(date +%T) -> extending supervision timeout to $((TO*10))ms"
          ( sleep 0.3; hcitool lecup --handle "$handle" --min "$iv" --max "$iv" \
              --latency 0 --timeout "$TO" ) &
        fi
      fi ;;
    *"Cycling Speed and Cadence Measurement"*|*"Handle Value Notification"*) echo "$(date +%T) NOTIFY" ;;
  esac
done
