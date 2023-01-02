# isaac-danmu

这是在isaac中使用c++创建游戏mod的一次尝试。此mod为游戏的lua虚拟机提供bilibili弹幕的访问接口。
（原理已验证，暂未编码实现）

# Thread

该mod创建一个服务线程，用于收取bilibili的直播弹幕信息，并将信息发送至游戏内的lua虚拟机中。

# 接口说明

该mod新增全局变量`danmuB`为其接口。

# 发布说明

由于创意工坊不允许包含`dll`文件的mod，此mod无法上传至创意工坊。此外，需要以`--luadebug`参数启动游戏，使用时应知晓其安全风险。