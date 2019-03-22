//
//  MXStackTraceThread.c
//  MXStackTrace
//
//  Created by 贺靖 on 2019/3/20.
//  Copyright © 2019 贺靖. All rights reserved.
//  申请的堆空间记得释放

#include "MXStackTraceThread.h"

// 根据不同的系统设置不同的
#pragma -mark DEFINE MACRO FOR DIFFERENT CPU ARCHITECTURE
#if defined(__arm64__)
#define DETAG_INSTRUCTION_ADDRESS(A) ((A) & ~(3UL))
#define THREAD_STATE_COUNT ARM_THREAD_STATE64_COUNT
#define THREAD_STATE ARM_THREAD_STATE64
#define FRAME_POINTER __fp
#define STACK_POINTER __sp
#define INSTRUCTION_ADDRESS __pc

#elif defined(__arm__)
#define DETAG_INSTRUCTION_ADDRESS(A) ((A) & ~(1UL))
#define THREAD_STATE_COUNT ARM_THREAD_STATE_COUNT
#define THREAD_STATE ARM_THREAD_STATE
#define FRAME_POINTER __r[7]
#define STACK_POINTER __sp
#define INSTRUCTION_ADDRESS __pc

#elif defined(__x86_64__)
#define DETAG_INSTRUCTION_ADDRESS(A) (A)
#define THREAD_STATE_COUNT x86_THREAD_STATE64_COUNT
#define THREAD_STATE x86_THREAD_STATE64
#define FRAME_POINTER __rbp
#define STACK_POINTER __rsp
#define INSTRUCTION_ADDRESS __rip

#elif defined(__i386__)
#define DETAG_INSTRUCTION_ADDRESS(A) (A)
#define THREAD_STATE_COUNT x86_THREAD_STATE32_COUNT
#define THREAD_STATE x86_THREAD_STATE32
#define FRAME_POINTER __ebp
#define STACK_POINTER __esp
#define INSTRUCTION_ADDRESS __eip

#endif

#define CALL_INSTRUCTION_FROM_RETURN_ADDRESS(A) (DETAG_INSTRUCTION_ADDRESS((A)) - 1)

#if defined(__LP64__)
#define TRACE_FMT         "%-4d%-31s 0x%016lx %s + %lu"
#define POINTER_FMT       "0x%016lx"
#define POINTER_SHORT_FMT "0x%lx"
#define NLIST struct nlist_64
#else
#define TRACE_FMT         "%-4d%-31s 0x%08lx %s + %lu"
#define POINTER_FMT       "0x%08lx"
#define POINTER_SHORT_FMT "0x%lx"
#define NLIST struct nlist
#endif

#define MAXLENGTH 256

// MARK: 声明结构体类型
// 回溯栈帧(链表)
struct MXStackFrame {
    // 存储前一个栈帧
    const struct MXStackFrame *const preFrame;
    // 存储
    uintptr_t back_address;
};

// MARK: 获取寄存器地址
uintptr_t getRegisterAddress(_STRUCT_MCONTEXT* mainContext) {
#if defined(__i386__) || defined(__x86_64__)
    return 0;
#else
    return mainContext->__ss.__lr;
#endif
}

// MARK: 获取大小
size_t getMXStackFrameSize() {
    struct MXStackFrame compute = {0};
    return sizeof(compute);
}

// MARK: 获取当前线程所有的信息, 信息都保存在__darwin_mcontext64
bool fillThreadStateIntoMachineContext(thread_t thread, _STRUCT_MCONTEXT* mainContext) {
    mach_msg_type_number_t stateCount = THREAD_STATE_COUNT;
    kern_return_t kernelState = thread_get_state(thread, THREAD_STATE, (thread_state_t)&mainContext->__ss, &stateCount);
    return kernelState == KERN_SUCCESS;
}

// MARK: 获取指令地址
uintptr_t getInstructionAddress(_STRUCT_MCONTEXT* mainContext) {
    return mainContext->__ss.INSTRUCTION_ADDRESS;
}

// MARK: 获取栈帧地址
uintptr_t getFramePointer(_STRUCT_MCONTEXT* mainContext) {
    return mainContext->__ss.FRAME_POINTER;
}

// MARK: 拷贝栈帧的内容
kern_return_t machCopyMem(const void *const src, void *const dst, const size_t numBytes) {
    vm_size_t bytesCopied = 0;
    return vm_read_overwrite(mach_task_self(), (vm_address_t)src, (vm_size_t)numBytes, (vm_address_t)dst, &bytesCopied);
}

// MARK: 获取符号名
void convertToSymbol(const uintptr_t* const backtraceBuffer,
                     Dl_info* const symbolsBuffer,
                     const int numEntries,
                     const int skippedEntries) {
    
//     typedef struct dl_info {
//     const char         *dli_fname;     /* Pathname of shared object */
//        void            *dli_fbase;     /* Base address of shared object */
//        const char      *dli_sname;     /* Name of nearest symbol */
//        void            *dli_saddr;     /* Address of nearest symbol */
//    } Dl_info;
    
    int i = 0;
    
    if(!skippedEntries && i < numEntries) {
        // 解析到该地址
        dladdr((void *)backtraceBuffer[i], &symbolsBuffer[i]);
        i++;
    }
    
    for(; i < numEntries; i++) {
        dladdr((void *)CALL_INSTRUCTION_FROM_RETURN_ADDRESS(backtraceBuffer[i]), &symbolsBuffer[i]);
    }
}

// MARK: 解析递归树
const char* getLastEntry(const char* const path) {
    if(path == NULL || strlen(path) == 0) {
        return NULL;
    }
    
    char* lastFile = strrchr(path, '/');
    return lastFile == NULL ? path : lastFile + 1;
}

char* stackTraceTree(const int entryNum,
                     const uintptr_t address,
                     const Dl_info* const dlInfo) {
    // 文件地址缓冲
    char faddrBuff[20];
    // 符号地址缓冲
    char saddrBuff[20];
    
    // 获取文件名
    const char* fname = getLastEntry(dlInfo->dli_fname);
    
    if(fname == NULL) {
        sprintf(faddrBuff, POINTER_FMT, (uintptr_t)dlInfo->dli_fbase);
        fname = faddrBuff;
    }
    // 获取代码便宜了
    uintptr_t offset = address - (uintptr_t)dlInfo->dli_saddr;
    const char* sname = dlInfo->dli_sname;
    if(sname == NULL) {
        sprintf(saddrBuff, POINTER_SHORT_FMT, (uintptr_t)dlInfo->dli_fbase);
        sname = saddrBuff;
        offset = address - (uintptr_t)dlInfo->dli_fbase;
    }
    // 合并内容
    char* result= calloc(256, sizeof(char*));
    sprintf(result, "%s  %s  %s  %lu\n", fname,  sname, "+", offset);
    return result;
}

// MARK: 解析栈
char* mx_StackTrace(thread_t thread, int bufferSize) {
    // 计数器
    int index = 0;
    // 初始化二维数组, 代表缓存函数
//    char result[bufferSize][MAXLENGTH];
    char* result = calloc(bufferSize * MAXLENGTH, sizeof(char));
    // 缓存回退地址
    uintptr_t* backTraceBuffer = calloc(bufferSize, sizeof(uintptr_t));
    // 线程上下文(内核级)
    _STRUCT_MCONTEXT mainContext;
    // 判断是否能获取到该线程的信息
    if (!fillThreadStateIntoMachineContext(thread, &mainContext)) {
        return (char*)("无法找到该线程的信息");
    }
    // 获取指令地址, 栈的位置
    uintptr_t instructionsAddress = getInstructionAddress(&mainContext);
    // 判断指令地址是否存在
    if (instructionsAddress == 0 ) {
        return (char*)("无法获取该线程的地址");
    }
    // 放入缓存中, 当前的栈帧
    backTraceBuffer[index] = instructionsAddress;
    index++;
    // 在寄存器中的地址
    uintptr_t registerAddress = getRegisterAddress(&mainContext);
    // 如果有寄存器地址(置顶操作系统下, 会存储寄存器那边的值)
    if (registerAddress != 0) {
        backTraceBuffer[index] = registerAddress;
        index++;
    }
    // 获取栈帧地址
    const uintptr_t framePointer = getFramePointer(&mainContext);
    // 拷贝的目的地址
    struct MXStackFrame frame = {0};
    // 检测栈帧是否合法
    if (framePointer == 0 || machCopyMem((void *)framePointer, &frame, sizeof(frame)) != KERN_SUCCESS) {
        return (char*)("无法获取到栈帧地址");
    }
    // 将栈帧完善, 循环(递归)
    for (; index < bufferSize; index++) {
        backTraceBuffer[index] = frame.back_address;
        if(backTraceBuffer[index] == 0 ||
           frame.preFrame == 0 ||
           machCopyMem(frame.preFrame, &frame, sizeof(frame)) != KERN_SUCCESS){
            break;
        }
    }
    // 回溯解析出符号表
    int symbolCounts = index;
    Dl_info* symbolicated = calloc(symbolCounts, sizeof(Dl_info));
    // 得到符号表
    convertToSymbol(backTraceBuffer, symbolicated, symbolCounts, 0);
    // 解析成指定的格式
    for (int index = 0; index < symbolCounts; index++) {
        char* frameResult = stackTraceTree(index, backTraceBuffer[index], &symbolicated[index]);
        strcat(result, frameResult);
        // 释放堆内存
        free(frameResult);
    }
    // 释放内存
    free(backTraceBuffer);
    free(symbolicated);
    // 通过 '\n' 分隔的调用栈
    return result;
}


