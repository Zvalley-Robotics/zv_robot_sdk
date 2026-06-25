#include <chrono>
#include <ctime>
#include <iostream>
#include <thread>

#include <zv/robot/channel/channel_subscriber.hpp>
#include "NavAll_.hpp"

static const std::string NAV_ALL_TOPIC = "rt/nav_all";

using namespace zv::robot;
using namespace zv::common;

static int64_t g_count = 0;
static int64_t g_total_latency_ms = 0;

void Handler(const void* msg)
{
    const yesense_interface::msg::dds_::NavAll_* nav_msg =
        (const yesense_interface::msg::dds_::NavAll_*)msg;

    const auto& utc = nav_msg->utc_();
    uint16_t tid = nav_msg->tid_().tid_();


    if (false)
    {
        std::cout << "count: " << g_count << ", tid: " << tid;
        
        //若发布端GNSS UTC授时
        if (utc.year_() >= 2000)
        {
            auto now = std::chrono::system_clock::now();
            auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
            auto now_epoch = now_ms.time_since_epoch().count();

            struct tm send_tm = {};
            send_tm.tm_year = utc.year_() - 1900;
            send_tm.tm_mon = utc.month_() - 1;
            send_tm.tm_mday = utc.day_();
            send_tm.tm_hour = utc.hour_();
            send_tm.tm_min = utc.min_();
            send_tm.tm_sec = utc.sec_();
            time_t send_epoch = timegm(&send_tm) * 1000 + utc.ms_();

            int64_t latency_ms = now_epoch - send_epoch;

            g_count++;
            g_total_latency_ms += latency_ms;
            double avg_latency = static_cast<double>(g_total_latency_ms) / g_count;

            std::cout << ", latency: " << latency_ms << " ms, avg_latency: " << avg_latency << " ms" << std::endl;
        }
        else
        {
            std::cout << ", latency: N/A (UTC 未授时)" << std::endl;
        }
    }
    else
    {
        const auto& acc = nav_msg->acc_();
        const auto& gyro = nav_msg->gyro_();
        const auto& euler = nav_msg->euler_();
        const auto& quat = nav_msg->quat_();
        const auto& pos = nav_msg->pos_();
        const auto& vel = nav_msg->vel_();
        const auto& status = nav_msg->status_();
        const auto& temp = nav_msg->temp_();
        const auto& pressure = nav_msg->pressure_();

        std::cout << "===== IMU NavAll =====" << std::endl;
        std::cout << "  tid: " << tid << std::endl;
        std::cout << "  acc  [x, y, z]: " << acc.x_() << ", " << acc.y_() << ", " << acc.z_() << std::endl;
        std::cout << "  gyro [x, y, z]: " << gyro.x_() << ", " << gyro.y_() << ", " << gyro.z_() << std::endl;
        std::cout << "  euler [pitch, roll, yaw]: " << euler.pitch_() << ", " << euler.roll_() << ", " << euler.yaw_() << std::endl;
        std::cout << "  quat [q0, q1, q2, q3]: " << quat.q0_() << ", " << quat.q1_() << ", " << quat.q2_() << ", " << quat.q3_() << std::endl;
        std::cout << "  gnss [lon, lat, alt]: " << pos.longitude_() << ", " << pos.latitude_() << ", " << pos.altitude_() << std::endl;
        std::cout << "  vel  [e, n, u]: " << vel.vel_e_() << ", " << vel.vel_n_() << ", " << vel.vel_u_() << std::endl;
        std::cout << "  utc  [" << static_cast<int>(utc.year_()) << "-"
                  << static_cast<int>(utc.month_()) << "-"
                  << static_cast<int>(utc.day_()) << " "
                  << static_cast<int>(utc.hour_()) << ":"
                  << static_cast<int>(utc.min_()) << ":"
                  << static_cast<int>(utc.sec_()) << "."
                  << utc.ms_() << "]" << std::endl;
        std::cout << "  status [fusion, gnss]: " << static_cast<int>(status.fusion_status_())
                  << ", " << static_cast<int>(status.gnss_status_()) << std::endl;
        std::cout << "  temp: " << temp.temp_() << std::endl;
        std::cout << "  pressure: " << pressure.val_() << std::endl;
    }
}

int main()
{
    ChannelFactory::Instance()->Init(0);
    ChannelSubscriber<yesense_interface::msg::dds_::NavAll_> subscriber(NAV_ALL_TOPIC);
    subscriber.InitChannel(Handler);

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    return 0;
}
