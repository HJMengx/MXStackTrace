//
//  MXStackThread.h
//  MXStackTrace
//
//  Created by 贺靖 on 2019/3/21.
//  Copyright © 2019 贺靖. All rights reserved.
//  如果直接包含.c文件, 会有多重定义的问题, 每一次包含, 编译器会重新定义内部的函数.

#ifndef MXStackThread_h
#define MXStackThread_h

#import <mach/mach.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/types.h>
#include <limits.h>
#include <string.h>
#include <inttypes.h>
#include <mach-o/dyld.h>
#include <mach-o/nlist.h>
#include <stdio.h>
#include <stdlib.h>

size_t getMXStackFrameSize(void);

// MARK: 获取寄存器地址
uintptr_t getRegisterAddress(_STRUCT_MCONTEXT* mainContext);

// MARK: 获取线程的状态
bool fillThreadStateIntoMachineContext(thread_t thread, _STRUCT_MCONTEXT* mainContext);

// MARK: 获取线程的指令地址(StackPointer)
uintptr_t getInstructionAddress(_STRUCT_MCONTEXT* mainContext);

// MARK: 获取栈帧地址
uintptr_t getFramePointer(_STRUCT_MCONTEXT* mainContext);

// MARK: 拷贝栈帧的内容
kern_return_t machCopyMem(const void *const src, void *const dst, const size_t numBytes);

// MARK: 解析递归树
char* stackTraceTree(const int entryNum,
                           const uintptr_t address,
                           const Dl_info* const dlInfo);

// MARK: 寻找镜像文件, 获取符号名
void convertToSymbol(const uintptr_t* const backtraceBuffer,
                     Dl_info* const symbolsBuffer,
                     const int numEntries,
                     const int skippedEntries);

// MARK: 解析栈
char* mx_StackTrace(thread_t thread, int bufferSize);

#endif /* MXStackThread_h */
