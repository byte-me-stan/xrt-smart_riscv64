## 哪吒开发板：D1
- 全志 Allwinner D1 开发板，部分资料可以在[全志在线开发者社区](https://www.aw-ol.com/downloads/resources/31)下载，注册一个账号即可

- 开发板默认支持Linux，这里移植到RT-Thread Smart


## RT-Thread Smart
- [RT-Thread Smart 入门指南](https://www.rt-thread.org/document/site/#/rt-thread-version/rt-thread-smart/rt-smart-quickstart/rt-smart-quickstart)
- 参考rt-smart的BSP：[https://gitee.com/rtthread/rt-thread/tree/rt-smart/bsp/d1-allwinner-nezha](https://gitee.com/rtthread/rt-thread/tree/rt-smart/bsp/d1-allwinner-nezha)，注意切到 rt-smart git 分支


## 环境搭建
- ubuntu 20.04 ：`scons --menuconfig ` 配置 `scons`编译通过，RISCV64交叉编译工具链：
- `source smart-env.sh riscv64`

```c
# riscv64-unknown-linux-musl-gcc -v
Using built-in specs.
COLLECT_GCC=riscv64-unknown-linux-musl-gcc
COLLECT_LTO_WRAPPER=/home/rtthread/rt-thread/tools/gnu_gcc/riscv64-linux-musleabi_for_x86_64-pc-linux-gnu/bin/../libexec/gcc/riscv64-unknown-linux-musl/10.1.0/lto-wrapper
Target: riscv64-unknown-linux-musl
Configured with: /builds/alliance/risc-v-toolchain/riscv-gcc/configure --target=riscv64-unknown-linux-musl --prefix=/builds/alliance/risc-v-toolchain/install-native/ --with-sysroot=/builds/alliance/risc-v-toolchain/install-native//riscv64-unknown-linux-musl --with-system-zlib --enable-shared --enable-tls --enable-languages=c,c++ --disable-libmudflap --disable-libssp --disable-libquadmath --disable-libsanitizer --disable-nls --disable-bootstrap --src=/builds/alliance/risc-v-toolchain/riscv-gcc --disable-multilib --with-abi=lp64 --with-arch=rv64imafdc --with-tune=rocket 'CFLAGS_FOR_TARGET=-O2   -mcmodel=medany' 'CXXFLAGS_FOR_TARGET=-O2   -mcmodel=medany'
Thread model: posix
Supported LTO compression algorithms: zlib
gcc version 10.1.0 (GCC) 
```

## 用户apps
- 待续

## windows 编译环境

- 打开 RT-Thread ENV 工具，menuconfig 保存可能修改会比较大，这部分后面研究如何规避，Linux 环境下还算正常

- `smart-env.bat` 配置好 交叉工具链

- `scons` 编译，如果编译不过，可能是Git 拉取代码引起的【回车换行符】导致，这个问题后面有时间确认下

- `mkimg.bat` 生成 img 文件（当前u-boot 引导好像存在问题，TODO）


## 经验分享

- RT-Thread 社区：[https://club.rt-thread.org/index.html](https://club.rt-thread.org/index.html)
- CSDN博客 [https://zhangsz.blog.csdn.net/](https://zhangsz.blog.csdn.net/)
