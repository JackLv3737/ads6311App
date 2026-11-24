#include "bootloader.h"

int main(void) {
  bootloader_init();
  while (1) {
    bootloader_run();
  }
  return 0;
}
// TODO 读写flash了解flash的分布情况
// TODO 研究时钟树
// TODO 变量存放的位置，变量的命名
// TODO debug信息的调整
// TODO 全局变量与非全局变量最后再弄
// TODO 禁止修改的寄存器做保护，有些寄存器的值需要做保护
// TODO 只打印必要的log，不必要的log被debug_mode控制
// TODO 相同的文件用同一个
// TODO 写完flash要跟mcu_reg对比一下
// TODO 打印当前运行模式，是否为debug模式