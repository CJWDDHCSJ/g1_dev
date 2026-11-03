#!/usr/bin/env bash
# G1 磁盘与日志容量只读检查（P0-18.4）
# 只输出一行状态；不清理、不删除任何文件。
set -euo pipefail

ROOT_PCT=$(df -P / | awk 'NR==2 {gsub("%","",$5); print $5}')
USED=$(df -h -P / | awk 'NR==2 {print $3}')
AVAIL=$(df -h -P / | awk 'NR==2 {print $4}')
LOGDIR=$(du -sh /home/unitree/g1_logs_systemd 2>/dev/null | cut -f1)
BACKUPS=$(du -sh /home/unitree/g1_backups 2>/dev/null | cut -f1)
LARGEST=$(du -h /home/unitree/g1_logs_systemd/*.log 2>/dev/null | sort -h | tail -1 | awk '{print $1" "$2}')

if [ "$ROOT_PCT" -ge 90 ]; then STATE=CRITICAL
elif [ "$ROOT_PCT" -ge 80 ]; then STATE=WARNING
else STATE=OK; fi

echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) state=$STATE root_used=${ROOT_PCT}% used=$USED avail=$AVAIL logs=$LOGDIR backups=$BACKUPS largest_log=$LARGEST" | tee -a /home/unitree/g1_logs_systemd/disk_check.log
