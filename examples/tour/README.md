# 配置化导览示例

`tour.cpp` 使用现有 `SlamClient`、`AudioClient`、`LocoClient`，依次完成：加载指定场景并定位、
导航到点位、播放 WAV 讲解，并在语音期间按配置触发一次性动作，再前往下一点。无需新增机器人端 RPC 接口。

配置文件为 [../config/tour.json](../config/tour.json)。默认场景 ID 为用户指定的
`2112734270341910528`。附件原场景 ID 为 `2110300321325387776`，本示例只提取其坐标；
使用前需确认这些坐标适用于目标地图。附件 `pointCnt` 为 15，但 `pointList` 实际为 14 条，
初始坐标按 `orderNo` 排序提取全部 14 条，原记录包含点位9之后的“点位16”，未补造缺失点位。
当前点位名称、音频关联和动作以配置文件为准，可按实际讲解内容修改。

## 点位和讲解配置

修改 `waypoints` 数组即可调整顺序和点位；数组顺序就是执行顺序。以下为单个点位的配置形式：

```json
{
    "name": "点位1",
    "coordinateX": 3.888884351242676,
    "coordinateY": -5.203282964374892,
    "coordinateA": 1.5707963267948966,
    "enabled": true,
    "wav_file": "/opt/app/audio/tour/点位1.wav",
    "actions": [
        {"model": "t4_zl1", "delay_s": 0}
    ],
    "wait_after_s": 0
}
```

| 配置 | 说明 |
| --- | --- |
| `coordinateX` / `coordinateY` | 地图坐标，米；导航使用绝对坐标，Z 为 0 |
| `coordinateA` | Yaw，弧度，直接使用附件数值 |
| `enabled` | `false` 时跳过该点 |
| `wav_file` | **机器人音频播放器主机上的绝对路径**；通过 `AudioClient::PlayWavFile` 播放，不从 SDK 主机上传 |
| `actions` | 语音期间按数组顺序下发的动作编排，空数组表示只播放语音；每项可指定不同模型 |
| `actions[].model` | 本步骤的 `t4_zl*` 一次性动作模型；同一模型可在多个步骤中出现 |
| `actions[].delay_s` | 下发该步骤前的等待间隔，秒，默认 0，不能为负；首项相对语音调用开始，后续项相对上一动作请求开始 |
| `action_model` / `action_once` | 兼容旧单模型配置，不与 `actions` 同时使用。`t4_zl*` 转为一个立即下发的编排步骤；其他循环动作按 `action_once` 播放一轮或在语音结束后退出循环，并等待回到行走模型 |
| `wait_after_s` | 讲解流程结束后的额外停留秒数，默认 0 |

每个点位通过 `wav_file` 显式关联音频，例如“点位20”使用 `/opt/app/audio/tour/点位20.wav`，
程序不按数组位置推导文件名。SDK 不附带音频文件，运行前需将对应 WAV 部署到播放器主机。
`tts_outputs.zip` 中的 24 个同名 WAV 可用于此配置。点位与动作的对应关系由各点的
`actions` 中每个步骤的 `model` 指定，空数组时只播放语音。可从以下模型中选择，填入对应步骤：

| 模型 | 动作 |
| --- | --- |
| `t4_zl2` | 交互型解释(表现逻辑性较强的思考) |
| `t4_zl1` | 介绍讲解(无展示物) |
| `t4_zl5` | 右后方介绍 |
| `t4_zl4` | 正后方介绍(先右手，后左手) |
| `t4_zl6` | 左后方介绍 |

配置内的 `action_options` 仅列出可选动作，执行时以点位的 `actions` 编排为准。
若配置了动作，程序会在导航前查询模型列表并检查全部步骤，缺少任一步骤所需模型时退出。

## 场景、超时和导航结果

| 配置 | 默认值 | 说明 |
| --- | --- | --- |
| `scene_id` | `2112734270341910528` | 整条路线使用的导航场景 ID |
| `navigation_model` | `t4_walk` | 导航所需行走模型；随附配置使用 `z01_pa_walk`，以实际配置为准 |
| `initialize_localization` | `true` | 仅在 SLAM 为 `IDLE` 时调用 `InitPose(scene_id, true, ...)` 自动定位；`false` 要求事先定位 |
| `rpc_timeout_s` | 10 | SLAM / 运控单次调用超时，秒 |
| `audio_timeout_s` | 120 | WAV 同步播放调用超时，秒；应大于最长音频时长，也需与机器人端音频桥接超时匹配 |
| `localization_timeout_s` | 120 | 等待定位就绪超时，秒 |
| `navigation_timeout_s` | 300 | 等待本次导航最终结果的超时，秒 |
| `action_timeout_s` | 60 | 等待导航行走模型、其他循环动作模型切换或结束的超时，秒；不用于等待 `t4_zl*` 动作名称 |
| `poll_interval_s` | 0.5 | 状态查询间隔，秒 |

程序复用指定场景的 `NAV_READY` 定位，也可等待同场景的 `NAV_STARTING` 完成。
已有其他场景、建图或导航任务时退出，不自动关闭或接管已有任务。
机器人应事先处于适合导航的运动状态、使用指定行走模型，并已设置自动导航配置；
本示例不自动起身、清除故障、切换运控模式或切换导航配置。
导览期间由本程序独占导航和动作控制，避免其他控制端同时下发目标。

`PoseNav` 返回 `0` 表示接受请求，程序继续等待 `zv_api` 返回同一请求的最终结果。
`1` 表示导航成功，此时开始讲解；`2` 表示失败，打印原始响应并结束导览。
到达、位置、朝向及定位误差的判断由导航后端负责，示例不再查询位姿或设置到达阈值，
也不根据 `NAV_READY` 判定成功或跳过导航请求。

普通 SDK RPC 调用在收到首条响应后返回。为接收 `0` 之后的最终结果，示例通过
`ChannelSubscriber<Response_>` 监听同一 `rt/rpc/slam/response` 话题，
按本次导航请求 ID 匹配终态，无需额外转发原生 `/slam/status` 话题。
失败、通信异常或等待超时均结束导览，不自动重发、恢复导航或等待人工确认。

`t4_zl*` 为一次性动作：启动 WAV 播放后，按 `actions` 数组顺序及各步骤的 `delay_s`
通过 `SetModel` 下发，可依次执行不同模型。不等待 `active_rl_model` 变成动作名称，
也不发送 `FinishLoop`、暂停或切换回行走模型的命令。
每个点位最多配置两个动作；后一动作必须等前一动作完整结束后再下发，且整个动作必须在语音结束前播完，
否则删除该动作。`delay_s` 应不小于前一动作时长加 0.5 秒余量，并优先落在话术的自然停顿处。
各模型时长见 `action_options` 中的标注。
上例点位1语音约 34.8 秒，`t4_zl1` 时长 12.16 秒；剩余时间不足以完整播放原第二个 `t4_zl2`（23.76 秒），
因此只保留一个动作。语音结束即停止下发未启动的后续步骤。
其他循环动作沿用 `action_once`，确认模型名称匹配后播放语音，并等待动作回到 `navigation_model`。
WAV 调用同步等待播放器完成，成功要求 RPC 返回 0 且 `result.success` 为 `true`。
语音播放超时、动作请求失败或用户中断时停止后续下发与后续点位；已发出的同步音频请求仍等待完成或超时。

## 编译和运行

在已配置 SDK 源码构建目录中编译：

```bash
cmake -S . -B build -DBUILD_EXAMPLES=ON
cmake --build build --target tour -j 3

# 只查看配置和执行顺序，不初始化 DDS，不控制机器人。
./build/examples/tour --config=examples/config/tour.json --check=true

# WAV 文件准备完毕、定位/运控条件满足后再执行。DDS 两种方式二选一。
./build/examples/tour --config=examples/config/tour.json --use_config
./build/examples/tour --config=examples/config/tour.json --network_interface=eth0
```

不传 `--config` 时，从可执行文件所在目录向上查找 `config/tour.json`，兼容
`examples/build`、制品的 `examples/bin/<arch>` 和安装后的示例目录。
构建目录不在项目下时请显式指定 `--config`。DDS 文件模式沿用
`examples/config/dds.json`；网卡模式默认自动选择，域 ID 由 `ZV_DDS_DOMAIN_ID` 设置，
未设置时为 0。修改 DDS 配置后需重新启动程序。

## 运行日志

启动时输出 DDS 配置来源、查询间隔与导航结果等待超时。运行日志带机器人本机时间
（含时区偏移）和本次导览已用秒数，便于与机器人端日志对照。
运控及 SLAM 查询状态在变化时输出；导航等待期间每 2 秒输出请求 ID 和等待时间。

每次导航开始时输出点位名称及目标 X/Y/Yaw。
`InitPose`、`PoseNav`、导航最终结果和取消导航会打印返回码及原始响应，
保留后端请求 ID 和错误说明；WAV 播放返回时输出成功标志、音频时长、调用耗时和错误说明。

动作编排记录每个步骤的模型名、序号、等待间隔、`SetModel` 返回码、调用耗时及最终已下发步骤数。
`SetModel` 返回 `0` 只表示 `zv_api` 已接受并转发请求；模型列表也来自 `zv_api` 配置，
不能据此确认模型已实际执行。`t4_zl*` 不再进入动作模型名称等待。
其他循环动作保留 `SetModel`、`FinishLoop` 和模型等待日志；等待时输出期望模型、实际模型、
系统状态、模式和控制来源，超时报错附带最后一次运控状态。

部署后在 `current/examples` 目录可同时查看和保存日志：

```bash
set -o pipefail
./bin/tour --config=config/tour.json --use_config 2>&1 | tee tour.log
```

导航失败后的取消请求可能因当前已无导航任务而返回失败；
取消响应是清理结果，判断原因应先查看本次导航最终结果中的返回码和原始响应。

Ctrl+C / SIGTERM 或错误退出时，程序尝试取消自身发起的导航；自身循环讲解动作仍在播放时尝试暂停。
`t4_zl*` 一次性动作停止后续下发，已触发的动作自行完成。
取消和暂停的返回只说明请求响应，实际停止情况需查看机器人状态。程序不关闭 SLAM、不自动恢复运动。
同步 RPC 执行期间信号不能取消已送达的 WAV；会在调用返回或超时后停止后续流程。
音频超时不会自动重试，防止重复播放。
