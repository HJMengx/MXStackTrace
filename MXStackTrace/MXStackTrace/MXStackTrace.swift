//
//  MXStackTrace.swift
//  MXStackTrace
//
//  Created by 贺靖 on 2019/3/20.
//  Copyright © 2019 贺靖. All rights reserved.
//

import Foundation
import Darwin.machine


// 拓展转 ASCII 码方法
extension Character {
    var isAscii: Bool {
        return unicodeScalars.allSatisfy { $0.isASCII }
    }
    var ascii: UInt32? {
        return isAscii ? unicodeScalars.first?.value : nil
    }
}

extension String {
    var ascii : [Int8] {
        var unicodeValues = [Int8]()
        for code in unicodeScalars {
            unicodeValues.append(Int8(code.value))
        }
        return unicodeValues
    }
}

class MXStatckTrace {
    // 首先获取到主线程的引用
    static let main_thread_t = mach_thread_self()
    
    // 开放接口
    class func getMainThreadStack() -> ([Substring], Double) {
        return MXStatckTrace.middleHandle(thread: Thread.main)
    }
    
    class func getThreadStatck(with thread : Thread) -> ([Substring], Double) {
        return MXStatckTrace.middleHandle(thread: thread)
    }
    
    // 中间步骤
    internal class func middleHandle(thread : Thread) -> ([Substring], Double) {
        return MXStatckTrace.mxStackTrace(with: MXStatckTrace.threadToMachThread(thread: thread))
    }
    
    // MARK: 进行Thread到 pthread_t的转换
    internal class func threadToMachThread(thread : Thread) -> thread_t {
        var name : [Int8] = [Int8]()
        // 线程数目
        var count : mach_msg_type_number_t = 0
        // 存储所有线程的函数
        var threads : thread_act_array_t?
        // 获取所有线程
        task_threads(mach_task_self_, &threads, &count)
        
        // 获取当前线程的名字
        let tread_name = thread.name
        
        // 判断是否是主线程, 我们已经获取过主线程了
        if (thread.isMainThread) {
            return self.main_thread_t
        }
        
        // 遍历所有的线程, 寻找名字相同的
        for index in 0..<count {
            let p_thread = pthread_from_mach_thread_np((threads![Int(index)]))
            if (p_thread != nil) {
                name.append(Int8(Character.init("\0").ascii!))
                // 获取名字
                pthread_getname_np(p_thread!, &name, MemoryLayout<Int8>.size * 256)
                // 相等的时候是0
                if (strcmp(&name, (thread.name!.ascii)) == 0) {
                    // 寻找到名字匹配的, 返回
                    thread.name = tread_name
                    return threads![Int(index)]
                }
            }
        }
        
        thread.name = tread_name
        
        return mach_thread_self()
    }
    
    // MARK: 通过 pthread_t 去获取栈信息
    internal class func mxStackTrace(with thread : thread_t, bufferSize : Int = 50) -> ([Substring], Double) {
        // 打桩时间
        let clangTime = Date().timeIntervalSince1970
        // 缓存的帧
        let tree = mx_StackTrace(thread, Int32(bufferSize))
        
        let function = withUnsafePointer(to: tree) { (ptr) -> String in
            let callStack = String.init(cString: ptr.pointee!)
            return callStack
        }
        
        let functions = function.split(separator: Character.init("\n"))
        
        return (functions, clangTime)
    }
}
