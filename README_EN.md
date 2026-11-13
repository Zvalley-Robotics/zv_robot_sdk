# Application Development

[中文](README.md) | [English](README_EN.md)

## **SDK Overview**

Zvalley Robot SDK is a high-performance robot control software development kit based on DDS (Data Distribution Service). It provides standardized communication and control interfaces for robotic systems. The SDK uses a modern C++ architecture and supports ROS2-compatible data formats.

## **Getting the SDK**

### Introduction

Zvalley Robot SDK is a development kit from Zvalley for next-generation robots. It provides a unified interface for low-level motor control and high-level motion control. Standardized interfaces and example projects help developers extend robot functionality and build custom applications. With Zvalley Robot SDK, developers can efficiently build mobile platforms, control algorithms, and application systems.

### SDK Repository

https://github.com/Zvalley-Robotics/zv_robot_sdk.git

Follow the repository README to install the SDK on your development computer.

## Quick Start

### System Requirements

| Hardware | Recommended Configuration |
| :------- | :------------------------ |
| OS | Ubuntu 22.04 LTS |
| CPU | x86_64 / ARM64 |
| CPU Cores | 4 |
| RAM | 16 GB |

The SDK includes MCAP reading support. Install the LZ4 and Zstd runtime libraries to use data collection:

```bash
sudo apt-get install liblz4-1 libzstd1
```

### Using the SDK

To build your own application with the SDK, create an example program in the examples directory, compile it, and run the resulting executable to verify its functionality.

```
cd examples
mkdir build && cd build
cmake ..
make
```

After compilation, executables are generated in the `build` directory and can be run directly.

High-level service operations also require the corresponding services on the robot to be online.

# Services

Zvalley Robot SDK provides both **high-level services** and **low-level services**, using different communication patterns to meet their respective real-time requirements. High-level services control overall robot behavior, such as mode switching, omnidirectional movement, composite actions, and head control. These commands use RPC request/response calls, which suit one-time or low-frequency operations. Low-level services handle real-time data and low-level control, including high-frequency motor and IMU data acquisition and direct motor control. These interfaces use DDS publish/subscribe communication for continuous, high-speed data exchange.

High-level and low-level motion control interfaces are compatible with motion control version `v1.2.21`.

## High-Level Service Interfaces

### DDS Communication Interfaces

#### **Introduction to Cyclone DDS**

DDS is a distributed communication framework that implements the OMG DDS specification. Publish/subscribe communication allows nodes to exchange data directly without a central server, while QoS policies provide reliability and real-time behavior.

![](images/dds.png)

#### Interface Reference

`zv::robot` provides DDS communication for sending and receiving messages within a robotic system or between the robot and external applications. The main classes are `ChannelFactory`, `ChannelPublisher`, and `ChannelSubscriber`.

---

**zv:: robot::ChannelFactory**

ChannelFactory is a singleton used to create channels based on DDS topics. Call its initialization interface before use to configure the underlying DDS communication.

| Function | Instance |
|-----------|------|
| Prototype | static zv:: robot::ChannelFactory* zv:: robot::ChannelFactory::Instance() |
| Description | Returns a pointer to the singleton instance |
| Parameters | None |
| Returns | Pointer to the zv::robot::ChannelFactory singleton |
| Notes | None |

| Function | Release |
|-----------|------|
| Prototype | void Release() |
| Description | Releases ChannelFactory static resources |
| Parameters | None |
| Returns | None |
| Notes | zv::robot::ChannelFactory::Instance()->Release() |

| Function | Init |
|-----------|------|
| Prototype | void Init(const nlohmann::json &config) |
| Description | Initializes the factory and its internal DDS factory from the configuration |
| Parameters | config: JSON configuration containing domain_id (an integer from 0 to 232) and a nonempty dds_uri string; dds_uri contains the complete DDS XML configuration or a configuration file URI |
| Returns | None |
| Notes | When passing a JSON object directly, use an absolute path for file URIs; domain_id 0 selects domain 0 without reading ZV_DDS_DOMAIN_ID |

| Function | Init |
|-----------|------|
| Prototype | void Init(int32_t domainId, const std::string& networkInterface = "") |
| Description | Initializes ChannelFactory with a domain ID and network interface |
| Parameters | domainId: DDS communication domain ID; networkInterface: network interface name, empty by default |
| Returns | None |
| Notes | If networkInterface is empty, DDS selects the network interface automatically; when domainId is 0, the SDK reads ZV_DDS_DOMAIN_ID and uses domain 0 if it is unset |

| Function | Init |
|-----------|------|
| Prototype | void Init(const std::string& configPath) |
| Description | Reads a configuration file to initialize ChannelFactory |
| Parameters | configPath: path to a JSON configuration file; domain_id specifies the domain ID (0–232), and dds_uri contains the complete DDS XML configuration or a configuration file URI, such as file:///etc/cyclonedds.xml |
| Returns | None |
| Notes | The domain ID and network configuration come from the file; domain ID 0 in the file selects domain 0 without reading ZV_DDS_DOMAIN_ID; relative file:// references are resolved against the JSON file's directory; a missing or invalid configuration file causes an exception |

Configuration file example:

```json
{
  "domain_id": 10,
  "dds_uri": "file://./cyclonedds_localhost.xml"
}
```

Place `cyclonedds_localhost.xml` in the same directory as the JSON file.

The example configuration is in `examples/config/` and includes JSON and XML files. The bundled `dds.json` uses domain ID `10`.
The XML binds to the local `lo` interface and uses unicast for communication on the same host; modify the configuration for the actual network when communicating across hosts.

When initializing with a network interface, the domain ID is selected through the `ZV_DDS_DOMAIN_ID` environment variable, defaulting to domain 0 when it is unset. When using a configuration file, the domain ID comes from the file.

C++ initialization from a configuration file:

```cpp
// Read the domain ID and DDS configuration from JSON to initialize the process-wide channel factory
zv::robot::ChannelFactory::Instance()->Init(std::string("examples/config/dds.json"));
```

| Function | CreateSendChannel |
|-----------|------|
| Prototype | Channel pointer CreateSendChannel(const std::string& name) |
| Description | Creates a sending channel with the specified name and message type MSG |
| Parameters | name: channel name |
| Returns | Pointer to the templated channel |
| Notes | None |

| Function | CreateRecvChannel |
|-----------|------|
| Prototype | Channel pointer CreateRecvChannel(const std::string& name, std::function<void(const void*)> callback, int32_t queue_len = 0, bool reliable = true) |
| Description | Creates a receiving channel with the specified name and message type MSG |
| Parameters | name: channel name; callback: callback invoked when a message arrives; queue_len: sets the DDS KeepLast history depth when greater than 0, or retains the default history policy when 0; reliable: true for reliable delivery, false for best-effort delivery |
| Returns | Pointer to the templated channel |
| Notes | Callbacks run on the DDS receive thread and should avoid time-consuming work; queue_len only changes the DDS history depth and does not create a separate processing thread |

---

**zv:: robot::ChannelPublisher**

Publishes messages of a specified type.

| Class | Constructor and Destructor |
|-----------|------|
| template ChannelPublisher | explicit ChannelPublisher(const std::string& channelName); ~ChannelPublisher() |

| Function | InitChannel |
|-----------|------|
| Prototype | void InitChannel() |
| Description | Initializes the channel for publishing messages |
| Parameters | None |
| Returns | None |
| Notes | None |

| Function | CloseChannel |
|-----------|------|
| Prototype | void CloseChannel() |
| Description | Closes the channel |
| Parameters | None |
| Returns | None |
| Notes | None |

| Function | Write |
|-----------|------|
| Prototype | void Write(const MSG& msg,int64_t wait_microsec =0) |
| Description | Publishes a message of the specified type |
| Parameters | msg: message of type MSG to publish |
| Returns | None |
| Notes | None |

**DDS Publishing Example — Hello World**

Example: `examples/helloworld/publisher.cpp`

This example uses the SDK's ChannelPublisher API to publish a simple string message.

```cpp
#include <unistd.h>

#include <zv/robot/channel/channel_publisher.hpp>

#include "String_.hpp"

// Topic to publish to
static const std::string HELLO_WORLD_TOPIC = "rt/hello_world";

using namespace zv::robot;
using namespace zv::common;

int main()
{
    // Initialize the channel factory once per process
    ChannelFactory::Instance()->Init(0);

    // Create and initialize the publisher
    ChannelPublisher<std_msgs::msg::dds_::String_> publisher(HELLO_WORLD_TOPIC);
    publisher.InitChannel();
    int count = 0;
    while (true) {
        // Construct a message and write it to the channel
        std_msgs::msg::dds_::String_ msg("HelloWorld. " + std::to_string(count++));
        publisher.Write(msg);
        // Publish at 1 Hz
        sleep(1);
    }
    return 0;
}
```

---

**zv:: robot::ChannelSubscriber**

Subscribes to messages of a specified type.

| Class | Constructor and Destructor |
|-----------|------|
| template ChannelSubscriber | explicit ChannelSubscriber(const std::string& channelName); ~ChannelSubscriber() |

| Function | InitChannel |
|-----------|------|
| Prototype | void InitChannel(const std::function<void(const void*)>& handler, int64_t queue_len = 0, bool reliable = true) |
| Description | Initializes the channel to receive and process messages |
| Parameters | handler: callback invoked when a message arrives; queue_len: sets the DDS KeepLast history depth when greater than 0, or retains the default history policy when 0; reliable: true for reliable delivery, false for best-effort delivery |
| Returns | None |
| Notes | Callbacks run on the DDS receive thread and should avoid time-consuming work; queue_len only changes the DDS history depth and does not create a separate processing thread |

| Function | CloseChannel |
|-----------|------|
| Prototype | void CloseChannel() |
| Description | Closes the channel |
| Parameters | None |
| Returns | None |
| Notes | None |

| Function | GetLastDataAvailableTime |
|-----------|------|
| Prototype | int64_t GetLastDataAvailableTime() const |
| Description | Returns the time the most recent message was received |
| Parameters | None |
| Returns | -1 if the channel is uninitialized; otherwise, a monotonically increasing timestamp since system boot, in microseconds |
| Notes | None |

**DDS Subscription Example — Hello World**

Example: `examples/helloworld/subscriber.cpp`

This example uses the SDK's ChannelSubscriber API to subscribe to string messages and read their contents in a callback.

```cpp
#include <iostream>
#include <zv/robot/channel/channel_subscriber.hpp>

#include "String_.hpp"

// Topic to subscribe to
static const std::string HELLO_WORLD_TOPIC = "rt/chatter";

using namespace zv::robot;
using namespace zv::common;

// 1. The SDK invokes this callback when a message arrives on the topic
void Handler(const void * msg)
{
    // Cast the generic pointer to the specific String_ message type
    const std_msgs::msg::dds_::String_ * hello_world_msg = (const std_msgs::msg::dds_::String_ *)msg;

    // Print the received message
    std::cout << "message:" << hello_world_msg->data_() << std::endl;
}

int main()
{
    // 2. Initialize the channel factory to manage underlying SDK communication resources
    ChannelFactory::Instance()->Init(0);

    // 3. Create a subscriber for the topic and message type
    ChannelSubscriber<std_msgs::msg::dds_::String_> subscriber(HELLO_WORLD_TOPIC);

    // 4. Initialize the channel and register Handler to process incoming messages
    subscriber.InitChannel(Handler);

    // 5. Keep the process running; the subscriber invokes the callback on an internal thread
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    return 0;
}
```

---

<div style="page-break-after: always;"></div>

## High-Level Motion Control Interfaces

#### Overall Design

![State transition diagram](images/yk.svg)

The diagram shows only the main states and transition directions. Check `/robot/state` for the actual state.

#### Control Interface Reference

**1. /robot/input/intent Motion Control Interface**

Sends unified control intents to the robot, including state switching, action model switching, action pause/resume, and continuous motion control.

**Topic**

```
/robot/input/intent
```

**Message Type**

```RobotIntent.msg
std_msgs/Header header
string source
string intent_type
string command
string action
float32 linear_x_mps
float32 linear_y_mps
float32 angular_z_radps
uint8 priority
```

**Parameters**

| Parameter | Type | Description |
| --------- | ---- | ----------- |
| header | std_msgs/Header | Standard message header; stamp is used to determine whether the intent has expired |
| source | string | Intent source; use `sdk` |
| intent_type | string | Intent type: `status`, `motion`, `mode`, `model`, `config`, or `reset` |
| command | string | Specific command, such as `ready`, `walk`, `switch_model`, or `speed` |
| action | string | Command target or parameter; model name for model, `rl`/`rule` for mode, and a configuration value for config |
| linear_x_mps | float32 | Desired forward velocity (m/s) |
| linear_y_mps | float32 | Desired lateral velocity (m/s) |
| angular_z_radps | float32 | Desired yaw angular velocity (rad/s) |
| priority | uint8 | Intent priority; use `50` |

**intent_type and command Combinations**

| intent_type | command | action | Description |
| ----------- | ------- | ------ | ----------- |
| status | `ready`/`walk`/`damp`/`reinit` | — | Switch motion state |
| motion | `walk` | — | Send continuous motion velocities through `linear_x_mps`/`linear_y_mps`/`angular_z_radps` |
| model | `switch_model` | Model name | Switch action model |
| model | `motion_stop` | `stop` | Pause execution of the current action (model) |
| model | `motion_stop` | `resume` | Resume execution of the current action (model) |
| model | `finish_loop` | — | Request that a looping action finish its current cycle and end |
| config | `getup` | `stand` / `lie` / `face` | Set the preparation posture: get up from standing / a lying position / a prone position |
| config | `nav` | `manual` / `auto` / `follow` | Set the navigation mode |
| config | `speed` | `std` / `high` | Set the speed mode |
| config | `mode` | `rl` / `mix` / `spec` / `mocap` / `app` | Set the control mode |
| reset | `reset_fault` | — | Submit a fault reset intent |

**status Values**

| Value | Description |
| ----- | ----------- |
| ready | Ready state |
| walk | Walking control |
| damp | Damping state |
| reinit | Reinitialize |

**model Parameters**

Switches robot action models and pauses or resumes execution of the current action (model).

Available models:

| Model | Description |
| ----- | ----------- |
| z01_std_walk | Walking |
| z01_pa_walk | Walking |
| t4_walk | Walking |
| t4_sprint | Fast walking |
| t4_depth | Vision-guided walking |
| t4_heart | Heart gesture |
| t4_salute | Salute |
| t4_hello | Wave |
| t4_introduce | Self-introduction |
| t4_introduce_left | Self-introduction (left) |
| t4_introduce_right | Self-introduction (right) |
| t4_high_five | High five |
| t4_zl1 | Exhibition 1 |
| t4_zl2 | Exhibition 2 |
| t4_zl3 | Exhibition 3 |
| t4_zl4 | Exhibition 4 |
| t4_zl5 | Exhibition 5 |
| t4_zl6 | Exhibition 6 |
| t4_standby | Standby |
| t4_clap | Clap |
| t4_handshake | Handshake |
| t4_rhello | Right-hand wave |
| t4_rintroduce_left | Self-introduction (left, continuous) |
| t4_rintroduce_right | Self-introduction (right, continuous) |
| t4_yb1 | Welcome 1 |
| t4_yb2 | Welcome 2 |
| t4_yb2_left | Welcome 2 (left) |
| t4_yb3 | Welcome 3 |
| t4_yb3_left | Welcome 3 (left) |
| t4_yb4 | Welcome 4 |
| t4_taichi | Tai chi in motion |
| t4_taichi_w | Static tai chi |
| t4_mj_dance | Moonwalk dance |
| t4_xqxa123_dance | Xiang Qin Xiang Ai dance |
| t4_apt | APT dance |
| t4_rhythm | Rhythm dance |

**motion Parameters**

`motion` is a continuous motion command. A continuous publishing rate of 10 Hz is recommended. To stop, send an intent with all three velocities set to 0.

| Parameter | Type | Description |
| --------- | ---- | ----------- |
| linear_x_mps | float32 | Forward/backward linear velocity (m/s); positive values move forward |
| linear_y_mps | float32 | Lateral linear velocity (m/s); positive values move left |
| angular_z_radps | float32 | Angular velocity (rad/s); positive values turn left |

**2. /robot/state Status Interface**

Reports the robot's current system state, active mode, active model, and fault information.

**Topic**

```
/robot/state
```

**Message Type**

```RobotState.msg
std_msgs/Header header
uint8 state
string state_name
string active_mode
string active_control_source
string active_rl_model
bool fault_active
string fault_reason
```

**Parameters**

| Parameter | Type | Description |
| --------- | ---- | ----------- |
| header | std_msgs/Header | Standard message header; stamp is the time the status was generated |
| state | uint8 | Current system state, represented by a `STATE_*` constant |
| state_name | string | State name, such as `READY` or `RL_ACTIVE` |
| active_mode | string | Current active mode, such as `idle`, `rl`, `rule`, or `debug` |
| active_control_source | string | Source currently holding control, such as `teleop`, `api`, or `rl_manager` |
| active_rl_model | string | Name of the active RL model; may be empty outside RL mode |
| fault_active | bool | Whether the robot is currently in a fault state |
| fault_reason | string | Main current fault reason; may be empty when there is no fault |

**state Values**

| Value | Description |
| ----- | ----------- |
| STATE_BOOTING (0) | Booting |
| STATE_INIT (1) | Initializing |
| STATE_IDLE (2) | Idle |
| STATE_DO_READY (3) | Preparing to enter the ready state |
| STATE_READY (4) | Ready |
| STATE_RL_STANDBY (5) | RL standby |
| STATE_RL_ACTIVE (6) | RL active |
| STATE_RULE_STANDBY (7) | Rule-based control standby |
| STATE_RULE_ACTIVE (8) | Rule-based control active |
| STATE_TRANSITIONING (9) | Transitioning |
| STATE_FAULT (10) | Fault |
| STATE_DAMP (11) | Damping |
| STATE_SHUTDOWN (12) | Shutdown |

**3. RobotIntent Publishing Example**

Example: `examples/highcmd/intent_publisher.cpp`

This example uses the SDK's `ChannelPublisher` to publish a selected `RobotIntent_` intent for state switching, model switching, or motion control. The default intent is `status` / `ready`, published once, with a period of 50 ms. Each publication updates the timestamp; stopping continuous motion control sends an additional zero-velocity message.

The following excerpt contains the message construction and publishing logic from the source file. Argument parsing and the wait and shutdown helpers are omitted. `value` reads the selected fields, `options` holds the DDS connection settings, `count` is the number of publications (`0` means continuous publication), and `period` is the interval in milliseconds.

```cpp
// 1. Set the publication count and period; the default is one publication, count=0 publishes continuously, and the period is in milliseconds
const int count = example::ParseInteger(value("count", "1"), 0, 1000000);
const int period = example::ParseInteger(value("period_ms", "50"), 10, 60000);

// 2. Construct the selected high-level control intent by setting the message fields directly
robot_interfaces::msg::dds_::RobotIntent_ intent;
// Intent source and priority default to sdk and 50, respectively
intent.source_(value("source", "sdk"));
intent.priority_(example::ParseInteger(value("priority", "50"), 0, 255));
// Intent type, command, and target; defaults are status / ready, with an empty action
intent.intent_type_(value("intent_type", "status"));
intent.command_(value("command", "ready"));
intent.action_(value("action", ""));
// Forward/backward and lateral linear velocities are in m/s; yaw angular velocity is in rad/s
intent.linear_x_mps_(std::stof(value("linear_x_mps", "0")));
intent.linear_y_mps_(std::stof(value("linear_y_mps", "0")));
intent.angular_z_radps_(std::stof(value("angular_z_radps", "0")));

// 3. Initialize DDS using a network interface or configuration file, then create the RobotIntent publisher; interface mode uses the environment variable for the domain ID
example::InitDds(options.network_interface, options.use_config);
zv::robot::ChannelPublisher<robot_interfaces::msg::dds_::RobotIntent_> publisher("rt/robot/input/intent");
publisher.InitChannel();

// 4. Register stop signals; Stop sets running to 0 so the publishing loop exits
std::signal(SIGINT, Stop);
std::signal(SIGTERM, Stop);
Wait(500);  // Allow DDS discovery so a single message can also reach existing subscribers.

// 5. Update the timestamp before each publication, setting seconds and nanoseconds separately
const auto stamp = [&] {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now);
    intent.header_().stamp_().sec_(static_cast<int32_t>(seconds.count()));
    intent.header_().stamp_().nanosec_(
        static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - seconds).count()));
};

// 6. Publish the same kind of intent at the configured count and period; count=0 continues until a stop signal arrives
for (int sent = 0; running && (count == 0 || sent < count); ++sent) {
    stamp();
    publisher.Write(intent);
    std::cout << "Published rt/robot/input/intent count=" << sent + 1 << std::endl;
    if (count == 0 || sent + 1 < count) Wait(period);
}

// 7. Send one zero-velocity motion intent when continuous publication ends or a stop signal arrives
if ((count == 0 || !running) && intent.intent_type_() == "motion") {
    intent.linear_x_mps_(0);
    intent.linear_y_mps_(0);
    intent.angular_z_radps_(0);
    stamp();
    publisher.Write(intent);
    std::cout << "Stopped; sent zero velocity on rt/robot/input/intent" << std::endl;
}

// 8. Briefly allow messages to be sent before closing the publishing channel
std::this_thread::sleep_for(std::chrono::milliseconds(100));
publisher.CloseChannel();
```

**4. RobotState Subscription Example**

Example: `examples/highcmd/state_subscriber.cpp`

This example uses the SDK's `ChannelSubscriber` to subscribe to `RobotState_` messages and read the robot's current state, active mode, and model.

The example's default domain ID is 0. When 0 is passed, the SDK reads ZV_DDS_DOMAIN_ID and uses domain 0 if it is unset. When using a DDS configuration file, the domain ID comes from the file. The code below retains the core subscription and state-reading logic.

```cpp
#include <unistd.h>

#include <iostream>
#include <zv/robot/channel/channel_subscriber.hpp>

#include "RobotState_.hpp"

static const std::string STATE_TOPIC = "rt/robot/state";
// Bridge configuration: reliability=best_effort, history=keep_last, depth=5.
static constexpr int64_t STATE_QUEUE_LEN = 5;
static constexpr bool STATE_RELIABLE = false;

using namespace zv::robot;
using namespace zv::common;

// 1. The SDK invokes this callback when a RobotState_ message arrives on the topic
void Handler(const void * msg)
{
    const auto * state = static_cast<const robot_interfaces::msg::dds_::RobotState_ *>(msg);

    // 2. Read and print the state fields
    std::cout << "state: " << static_cast<int>(state->state_()) << ", state_name: " << state->state_name_()
              << ", active_mode: " << state->active_mode_() << ", active_rl_model: " << state->active_rl_model_()
              << std::endl;
}

int main()
{
    // 3. Initialize the channel factory
    ChannelFactory::Instance()->Init(0);

    // 4. Create a subscriber for the topic and message type
    ChannelSubscriber<robot_interfaces::msg::dds_::RobotState_> subscriber(STATE_TOPIC);

    // 5. Initialize the channel and register the callback with QoS matching robot_external_bridge
    subscriber.InitChannel(Handler, STATE_QUEUE_LEN, STATE_RELIABLE);

    // 6. Keep the process running; the subscriber receives messages and invokes Handler on an internal thread
    while (true) {
        sleep(10);
    }
    return 0;
}
```

**5. Battery Information Subscription Example**

ROS2 topic `/robot/battery/battery_info` corresponds to DDS topic `rt/robot/battery/battery_info`,
with message type `robot_interfaces::msg::dds_::BatteryInfo_`. Use the same domain ID and network interface as the battery publisher.
When using a DDS bridge, ensure that the topic is configured for bridging. The subscription only receives data and does not send motion commands.

```cpp
#include <unistd.h>
#include <iostream>
#include <zv/idl/t4/BatteryInfo_.hpp>
#include <zv/robot/channel/channel_subscriber.hpp>

int main()
{
    using namespace zv::robot;
    using BatteryInfo = robot_interfaces::msg::dds_::BatteryInfo_;
    // Initialize the same DDS domain and network interface as the robot.
    ChannelFactory::Instance()->Init(0);
    ChannelSubscriber<BatteryInfo> subscriber("rt/robot/battery/battery_info");
    subscriber.InitChannel([](const void * message) {
        const auto & battery = *static_cast<const BatteryInfo *>(message);
        // Raw total voltage, current, and SOC resolutions are 0.1 V, 0.1 A, and 0.1%, respectively.
        std::cout << "SOC: " << battery.soc_() / 10.0 << " %, voltage: "
                  << battery.total_volt_() / 10.0 << " V, current: "
                  << battery.current_() / 10.0 << " A" << std::endl;
    }, 5, false);
    while (true) sleep(1);
}
```

Python provides the corresponding `BatteryInfo_` and `ChannelSubscriber_BatteryInfo_` types.

| Field | Units and Meaning |
|---|---|
| `total_volt_`, `current_`, `limit_current_` | Divide raw values by 10 to obtain V, A, and A, respectively; preserve the current's sign |
| `soc_`, `soh_` | Divide raw values by 10 to obtain remaining charge and state of health (%) |
| `life_`, `power_`, `energy_` | Lifetime (%), power (W), and total energy (Wh) |
| `mos_temp_`, `board_temp_`, `heat_temp_`, `max_temp_`, `min_temp_` | Degrees Celsius; the current SDK stores these as uint8, so negative temperatures must be interpreted as raw int8 bytes |
| `max_volt_`, `min_volt_`, `diff_volt_` | Highest/lowest cell voltage and voltage difference (mV) |
| `remain_capacity_`, `cycle_count_`, `rest_charge_time_` | Remaining capacity (mAh), cycle count, and remaining charging time (min) |
| `bat_state_` | 0: idle, 1: charging, 2: discharging; preserve other state codes as received |
| `chg_mos_`, `dis_mos_`, `chg_detect_`, `load_detect_` | Charge/discharge MOS and charger/load connection states; 0: off/disconnected, 1: on/connected |

The raw message fields provide the remaining cell/temperature indices, precharge/heating/fan MOS states, inputs and outputs, wake source, current limiting, PWM, and other information.

## Data Collection Interface

`zv::robot::DataCollectionClient` starts and stops data recording on the robot.
Initialize DDS communication before use and ensure that data collection and the corresponding RPC services on the robot are online.

| Interface | Description |
| --------- | ----------- |
| `StartRecording()` | Starts recording |
| `StopRecording(RecordingInfo& recording)` | Stops recording and returns recording file information and the message count |

An interface return value of `0` indicates success; any value other than `0` indicates failure. Use `GetLastErrorMessage()` for error details.
`StopRecording()` only stops recording and returns file information; it does not download the file automatically.

**Start/Stop Recording Example**

```cpp
#include <iostream>
#include <zv/robot/channel/channel_factory.hpp>
#include <zv/robot/data_collection/data_collection_client.hpp>

int main()
{
    // 1. Initialize communication and the data collection client; set the domain ID and interface for your environment
    zv::robot::ChannelFactory::Instance()->Init(0);
    zv::robot::DataCollectionClient client;
    client.Init();
    client.SetTimeout(30.0f);

    // 2. Start recording
    int32_t result = client.StartRecording();
    if (result != 0) {
        std::cerr << "Failed to start recording, code=" << result << ": " << client.GetLastErrorMessage() << '\n';
        return 1;
    }
    std::cout << "Recording started. Press Enter to stop." << std::endl;
    std::cin.get();

    // 3. Stop recording and retrieve file information from the robot
    zv::robot::RecordingInfo recording;
    result = client.StopRecording(recording);
    if (result != 0) {
        std::cerr << "Failed to stop recording, code=" << result << ": " << client.GetLastErrorMessage() << '\n';
        return 1;
    }
    std::cout << "File name: " << recording.file.name << "\nRemote path: " << recording.file.path
              << "\nFile size (bytes): " << recording.file.size << "\nMessage count: " << recording.message_count << '\n';
    return 0;
}
```

For recording file downloads and MCAP reading and parsing, see
[examples/data_collection/data_collection.cpp](examples/data_collection/data_collection.cpp).
`DownloadRecording()` demonstrates downloading a file, while `ReadMcapFile()` demonstrates reading messages one by one
and using `McapMessage::DecodeJson()` to preview application fields.
`McapReader::OpenRange()` supports filtering by a log timestamp range (nanoseconds, start inclusive and end exclusive) and topics;
`McapMessage::DecodeSchemaJson()` parses CDR1 using embedded ROS2 message definitions without classifying business data by topic name.
`DownloadRecording()` and `StopAndDownload()` download both the MCAP file and `metadata.yaml` from the same directory on the robot.
Metadata is saved in the same local directory as the MCAP file. Use a separate directory for each recording; success is returned only when both files have been downloaded successfully.

## SLAM Navigation Interfaces

`zv::robot::SlamClient` provides mapping, localization, navigation, and map management interfaces.
Initialize `ChannelFactory` before use and ensure that SLAM and the corresponding RPC services on the robot are online.
Example: `examples/slam/slam.cpp`.

#### Mapping, Localization, and Navigation

| Interface | Business Information |
| ---- | ---- |
| `StartMapping(scene_id, slam_type, data)` | Starts mapping; available only in `IDLE`. `scene_id` is the scene ID, and `slam_type` is the mapping type |
| `StopMapping(scene_id, data)` | Stops mapping and saves the map |
| `TerminateMapping(scene_id, data)` | Terminates mapping without saving the map |
| `InitPose(scene_id, auto_init, pose, data)` | Loads a scene and initializes localization; `auto_init=true` uses automatic localization, while `false` uses `pose` for manual initialization |
| `PoseNav(relative, target_pose, data)` | Navigates to the target pose; `relative` selects a relative or absolute target |
| `PauseNav(data)` / `ResumeNav(data)` | Pauses or resumes navigation |
| `CancelNav(data)` | Cancels the current navigation goal |
| `Close(data)` | Shuts down SLAM |

In `NavPose`, `position.x/y/z` are in meters, and `euler.r/p/y` represent roll, pitch, and yaw in radians.
Alternatively, set the quaternion `orientation.q_x/q_y/q_z/q_w`. When all quaternion components are `0`, the SDK uses Euler angles;
otherwise, it uses the quaternion. For a unit quaternion representing no rotation, set `q_x=q_y=q_z=0` and `q_w=1`.

A return value of `0` for an asynchronous operation only means that the request has been accepted; it does not mean that mapping, localization, or navigation has completed.
Check current progress through the state query interfaces. An interface response does not replace the final result of an asynchronous task.
For SLAM control and state queries, a return value of `1` means that the current response reports success, while `2` indicates an application-level failure;
communication errors return SDK RPC error codes. Map listing and download interfaces perform their own result parsing; refer to each interface's return-value description.

#### State and Current Scene

![Navigation State Transition Diagram](images/nav.svg)

The diagram shows the main navigation states and transition directions. Use `GetState` queries for the complete state information.

| Query Interface | Business Information |
| --- | --- |
| `GetState(std::string& state)` | Queries the current SLAM state |
| `GetState(std::string& state, std::string& scene_id)` | Calls API 1815 once to return both the state and scene ID; the scene ID is empty before initialization, identifies the mapping scene in `MAPPING_*`, and identifies the currently active navigation map in `NAV_*` |
| `GetPoseState(std::string& pose_state)` | Returns a pose JSON string containing `global_pose` position, quaternion, Euler angles, and the `pose_error` localization error |
| `GetNavState(std::string& nav_state)` | Returns a navigation progress JSON string containing the absolute target pose `goal_pose`, elapsed navigation time `navigation_time` in seconds, and remaining distance `distance_remaining` in meters; available only in `NAV_RUNNING` |

`GetState` returns `state` as a state string. Parse the outputs of `GetPoseState` and `GetNavState` as JSON before reading their business fields.

| State | Description |
| ---- | ---- |
| `IDLE` | Idle, with neither mapping nor localization active |
| `MAPPING_STARTING` / `MAPPING_RUNNING` | Starting mapping / Mapping in progress |
| `MAPPING_STOPPING` / `MAPPING_TERMINATING` | Stopping and saving mapping / Terminating mapping |
| `NAV_STARTING` / `NAV_READY` | Starting localization and navigation / Localized and waiting for a navigation goal |
| `NAV_RUNNING` | Navigating |
| `NAV_PAUSING` / `NAV_PAUSED` | Pausing navigation / Navigation paused |
| `NAV_STOPPING` | Stopping navigation |

**State and Scene Query Example**

```cpp
#include <iostream>
#include <string>
#include <zv/robot/channel/channel_factory.hpp>
#include <zv/robot/slam/slam_client.hpp>

int main()
{
    // 1. Initialize communication and the SLAM client; read the domain ID from the environment, defaulting to 0 if unset
    zv::robot::ChannelFactory::Instance()->Init(0);
    zv::robot::SlamClient client;
    client.Init();
    client.SetTimeout(10.0f);

    // 2. Obtain the state and scene ID in one API 1815 request, without separate queries
    std::string state, scene_id;
    const int32_t result = client.GetState(state, scene_id);
    if (result != 0 && result != 1) {
        std::cerr << "Failed to query SLAM state, code=" << result << '\n';
        return 1;
    }
    std::cout << "State: " << state << ", scene: " << scene_id << '\n';

    // 3. Only a scene ID in a NAV_* state identifies the currently active navigation map
    if (state.rfind("NAV_", 0) == 0 && !scene_id.empty())
        std::cout << "Currently active map: " << scene_id << '\n';
    return 0;
}
```

#### Configurable Guided Tour

The guided tour example `examples/tour/tour.cpp` combines navigation, audio playback, and motion control actions into a sequence of waypoint explanations.
Its configuration file is `examples/config/tour.json`. The scene ID, waypoint order and coordinates, WAV paths, and explanation actions can all be modified.

| Configuration | Business Information |
| --- | --- |
| `scene_id` | Tour scene ID; the bundled configuration uses `2112734270341910528` |
| `waypoints` | Waypoints visited in array order; `enabled=false` skips a waypoint |
| `coordinateX` / `coordinateY` / `coordinateA` | Absolute map coordinates X and Y (meters) and yaw (radians), with Z set to 0 |
| `wav_file` | Absolute WAV path on the robot's audio-player host, played synchronously through `AudioClient::PlayWavFile` |
| `actions` | Action sequence submitted in array order during speech playback; each step can use a different model, and an empty array means audio only |
| `actions[].model` | The step's `t4_zl*` one-shot action model; the same model may appear in multiple steps |
| `actions[].delay_s` | Delay before submission, defaulting to 0 seconds and required to be nonnegative; the first step is relative to the start of the audio call, and later steps are relative to the start of the previous action request |
| `action_model` / `action_once` | Compatibility with the old single-model configuration; do not use together with `actions`. A `t4_zl*` model becomes one immediately submitted step; other looping actions follow `action_once` and wait for a return to the walking model |
| `wait_after_s` | Additional dwell time in seconds after the explanation sequence ends |
| `navigation_timeout_s` | Timeout for waiting for the final result of the current navigation request, defaulting to 300 seconds |

Available actions include `t4_zl2` for interactive explanation (a thoughtful gesture emphasizing logical reasoning), `t4_zl1` for an introduction or explanation without a display object,
`t4_zl5` for introducing something behind and to the right, `t4_zl4` for introducing something directly behind (right hand first, then left), and `t4_zl6` for introducing something behind and to the left.
Actual support is determined by the model list returned by the robot. Each waypoint uses `actions` to arrange its explanation actions.
The WAV path comes from each waypoint's `wav_file` and is not derived from the waypoint name. Deploy the corresponding audio files to the player host in advance.

The robot's motion control must meet the navigation prerequisites before starting the tour. When SLAM is idle, the program can load the specified scene and initialize localization automatically.
Reusing existing localization requires the same scene and a navigation-ready state. Navigation result `0` means accepted, so the program continues waiting for the final result of the same request;
`1` means success and starts the explanation; `2` means failure, causing the raw backend response to be printed and the tour to end.
The navigation backend determines arrival, position, heading, and localization error. The example does not perform its own checks or infer arrival from the navigation-ready state.
WAV playback succeeds only when the RPC returns `0` and the playback result has `success=true`. A `t4_zl*` model is a one-shot action.
During speech playback, the program submits actions through `SetModel` in the configured order and at the configured intervals, without waiting for the model name in the state to match.
It does not send commands to end or pause these actions or switch back to the walking model. When speech ends, no further steps are submitted; the robot completes actions that have already been triggered.
Action intervals should be at least as long as the preceding action, with enough time for complete playback. For example, submit `t4_zl1` first, then submit `t4_zl2` at least 14.76 seconds later:

```json
{
    "actions": [
        {"model": "t4_zl1", "delay_s": 0},
        {"model": "t4_zl2", "delay_s": 14.76}
    ]
}
```

Other looping actions follow `action_once`, use `FinishLoop` to end the current loop, and wait for a return to the walking model.
The tour example uses a generic `ChannelSubscriber<Response_>` to listen on `rt/rpc/slam/response`,
matching the current navigation request ID to await the final result forwarded by `zv_api`, without an additional subscription to the native `/slam/status` topic.
Navigation failure, communication errors, or a timeout end the tour; the program does not automatically resend requests, resume navigation, or wait for manual confirmation.
On an error or interruption, it attempts to cancel its own navigation and pause its own looping explanation action if it is still playing. For one-shot actions, it stops submitting further steps,
while actions already triggered complete on their own. A synchronous audio request already sent continues waiting for completion or timeout. The program does not automatically shut down SLAM.

#### Map Management

| Interface | Business Information |
| ---- | ---- |
| `QueryMap(files)` | Returns a list of map files; `FileInfo` contains `name`, `path`, and `size` in bytes |
| `PreviewMap(map_id, map_data)` | Queries map preview information by scene ID |
| `DeleteMap(map_id, data)` | Deletes a map by scene ID |
| `GetMap(file, local_path, progress)` | Downloads one map file; `file.name` uses `scene_id/filename`, `local_path` is the local file path, and the download progress callback is optional |
| `DownloadMaps(local_dir, ok_count, fail_count)` | Downloads map files in bulk to a local directory and can return the numbers of successful and failed downloads; returns `0` if all succeed, a positive value for the number of failed files, or a negative value for a fatal error |

To identify the currently active map, use the `NAV_*` state and corresponding `scene_id` returned by `GetState(state, scene_id)`.
`IDLE` means that neither mapping nor localization is active. A mapping scene must not be treated as the currently active navigation map.
Before deleting a map, the application should query the current state again. If the selected scene is being used for localization and navigation, do not delete that map.
`DeleteMap` directly submits the deletion request and does not include interactive confirmation.

#### Map, Pose, and Collision Risk Subscriptions

`zv::robot::SlamSubscriber` uses an independent DDS communication instance and does not require `ChannelFactory` initialization.
Its domain ID and network configuration should match the robot-side publishers; when used with RPC, use the same configuration for both.

| Initialization and Connection Interface | Description |
| ---- | ---- |
| `Init(int32_t domain_id = 0, const std::string& network_interface = "", bool transient_map = false)` | Initializes subscriptions; domain ID 0 reads `ZV_DDS_DOMAIN_ID`, defaulting to 0 if unset; an empty network interface name enables automatic selection |
| `Init(const std::string& config_path, bool transient_map)` | Reads the domain ID and DDS configuration from a JSON file; `transient_map` must be passed explicitly |
| `GetMapPublisherCount()` / `GetPosePublisherCount()` / `GetCollisionRiskPublisherCount()` | Queries the number of currently matched publishers for each topic |
| `Close()` | Closes subscriptions; initialization can be performed again after closing |

| Subscription Interface | ROS2 Topic / DDS Topic | Message Type and Business Information |
| --- | --- | --- |
| `TakeMap(SlamMap& output)` | `/map` / `rt/map` | `nav_msgs/msg/OccupancyGrid`; `SlamMap` contains `header`, `info`, and `data`. `info` includes dimensions, resolution, origin, and other information; in `data`, `-1` indicates an unknown cell, and `0–100` represents occupancy probability |
| `TakePose(geometry_msgs::msg::dds_::PoseStamped_& output)` | `/guide_tour/global_pose` / `rt/guide_tour/global_pose` | `geometry_msgs/msg/PoseStamped`; global pose with a timestamp and coordinate frame |
| `TakeCollisionRisk(float& output)` | `/slam/collision_risk` / `rt/slam/collision_risk` | `std_msgs/msg/Float32`; ranges from `0–1`, with higher values indicating greater risk, at approximately 1 Hz |

All of these read interfaces are nonblocking and consume the latest valid sample; they return `false` and reset the output when no new valid sample is available.
Non-finite or out-of-range collision risk values are invalid samples and cause `false` to be returned.
Do not treat missing new messages, no matched publishers, or stale data as zero risk. The application should record reception times and distinguish a valid zero value from unavailable data.

By default, subscriptions receive live maps, poses, and collision risk using best-effort reliability, volatile durability, and a history depth of one latest sample.
When `transient_map=true`, only the map subscription switches to reliable, transient-local mode to receive retained maps from publishers supporting that mode;
pose and collision risk subscriptions continue to receive live data.

**Collision Risk Subscription Example**

```cpp
#include <chrono>
#include <iostream>
#include <thread>
#include <zv/robot/slam/slam_subscriber.hpp>

int main()
{
    // 1. Initialize an independent subscription; the domain ID and network settings must match the robot
    zv::robot::SlamSubscriber subscriber;
    subscriber.Init(0, "", false);
    // To use a configuration file, replace the previous line with:
    // subscriber.Init(std::string("examples/config/dds.json"), false);

    while (true) {
        // 2. Update the risk only when a new valid sample is received; 0.0 is valid data
        float risk = 0.0f;
        if (subscriber.TakeCollisionRisk(risk))
            std::cout << "Collision risk: " << risk << '\n';
        // 3. Without a new sample, do not use the reset output to infer safety; check data freshness separately when displaying it
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
```

The SDK does not provide SLAM state event subscriptions. Query the current state, pose, and navigation progress through the RPC interfaces above;
query results do not contain a complete history of asynchronous results or automatic recovery events.

## Operations Diagnostics Interface

`zv::robot::DiagnosticsSubscriber` subscribes to the robot's real-time aggregated diagnostic information.
The message is compatible with ROS2 `diagnostic_msgs/msg/DiagnosticArray`.
It uses an independent DDS communication instance and does not require `ChannelFactory` initialization. The domain ID and network interface must match the diagnostic publisher.

| Item | Description |
| ---- | ---- |
| ROS2 Topic | `/diagnostics_agg` |
| DDS Topic | `rt/diagnostics_agg` |
| Message Type | `diagnostic_msgs::msg::dds_::DiagnosticArray_` |
| Header | `<zv/robot/ops/diagnostics_subscriber.hpp>` |

| Interface | Description |
| ---- | ---- |
| `Init(int32_t domain_id = 0, const std::string& network_interface = "")` | Initializes the subscription; domain ID 0 reads `ZV_DDS_DOMAIN_ID`, defaulting to 0 if unset; an empty network interface name enables automatic selection |
| `Init(const std::string& config_path)` | Reads the domain ID and DDS configuration URI from a parameter file, using the same file format as the DDS communication interfaces |
| `Take(DiagnosticArray_& output)` | Reads the latest frame without blocking; returns `true` when a valid message is received, or `false` and resets the output if there is no new message |
| `GetPublisherCount()` | Returns the number of currently matched diagnostic publishers |
| `Close()` | Closes the subscription; can be called repeatedly, and initialization can be performed again after closing |

In `DiagnosticArray_`, `header_()` is the message header and `status_()` is the array of aggregated diagnostic items.
Each message is a complete snapshot, so its new array should replace the previous one. An empty array means that the frame contains no diagnostic items;
the absence of a new message must not be interpreted as fault recovery. The subscription uses best-effort reliability, volatile durability, and a history depth of one latest frame,
and can receive live diagnostic messages from reliable or best-effort publishers.

| DiagnosticStatus_ Field | Type | Description |
| ---- | ---- | ---- |
| `level_()` | `uint8_t` | 0: OK, 1: WARN, 2: ERROR, 3: STALE |
| `name_()` | `std::string` | Diagnostic item name |
| `message_()` | `std::string` | Status or fault description |
| `hardware_id_()` | `std::string` | Hardware identifier |
| `values_()` | `std::vector<KeyValue_>` | Additional information; use `key_()` and `value_()` to read each item's key and value |

**Real-Time Diagnostics Subscription Example**

```cpp
#include <chrono>
#include <iostream>
#include <thread>
#include <zv/robot/ops/diagnostics_subscriber.hpp>

int main()
{
    // 1. Initialize an independent subscription; read the domain ID from the environment and select the network interface automatically
    zv::robot::DiagnosticsSubscriber subscriber;
    subscriber.Init(0);
    // To use a configuration file, replace the previous line with:
    // subscriber.Init(std::string("examples/config/dds.json"));

    while (true) {
        // 2. Read the latest aggregated snapshot, including OK, WARN, ERROR, and STALE items
        diagnostic_msgs::msg::dds_::DiagnosticArray_ message;
        if (subscriber.Take(message)) {
            for (const auto& status : message.status_()) {
                std::cout << "Level: " << static_cast<int>(status.level_())
                          << ", name: " << status.name_()
                          << ", description: " << status.message_()
                          << ", hardware: " << status.hardware_id_() << '\n';
                // 3. Print the diagnostic item's additional key/value information
                for (const auto& value : status.values_())
                    std::cout << "  " << value.key_() << ": " << value.value_() << '\n';
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
```

## Low-Level Service Interfaces

Low-level communication exchanges data between the user's PC and the robot using DDS.

#### Interface Reference

Subscribe to or publish topics using the methods described in the DDS communication section. Topic data is stored in structures defined by IDL. Common structures include:

| Structure | Description |
| --------- | ----------- |
| JointState_ | Feedback for a single logical joint, including position, velocity, effort, motor temperature, error codes, and error descriptions |
| JointStates_ | Feedback frame for multiple joints, including a sequence number, correlation ID of the most recent command, and a joint state list |
| HeadControl_ | Head yaw and pitch angle commands, in degrees |
| Twist_ | Chassis velocity command, including linear velocity (m/s) and angular velocity (rad/s) |

**Low-Level DDS Command and State Feedback Examples**

Low-level DDS command example: examples/lowcmd/publisher.cpp

This example uses the SDK to publish head control (HeadControl) and chassis velocity (Twist) DDS messages to rt/robot/external/head_control and rt/cmd_vel, respectively. Head control messages contain target yaw and pitch angles and a relative/absolute angle flag. Chassis messages contain linear and angular velocities.

The example supports either HeadControl or Twist, published once or periodically with a default period of 50 ms. Head angles are in degrees; chassis linear velocity is in m/s and angular velocity is in rad/s. Relative head angles are published only once; stopping continuous chassis velocity publication sends one additional zero-velocity message.

The code below retains the example's message construction and publication logic, using `publish_head` to select head or chassis messages.
`count` is the number of messages to send, defaulting to `1`; set it to `0` for continuous publication. `period_ms` is the publication interval in milliseconds.

```cpp
#include <algorithm>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
#include <zv/idl/ros2/Twist_.hpp>
#include <zv/idl/t4/HeadControl_.hpp>
#include <zv/robot/channel/channel_publisher.hpp>

static volatile std::sig_atomic_t keep_running = 1;
static void HandleSignal(int) { keep_running = 0; }

// Wait in short intervals so a stop signal can promptly end the publication loop
static void Wait(int period_ms)
{
    for (int elapsed = 0; keep_running && elapsed < period_ms; elapsed += 10)
        std::this_thread::sleep_for(std::chrono::milliseconds(std::min(10, period_ms - elapsed)));
}

using namespace zv::robot;

int main()
{
    // 1. Select the message type, count, and period; send one head command by default
    const bool publish_head = true;
    const int count = 1;
    const int period_ms = 50;

    // 2. Construct the head message; yaw and pitch are in degrees, and relative angles require count=1
    robot_interfaces::msg::dds_::HeadControl_ head;
    head.yaw_(0.0f);
    head.pitch_(0.0f);
    head.is_relative_(false);
    if (publish_head && head.is_relative_() && count != 1) return 1;

    // 3. Construct the chassis message; linear velocity is in m/s, angular velocity is in rad/s, and other components remain zero
    geometry_msgs::msg::dds_::Twist_ velocity;
    velocity.linear_().x_(0.0);
    velocity.linear_().y_(0.0);
    velocity.angular_().z_(0.0);

    // 4. Initialize communication; ChannelFactory can also be initialized from a JSON configuration file
    ChannelFactory::Instance()->Init(0);
    // To use a configuration file, replace the previous line with:
    // ChannelFactory::Instance()->Init(std::string("examples/config/dds.json"));
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    if (publish_head) {
        // 5. Create only the selected publisher, wait for DDS discovery, and send the specified number of head commands
        ChannelPublisher<robot_interfaces::msg::dds_::HeadControl_> publisher("rt/robot/external/head_control");
        publisher.InitChannel();
        Wait(500);
        for (int sent = 0; keep_running && (count == 0 || sent < count); ++sent) {
            publisher.Write(head);
            if (count == 0 || sent + 1 < count) Wait(period_ms);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        publisher.CloseChannel();
    } else {
        // 6. When chassis messages are selected, use the same publication count and period
        ChannelPublisher<geometry_msgs::msg::dds_::Twist_> publisher("rt/cmd_vel");
        publisher.InitChannel();
        Wait(500);
        for (int sent = 0; keep_running && (count == 0 || sent < count); ++sent) {
            publisher.Write(velocity);
            if (count == 0 || sent + 1 < count) Wait(period_ms);
        }
        // 7. Send one additional zero-velocity message when continuous publication ends or a stop signal is received
        if (count == 0 || !keep_running) publisher.Write(geometry_msgs::msg::dds_::Twist_{});
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        publisher.CloseChannel();
    }
    return 0;
}
```

Low-level DDS state feedback example: examples/lowcmd/subscriber.cpp

This example uses the SDK to subscribe to robot joint state (JointStates) DDS messages on rt/robot/driver/joint_states, corresponding to /robot/driver/joint_states in robot_external_bridge. The callback reads joint names, position, velocity, effort, and error information.

```cpp
#include <chrono>
#include <iostream>
#include <thread>
#include <zv/robot/channel/channel_subscriber.hpp>

#include "JointStates_.hpp"

// Topic for all robot joint states, corresponding to /robot/driver/joint_states
static const std::string LOW_STATE_TOPIC = "rt/robot/driver/joint_states";
// Bridge configuration: reliability=best_effort, history=keep_last, depth=5.
static constexpr int64_t LOW_STATE_QUEUE_LEN = 5;
static constexpr bool LOW_STATE_RELIABLE = false;

using namespace zv::robot;
using namespace zv::common;

// 1. The SDK invokes this callback when a JointStates_ message arrives on the topic
void Handler(const void * msg)
{
    // Cast the generic pointer to the specific JointStates_ message type
    const auto * joint_state_msg = static_cast<const robot_interfaces::msg::dds_::JointStates_ *>(msg);

    // Read the state frame sequence number and its correlation with the most recent command
    const uint32_t sequence = joint_state_msg->sequence_();
    const uint64_t last_command_trace_id = joint_state_msg->last_command_trace_id_();
    const uint32_t last_command_sequence = joint_state_msg->last_command_sequence_();
    const auto & joint_states = joint_state_msg->states_();

    // Print the state frame sequence number and command correlation information
    std::cout << "[Subscriber] Message received sequence: " << sequence
              << ", last command trace id: " << last_command_trace_id
              << ", last command sequence: " << last_command_sequence << std::endl;

    // Print all joint states; cast uint8_t to int to avoid printing it as a character
    for (size_t i = 0; i < joint_states.size(); i++) {
        const auto & js = joint_states[i];
        std::cout << "    joint " << i << " name: " << js.joint_name_() << ", position: " << js.position_()
                  << ", velocity: " << js.velocity_() << ", effort: " << js.effort_()
                  << ", motor_error: " << static_cast<int>(js.motor_error_())
                  << ", joint_error: " << static_cast<int>(js.joint_error_())
                  << ", error_message: " << js.error_message_() << std::endl;
    }
}

int main()
{
    // 2. Initialize the channel factory to manage underlying communication resources
    ChannelFactory::Instance()->Init(0);

    // 3. Create a subscriber for the topic and message type
    ChannelSubscriber<robot_interfaces::msg::dds_::JointStates_> subscriber(LOW_STATE_TOPIC);

    // 4. Initialize the channel and register the callback with QoS matching robot_external_bridge
    subscriber.InitChannel(Handler, LOW_STATE_QUEUE_LEN, LOW_STATE_RELIABLE);

    // 5. Keep the process running; the subscriber receives messages and invokes Handler on an internal thread
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    return 0;
}
```

**IMU / Navigation Data Subscription Example**

Example: examples/sensor/imu_nav_subscriber.cpp

This example uses the SDK to subscribe to IMU and integrated navigation data (NavAll_) on rt/robot/imu/nav_all, corresponding to /robot/imu/nav_all in robot_external_bridge. The callback reads acceleration, angular velocity, Euler angles, quaternion, GNSS longitude/latitude/altitude, east/north/up velocity, UTC time, fusion/GNSS status, temperature, and pressure.

```cpp
#include <chrono>
#include <iostream>
#include <thread>
#include <zv/robot/channel/channel_subscriber.hpp>

#include "NavAll_.hpp"

// Topic for IMU / integrated navigation data
static const std::string NAV_ALL_TOPIC = "rt/robot/imu/nav_all";

using namespace zv::robot;
using namespace zv::common;

// 1. The SDK invokes this callback when a NavAll_ message arrives on the topic
void Handler(const void * msg)
{
    // Cast the generic pointer to the specific NavAll_ message type
    const yesense_interface::msg::dds_::NavAll_ * nav_msg = (const yesense_interface::msg::dds_::NavAll_ *)msg;

    // Get references to the component structures
    const auto & acc = nav_msg->acc_();
    const auto & gyro = nav_msg->gyro_();
    const auto & euler = nav_msg->euler_();
    const auto & quat = nav_msg->quat_();
    const auto & pos = nav_msg->pos_();
    const auto & vel = nav_msg->vel_();
    const auto & status = nav_msg->status_();
    const auto & temp = nav_msg->temp_();
    const auto & pressure = nav_msg->pressure_();
    const auto & utc = nav_msg->utc_();
    uint16_t tid = nav_msg->tid_().tid_();

    // Cast uint8_t to int to avoid printing it as a character
    std::cout << "===== IMU NavAll =====" << std::endl;
    std::cout << "  tid: " << tid << std::endl;
    std::cout << "  acc  [x, y, z]: " << acc.x_() << ", " << acc.y_() << ", " << acc.z_() << std::endl;
    std::cout << "  gyro [x, y, z]: " << gyro.x_() << ", " << gyro.y_() << ", " << gyro.z_() << std::endl;
    std::cout << "  euler [pitch, roll, yaw]: " << euler.pitch_() << ", " << euler.roll_() << ", " << euler.yaw_()
              << std::endl;
    std::cout << "  quat [q0, q1, q2, q3]: " << quat.q0_() << ", " << quat.q1_() << ", " << quat.q2_() << ", "
              << quat.q3_() << std::endl;
    std::cout << "  gnss [lon, lat, alt]: " << pos.longitude_() << ", " << pos.latitude_() << ", " << pos.altitude_()
              << std::endl;
    std::cout << "  vel  [e, n, u]: " << vel.vel_e_() << ", " << vel.vel_n_() << ", " << vel.vel_u_() << std::endl;
    std::cout << "  utc  [" << static_cast<int>(utc.year_()) << "-" << static_cast<int>(utc.month_()) << "-"
              << static_cast<int>(utc.day_()) << " " << static_cast<int>(utc.hour_()) << ":"
              << static_cast<int>(utc.min_()) << ":" << static_cast<int>(utc.sec_()) << "." << utc.ms_() << "]"
              << std::endl;
    std::cout << "  status [fusion, gnss]: " << static_cast<int>(status.fusion_status_()) << ", "
              << static_cast<int>(status.gnss_status_()) << std::endl;
    std::cout << "  temp: " << temp.temp_() << std::endl;
    std::cout << "  pressure: " << pressure.val_() << std::endl;
}

int main()
{
    // 2. Initialize the channel factory to manage underlying communication resources
    ChannelFactory::Instance()->Init(0);

    // 3. Create a subscriber for the topic and message type
    ChannelSubscriber<yesense_interface::msg::dds_::NavAll_> subscriber(NAV_ALL_TOPIC);

    // 4. Initialize the channel and register the callback
    subscriber.InitChannel(Handler, 10, true);

    // 5. Keep the process running; the subscriber receives messages and invokes Handler on an internal thread
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    return 0;
}
```

> In `examples/CMakeLists.txt`, this example has the `imuNavSubscriber` build target, which produces the `imuNavSubscriber` executable.

**DDS Camera Preview Example**

Example: examples/sensor/camera_subscriber.cpp

`CameraSubscriber` subscribes to compressed images from the head camera and retrieves the latest frame without blocking through `TakeFrame()`.
Before running it, ensure that the camera driver is publishing images and set the topic, network interface, and domain ID for your environment.

```cpp
#include <unistd.h>

#include <iostream>
#include <map>
#include <zv/robot/media/camera_subscriber.hpp>

int main()
{
    // 1. Map camera names to complete DDS topic names
    const std::map<std::string, std::string> topics{
        {"head", "rt/media/camera/head/color/image_raw/compressed"}
    };

    // 2. Create and initialize CameraSubscriber with the camera names and topics
    zv::robot::CameraSubscriber subscriber;
    subscriber.Init(0, "", topics);
    // To use a configuration file, replace the previous line with:
    // subscriber.Init("examples/config/dds.json", topics);

    // 3. Continuously poll the latest compressed image from each camera
    zv::robot::CameraFrame frame;
    while (true) {
        for (const auto & [name, topic] : topics) {
            // TakeFrame returns without blocking and indicates whether a new frame is available for this camera.
            if (subscriber.TakeFrame(name, frame)) {
                // 4. Print the camera name, frame ID, compression format, and data length
                std::cout << name << ": " << frame.frame_id << ", " << frame.format << ", " << frame.data.size()
                          << " bytes" << std::endl;
            }
        }
        // 5. Poll every 100 ms to avoid consuming CPU in an empty polling loop
        usleep(100000);
    }

    return 0;
}
```

#### Message Types

The following message types are in the `robot_interfaces::msg::dds_` namespace. Field names retain the trailing `_` generated by DDS.

**Single Joint State (JointState_)**

| Field | Type | Description |
| ----- | ---- | ----------- |
| **joint_name_** | std::string | Joint name |
| **position_** | float | Current position (rad or m) |
| **velocity_** | float | Current velocity (rad/s or m/s) |
| **effort_** | float | Current torque/force (Nm or N) |
| **motor_temp_** | float | Motor temperature (degrees Celsius); 0 if unavailable |
| **mos_temp_** | float | MOS temperature (degrees Celsius) |
| **motor_error_** | uint8_t | Low-level motor drive fault code read directly from Encos/EtherCAT |
| **enabled_** | bool | Whether the joint is currently enabled |
| **joint_error_** | uint8_t | Joint error code, represented by a `JOINT_ERR_*` constant |
| **error_message_** | std::string | Human-readable error description; may be empty when there is no error |

`joint_error_` values (`JointState_Constants`):

| Constant | Value | Meaning |
| -------- | ----- | ------- |
| **JOINT_ERR_NONE_** | 0 | Joint operating normally |
| **JOINT_ERR_MOTOR_WARN_** | 1 | Low-level motor or drive warning |
| **JOINT_ERR_INVALID_ANGLE_STATE_** | 2 | Invalid feedback angle/position |
| **JOINT_ERR_INVALID_ANGLE_CMD_** | 3 | Invalid target angle/position command |
| **JOINT_ERR_ZERO_ABSENCE_** | 4 | Missing zero reference or abnormal zero-reference state |

**Multiple Joint State Frame (JointStates_)**

| Field | Type | Description |
| ----- | ---- | ----------- |
| **header_** | std_msgs::msg::dds_::Header_ | Standard message header; stamp is the time the feedback was generated |
| **sequence_** | uint32_t | State frame sequence number for detecting lost, out-of-order, or duplicate frames |
| **last_command_trace_id_** | uint64_t | trace_id of the most recently received/applied command |
| **last_command_sequence_** | uint32_t | Frame sequence number of the most recently received/applied command |
| **states_** | std::vector<JointState_> | Joint state list; the order in the robot configuration is recommended |

**Unified High-Level Control Intent (RobotIntent_)**

| Field | Type | Description |
| ----- | ---- | ----------- |
| **header_** | std_msgs::msg::dds_::Header_ | Standard message header; stamp is used to determine whether the intent has expired |
| **source_** | std::string | Intent source, such as `teleop`, `http`, `mqtt`, `sdk`, `cloud`, or `test` |
| **intent_type_** | std::string | Intent type: `status`, `motion`, `mode`, `model`, `config`, or `reset` |
| **command_** | std::string | Specific command, such as `ready`, `walk`, `switch_mode`, `switch_model`, or `speed` |
| **action_** | std::string | Command target or parameter; `rl`/`rule` for mode, model name for model, and a configuration value for config |
| **linear_x_mps_** | float | Desired forward velocity (m/s) |
| **linear_y_mps_** | float | Desired lateral velocity (m/s) |
| **angular_z_radps_** | float | Desired yaw angular velocity (rad/s) |
| **priority_** | uint8_t | Intent priority |

**Robot System State (RobotState_)**

| Field | Type | Description |
| ----- | ---- | ----------- |
| **header_** | std_msgs::msg::dds_::Header_ | Standard message header; stamp is the time the status was generated |
| **state_** | uint8_t | Current system state, represented by a `STATE_*` constant |
| **state_name_** | std::string | State name for logs and external API display |
| **active_mode_** | std::string | Current active mode, such as `idle`, `rl`, `rule`, or `debug` |
| **active_control_source_** | std::string | Source currently holding control, such as `teleop`, `api`, or `rl_manager` |
| **active_rl_model_** | std::string | Name of the active RL model; may be empty outside RL mode |
| **fault_active_** | bool | Whether the robot is currently in a fault state |
| **fault_reason_** | std::string | Main current fault reason; may be empty when there is no fault |

`state_` values (`RobotState_Constants`): `STATE_BOOTING_=0`, `STATE_INIT_=1`, `STATE_IDLE_=2`, `STATE_DO_READY_=3`, `STATE_READY_=4`, `STATE_RL_STANDBY_=5`, `STATE_RL_ACTIVE_=6`, `STATE_RULE_STANDBY_=7`, `STATE_RULE_ACTIVE_=8`, `STATE_TRANSITIONING_=9`, `STATE_FAULT_=10`, `STATE_DAMP_=11`, `STATE_SHUTDOWN_=12`.

**Head Control Command (HeadControl_)**

| Field | Type | Description |
| ----- | ---- | ----------- |
| **yaw_** | float | Target yaw angle (degrees, -180 to 180) |
| **pitch_** | float | Target pitch angle (degrees, -180 to 180) |
| **is_relative_** | bool | true = relative angle; false = absolute angle |

**Version Information (VersionInfo_)**

| Field | Type | Description |
| ----- | ---- | ----------- |
| **version_** | std::string | Version number, such as `zl_robot-v1.1.11` |
| **release_date_** | std::string | Release date, such as `2025-07-17` |

**Other Status Messages**

| Structure | Key Fields |
| --------- | ---------- |
| **BatteryInfo_** | Total voltage, current, SOC, lifetime, temperature, voltage/temperature statistics, MOS state, battery state, and current-limit/protection information |
| **DriverStatus_** | Drive state (`STATUS_*`), online status, fault state, total/active/faulty joint counts, and faulty joint names |
| **SystemStatus_** | CPU/memory/disk utilization and temperature, battery voltage/current/SOC, volume, mute status, and threshold warning flags |
| **RlManagerStatus_** | Node online status, model loading/readiness, inference activity/errors, active model, model type, inference frequency, and most recent inference duration |
| **SpeakStatus_** | Playback state (`STATE_IDLE/PLAYING/PAUSED`), current text, and queue length |
| **AudioRequest_** | Request type `kind_` (prompt text/file/temporary TTS), content `name_`, priority `priority_`, and interruption flag `interrupt_` |
| **FollowControl_** | Follow control command `command_` (enter/exit tracking mode) |
| **RlModelInfo_** | Model name `name_`, alias `alias_`, category `category_`, backend `backend_`, duration `duration_sec_`, and duration-valid flag `has_duration_` |

Python provides the corresponding `AudioRequest_`, `FollowControl_`, and `RlModelInfo_` types,
as well as the `ChannelPublisher_AudioRequest_` and `ChannelPublisher_FollowControl_` publishers.
Constants are in the `AudioRequest_Constants` submodule (`KIND_TIP_`, `KIND_FILE_`, `KIND_TEXT_`) and
the `FollowControl_Constants` submodule (`ENTER_TRACK_MODE_`, `EXIT_TRACK_MODE_`).

#### IMU / Navigation Message Types

The following message types are in the `yesense_interface::msg::dds_` namespace and carry IMU and integrated navigation data on `rt/robot/imu/nav_all`.

**Integrated Navigation Data (NavAll_)**

| Field | Type | Description |
| ----- | ---- | ----------- |
| **tid_** | Tid_ | Frame/thread identifier |
| **acc_** | ThreeAxis_ | Three-axis acceleration |
| **gyro_** | ThreeAxis_ | Three-axis angular velocity |
| **euler_** | EulerAngle_ | Euler angles |
| **quat_** | Quat_ | Quaternion |
| **temp_** | SensorTemp_ | Sensor temperature |
| **pos_** | GnssPos_ | GNSS position |
| **status_** | NavStatus_ | Navigation status |
| **vel_** | Vel_ | Velocity (east, north, up) |
| **utc_** | Utc_ | UTC time |
| **pressure_** | Pressure_ | Atmospheric pressure |

**Component Structure Fields**

| Structure | Fields (Types) |
| --------- | -------------- |
| Tid_ | tid_(uint16_t) |
| ThreeAxis_ | x_/y_/z_(float) |
| EulerAngle_ | pitch_/roll_/yaw_(float) |
| Quat_ | q0_/q1_/q2_/q3_(double) |
| GnssPos_ | longitude_(double), latitude_(double), altitude_(float) |
| Vel_ | vel_e_/vel_n_/vel_u_(float) |
| NavStatus_ | fusion_status_(uint8_t), gnss_status_(uint8_t) |
| SensorTemp_ | temp_(float) |
| Utc_ | year_(uint16_t), month_/day_/hour_/min_/sec_(uint8_t), ms_(uint32_t) |
| Pressure_ | val_(float) |
