## 0. 前置：安装 venv 支持

Ubuntu 24.04 最小安装可能没带 `venv` 模块，先装：

```bash
sudo apt update
sudo apt install python3-venv python3-pip
```

如果创建时报 `ensurepip is not available`，装对应版本：

```bash
sudo apt install python3.12-venv
```

## 1. 创建虚拟环境

```bash
python3 -m venv myenv          # 在当前目录创建 myenv/
python3 -m venv .venv          # 常用隐藏目录名
```

常用选项：

```bash
python3 -m venv --system-site-packages myenv   # 继承系统已装的包
python3 -m venv --without-pip myenv            # 不安装 pip
python3 -m venv --upgrade-deps myenv           # 顺便升级 pip/setuptools
```

## 2. 激活 / 停用

```bash
source myenv/bin/activate      # 激活（bash/zsh）
. myenv/bin/activate           # 等价写法
deactivate                     # 退出虚拟环境
```

激活后提示符前会出现 `(myenv)`，可用以下命令确认：

```bash
echo $VIRTUAL_ENV              # 输出虚拟环境路径
which python                   # 应指向 myenv/bin/python
```

> fish shell 用 `source myenv/bin/activate.fish`；csh 用 `activate.csh`。

## 3. 在环境内安装和管理包

```bash
pip install requests
pip install -r requirements.txt
pip freeze > requirements.txt
pip list
pip uninstall requests
python -m pip install --upgrade pip
```

## 4. 不激活也能用（适合脚本/CI）

```bash
myenv/bin/python script.py
myenv/bin/pip install requests
```

## 5. 删除虚拟环境

venv 就是普通目录，直接删：

```bash
deactivate        # 先退出（如果在里面）
rm -rf myenv
```

## 6. 其他常用

```bash
python3 -m venv --help         # 查看所有创建选项
python -m venv --upgrade myenv # 升级环境内的 Python 软链接（换 Python 版本后）
```

## 典型完整流程

```bash
cd ~/projects/myapp
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install flask
pip freeze > requirements.txt
# ...开发...
deactivate
```