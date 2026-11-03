# 日志轮转激活步骤（需先安装 logrotate）

当前状态：STAGED（未激活）。原因：G1 未安装 logrotate（dpkg 状态 un，apt 候选 3.19.0-1ubuntu1.1）。
按任务书要求未擅自安装系统包。安装 logrotate 需要 sudo 密码，由用户执行：

    sudo apt-get install -y logrotate

安装完成后执行以下命令激活（全部为用户级，无需 sudo）：

    mkdir -p ~/.config/logrotate ~/.local/state/logrotate ~/g1_logs_systemd
    cp ~/g1_systemd/logrotate_staged/g1-voice-stack.conf ~/.config/logrotate/
    cp ~/g1_systemd/logrotate_staged/g1-log-rotate.service ~/.config/logrotate/../systemd/user/ 2>/dev/null ||       cp ~/g1_systemd/logrotate_staged/g1-log-rotate.service ~/.config/systemd/user/
    cp ~/g1_systemd/logrotate_staged/g1-log-rotate.timer ~/.config/systemd/user/
    systemctl --user daemon-reload
    logrotate -d -s ~/.local/state/logrotate/g1-voice-stack.status ~/.config/logrotate/g1-voice-stack.conf   # 仅调试，不实际轮转
    systemd-analyze --user verify ~/.config/systemd/user/g1-log-rotate.service ~/.config/systemd/user/g1-log-rotate.timer
    systemctl --user enable --now g1-log-rotate.timer
    systemctl --user start g1-log-rotate.service
    systemctl --user status g1-log-rotate.timer g1-log-rotate.service

注意：
- 生产 ROS 节点不重启；copytruncate 会让现有进程继续写同一路径。
- copytruncate 在复制与截断之间有极短窗口，可能丢失极少日志，属已知边界。
- 若 logrotate 报  相关权限错误，启用 conf 中被注释的  行。
- 首次真实轮转建议在业务空闲时手工执行一次并检查五个日志文件是否重新增长。
