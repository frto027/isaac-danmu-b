# isaac-danmu

这是在isaac中使用c++创建游戏mod的一次尝试。此mod为游戏的lua虚拟机提供bilibili弹幕的访问接口。

# How To Build

首先请安装[vcpkg](https://vcpkg.io/en/getting-started.html)，以安装编译c语言环境所需的依赖包，务必使用static的方式安装静态包：

```
vcpkg install zlib:x86-windows-static
vcpkg install brotli:x86-windows-static
```

使用vs2022的c++开发环境，打开解决方案，编译生成的dll放在本项目的danmuB/lib下面，danmuB就是提供api的mod。

将本项目的danmuB和example两个mod放在游戏mod文件夹，并使用--luadebug启动游戏即可。

example偷懒用了EID的字体，所以请开启EID。

开启游戏后，在控制台输入以下内容设置房间ID为1234：

```
danmuB.setRoom(1234)
```

# Thread

该mod创建一个服务线程，用于收取bilibili的直播弹幕信息，并将信息发送至游戏内的lua虚拟机中。lua虚拟机无需考虑多线程，线程之间使用无锁、无等待的方式进行同步。

# 接口说明

该mod新增全局变量`danmuB`为其接口。

```
danmuB.receive(handler)

该函数内部反复调用handler处理每一条缓存的、未处理的弹幕数据。handler签名：

function handler(text)
	...
end

text是服务器返回的json字符串
```

```
danmuB.setRoom(roomid)
设置房间号（数值改变则会重新发起连接）
roomid是整数值
```

```
danmuB.getPopularity()
返回房间人气值
返回值是整数值
```

# 发布说明

由于创意工坊不允许包含`dll`文件的mod，此mod无法上传至创意工坊。此外，需要以`--luadebug`参数启动游戏，使用时应知晓其安全风险。

# 性能/功能

目前lua回调得到的是服务器返回的json字符串，可以几乎受到所有消息，可能会有性能问题。如果确实存在，则后续可以考虑使用c语言做简单预处理。

# c库引用说明

- 使用wininet完成网络操作
- 使用lua对接游戏lua系统
- 使用jsoncpp解析json数据
- 使用zlib和brotli解码弹幕数据（需使用vcpkg手动安装）
