
# 清除一段文字中所有的引用和空格

# 内存页面跟踪是跟踪虚拟机内存访问的关键机制,以便管理程序(或虚拟机监视器)可以改进其实现的各种服务的内存管理.内存页面跟踪是几个基本任务的核心,例如用于故障后恢复的检查点 [62]、用于维护和动态打包的实时迁移 [28],以及用于内存过量使用 [12] 的工作集大小 (WSS)1 估计 [31] 和快速恢复[62].

# ->

# 内存页面跟踪是跟踪虚拟机内存访问的关键机制,以便管理程序(或虚拟机监视器)可以改进其实现的各种服务的内存管理.内存页面跟踪是几个基本任务的核心,例如用于故障后恢复的检查点、用于维护和动态打包的实时迁移,以及用于内存过量使用的工作集大小(WSS)1估计和快速恢复.

import re


def main():

    try:
        
        while True:
            word = input("input: ")
            pattern = re.compile(r'\[\d+\]')
            cleaned_word = re.sub(pattern, '', word)
            cleaned_text = re.sub(r'\s+', '', cleaned_word)
            print(f'\n{cleaned_text}')
    except KeyboardInterrupt:
        ...
    
    
    
    
if __name__ == "__main__":
    main()