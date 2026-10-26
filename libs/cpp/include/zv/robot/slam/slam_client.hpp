#pragma once

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <zv/common/auth_client.hpp>
#include <zv/common/auth_client_util.hpp>
#include <zv/robot/client/client.hpp>
#include <zv/robot/slam/slam_api.hpp>
#include <zv/third_party/nlohmann_json/json.hpp>

namespace zv::robot
{

struct NavPose
{
    struct Position
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    } position;

    struct Orientation
    {
        float q_x = 0.0f;
        float q_y = 0.0f;
        float q_z = 0.0f;
        float q_w = 0.0f;
    } orientation;

    struct Euler
    {
        float r = 0.0f;
        float p = 0.0f;
        float y = 0.0f;
    } euler;

    nlohmann::json to_json(bool use_euler = true) const
    {
        nlohmann::json j;
        j["use_euler"] = use_euler;
        j["position"] = {
            {"x", position.x},
            {"y", position.y},
            {"z", position.z}
        };
        if (use_euler) {
            j["euler_angle"] = {
                {"r", euler.r},
                {"p", euler.p},
                {"y", euler.y}
            };
        } else {
            j["orientation"] = {
                {"q_x", orientation.q_x},
                {"q_y", orientation.q_y},
                {"q_z", orientation.q_z},
                {"q_w", orientation.q_w}
            };
        }
        return j;
    }

    void from_json(const nlohmann::json & j)
    {
        if (j.contains("position")) {
            position.x = j["position"].value("x", 0.0f);
            position.y = j["position"].value("y", 0.0f);
            position.z = j["position"].value("z", 0.0f);
        }
        if (j.contains("orientation")) {
            orientation.q_x = j["orientation"].value("q_x", 0.0f);
            orientation.q_y = j["orientation"].value("q_y", 0.0f);
            orientation.q_z = j["orientation"].value("q_z", 0.0f);
            orientation.q_w = j["orientation"].value("q_w", 0.0f);
        }
        if (j.contains("euler_angle") || j.contains("euler")) {
            const auto & angles = j.contains("euler_angle") ? j["euler_angle"] : j["euler"];
            euler.r = angles.value("r", 0.0f);
            euler.p = angles.value("p", 0.0f);
            euler.y = angles.value("y", 0.0f);
        }
    }
};

inline void to_json(nlohmann::json & j, const NavPose & p)
{
    // 如果四元数未被赋值（全为0的非合法单位四元数），则自动推断使用欧拉角
    bool use_euler = (p.orientation.q_w == 0.0f && p.orientation.q_x == 0.0f && p.orientation.q_y == 0.0f &&
                      p.orientation.q_z == 0.0f);
    j = p.to_json(use_euler);
}

class SlamClient : public Client
{
public:
    SlamClient() : Client(SLAM_SERVICE_NAME) {}
    ~SlamClient() {}

    // 初始化通信频道
    void Init()
    {
        SetApiVersion(SLAM_API_VERSION);

        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_SLAM_START_MAPPING);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_SLAM_STOP_MAPPING);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_SLAM_GET_MAP);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_SLAM_INIT_POSE);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_SLAM_GET_POSE_STATE);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_SLAM_POSE_NAV);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_SLAM_PAUSE_NAV);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_SLAM_RESUME_NAV);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_SLAM_CANCEL_NAV);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_SLAM_CLOSE);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_SLAM_QUERY_MAP);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_SLAM_PREVIEW_MAP);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_SLAM_DELETE_MAP);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_SLAM_GET_NAV_STATE);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_SLAM_GET_STATE);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_SLAM_RESTART_ODOM);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_SLAM_TERMINATE_MAPPING);

        auth_client_.Init();
        auth_client_.SetTimeout(30.f);
    }

    // 开启建图
    int32_t StartMapping(const std::string & scene_id, const std::string & slam_type, std::string & data)
    {
        nlohmann::json params;
        params["scene_id"] = scene_id;
        params["slam_type"] = slam_type;
        return Call(ROBOT_API_ID_SLAM_START_MAPPING, params.dump(), data);
    }

    // 停止建图
    int32_t StopMapping(const std::string & scene_id, std::string & data)
    {
        nlohmann::json params;
        params["scene_id"] = scene_id;
        return Call(ROBOT_API_ID_SLAM_STOP_MAPPING, params.dump(), data);
    }

    // Stop mapping without saving files (1822).
    int32_t TerminateMapping(const std::string & scene_id, std::string & data)
    {
        return Call(ROBOT_API_ID_SLAM_TERMINATE_MAPPING,
                    nlohmann::json{
                        {"scene_id", scene_id}
        }
                        .dump(),
                    data);
    }

    // Restart odometry; the backend owns recovery and optional relocalization.
    int32_t RestartOdom(const std::string & scene_id, bool relocalize, std::string & data)
    {
        return Call(ROBOT_API_ID_SLAM_RESTART_ODOM,
                    nlohmann::json{
                        {"scene_id",   scene_id  },
                        {"relocalize", relocalize}
        }
                        .dump(),
                    data);
    }

    // 查询机器人上的地图文件列表（通过 SLAM DDS 接口）
    // 请求：{"type":"maps"}  响应：{"files":[{name,path,size}]}
    int32_t QueryMap(std::vector<FileInfo> & files)
    {
        files.clear();
        nlohmann::json params;
        params["type"] = "maps";
        std::string data;
        int32_t ret = Call(ROBOT_API_ID_SLAM_QUERY_MAP, params.dump(), data);
        if (ret != 0 && ret != 1) return ret;
        return parse_list_response(data, files);
    }

    // 预览地图
    int32_t PreviewMap(const std::string & map_id, std::string & map_data)
    {
        nlohmann::json params;
        params["map_id"] = map_id;
        return Call(ROBOT_API_ID_SLAM_PREVIEW_MAP, params.dump(), map_data);
    }

    // 删除地图
    int32_t DeleteMap(const std::string & map_id, std::string & data)
    {
        nlohmann::json params;
        params["map_id"] = map_id;
        return Call(ROBOT_API_ID_SLAM_DELETE_MAP, params.dump(), data);
    }

    // 下载指定地图文件到本地（DDS 通知 SLAM 后端 + DDS 鉴权 + HTTP 下载）
    // 返回 0 表示成功，负值表示致命错误，正值表示应用层失败。
    // 单次调用时自管理 token 生命周期；批量调用前由调用方预先 RequestAccess 可复用 session。
    int32_t GetMap(const FileInfo & file, const std::string & local_path, DownloadProgressFn progress = nullptr)
    {
        // 1803 validates scene/filename and returns the authenticated HTTP path.
        nlohmann::json params;
        params["map_name"] = file.name;
        std::string slam_resp;
        int32_t ret = Call(ROBOT_API_ID_SLAM_GET_MAP, params.dump(), slam_resp);
        if (ret != 0 && ret != 1) return ret;
        std::string remote_path;
        try {
            const auto info = nlohmann::json::parse(slam_resp).at("data");
            remote_path = info.at("path").get<std::string>();
            if (info.at("name").get<std::string>() != file.name || remote_path.rfind("/files/maps/", 0) != 0) return -1;
        } catch (const nlohmann::json::exception &) {
            return -1;
        }

        // Step 2: 若无有效 token 则申请（单次调用场景）
        bool acquired = false;
        if (!auth_client_.HasValidToken()) {
            FileAccessInfo access;
            ret = auth_client_.RequestFileAccess(access);
            if (ret != 0) return ret;
            acquired = true;
        }

        // Step 3: HTTP 下载（直接使用 auth_client_ 持有的 token/base_url）
        ret = auth_client_.DownloadFile(remote_path, local_path, progress);

        // Step 4: 本次申请的 token 用完即释放，避免 session 泄漏
        if (acquired) auth_client_.RevokeAccess();
        return ret;
    }

    // 下载机器人上的所有导航地图文件到指定本地目录。
    // 返回 0 表示全部成功，负值表示致命错误（如鉴权或列举失败），正值表示失败文件数。
    int32_t DownloadMaps(const std::string & local_dir, int * ok_count = nullptr, int * fail_count = nullptr)
    {
        // 预申请 session，所有 GetMap 调用共用同一 token（避免服务端 SESSION_CONFLICT）
        FileAccessInfo access;
        int32_t ret = auth_client_.RequestFileAccess(access);
        if (ret != 0) {
            std::cerr << "RequestFileAccess failed (code=" << ret
                      << "). Check that zv_api is running and auth service is configured." << std::endl;
            return ret;
        }
        std::cout << "Access granted. token_ttl=" << access.ttl_seconds << "s  base_url=" << access.base_url
                  << std::endl;

        std::vector<FileInfo> files;
        ret = QueryMap(files);
        if (ret != 0) {
            std::cerr << "QueryMap failed (code=" << ret << ")" << std::endl;
            auth_client_.RevokeAccess();
            return ret;
        }

        if (ok_count) *ok_count = 0;
        if (fail_count) *fail_count = 0;

        if (files.empty()) {
            auth_client_.RevokeAccess();
            return 0;
        }

        std::cout << "Found " << files.size() << " map file(s):" << std::endl;
        for (const auto & f : files) {
            std::cout << "  " << f.name << "  (" << std::fixed << std::setprecision(1) << f.size / 1024.0 << " KB)"
                      << std::endl;
        }

        // RAII 确保无论是否异常，session 都会释放
        struct SessionGuard
        {
            AuthClient & ac;
            ~SessionGuard() { ac.RevokeAccess(); }
        } guard{auth_client_};

        int ok = 0, fail = 0;
        for (const auto & f : files) {
            // QueryMap names include scene/filename. Preserve scene directories so
            // different scenes' map.yaml/map.pgm/map.pcd do not overwrite one
            // another.
            namespace fs = std::filesystem;
            const fs::path relative(f.name);
            bool safe = !relative.empty() && !relative.is_absolute();
            for (const auto & component : relative) {
                if (component == ".." || component == ".") safe = false;
            }
            if (!safe) {
                ++fail;
                continue;
            }
            std::error_code ec;
            const auto root = fs::weakly_canonical(local_dir, ec);
            if (ec) {
                ++fail;
                continue;
            }
            const auto destination = fs::weakly_canonical(root / relative, ec);
            const auto within = destination.lexically_relative(root);
            if (ec || within.empty() || *within.begin() == "..") {
                ++fail;
                continue;
            }
            fs::create_directories(destination.parent_path(), ec);
            if (ec) {
                ++fail;
                continue;
            }
            const std::string local_path = destination.string();
            std::string name = f.name;
            auto progress = [name](int64_t downloaded, int64_t total) {
                if (total <= 0) return;
                int pct = static_cast<int>(downloaded * 100 / total);
                std::cout << "\r  " << name << "  " << pct << "%  (" << downloaded / 1024 << "/" << total / 1024
                          << " KB)" << std::flush;
            };
            ret = GetMap(f, local_path, progress);
            if (ret == 0) {
                std::cout << "\r  " << f.name << "  OK  (" << f.size / 1024 << " KB)" << std::endl;
                ++ok;
            } else {
                std::cout << std::endl;
                std::cerr << "  FAILED (code=" << ret << ")" << std::endl;
                ++fail;
            }
        }

        if (ok_count) *ok_count = ok;
        if (fail_count) *fail_count = fail;
        return fail;
    }

    // 初始化位姿
    int32_t InitPose(const std::string & scene_id, bool auto_init, const NavPose & pose, std::string & data)
    {
        nlohmann::json params;
        params["scene_id"] = scene_id;
        params["auto_init_pose"] = auto_init;
        if (!auto_init) {
            params["manual_pose"] = pose;
        }
        return Call(ROBOT_API_ID_SLAM_INIT_POSE, params.dump(), data);
    }

    // 获取SLAM状态
    int32_t GetState(std::string & state)
    {
        state.clear();
        std::string data;
        int32_t ret = Call(ROBOT_API_ID_SLAM_GET_STATE, "", data);
        if (ret == 0 || ret == 1) state = ParseJsonData(data);
        return ret;
    }

    // 坐标点导航
    int32_t PoseNav(bool relative, const NavPose & target_pose, std::string & data)
    {
        nlohmann::json params;
        params["relative"] = relative;
        params["target_pose"] = target_pose;  // 依赖全局 to_json 进行自动推断
        return Call(ROBOT_API_ID_SLAM_POSE_NAV, params.dump(), data);
    }

    // 获取导航状态
    int32_t GetNavState(std::string & nav_state)
    {
        nav_state.clear();
        std::string data;
        int32_t ret = Call(ROBOT_API_ID_SLAM_GET_NAV_STATE, "", data);
        if (ret == 0 || ret == 1) nav_state = ParseJsonData(data);
        return ret;
    }

    // 获取位姿状态
    int32_t GetPoseState(std::string & pose_state)
    {
        pose_state.clear();
        std::string data;
        int32_t ret = Call(ROBOT_API_ID_SLAM_GET_POSE_STATE, "", data);
        if (ret == 0 || ret == 1) pose_state = ParseJsonData(data);
        return ret;
    }

    // 暂停导航
    int32_t PauseNav(std::string & data) { return Call(ROBOT_API_ID_SLAM_PAUSE_NAV, "", data); }

    // 恢复导航
    int32_t ResumeNav(std::string & data) { return Call(ROBOT_API_ID_SLAM_RESUME_NAV, "", data); }

    // 取消导航
    int32_t CancelNav(std::string & data) { return Call(ROBOT_API_ID_SLAM_CANCEL_NAV, "", data); }

    // 关闭SLAM服务
    int32_t Close(std::string & data) { return Call(ROBOT_API_ID_SLAM_CLOSE, "", data); }

private:
    AuthClient auth_client_;

    std::string ParseJsonData(const std::string & data) const
    {
        try {
            nlohmann::json json = nlohmann::json::parse(data);
            if (json.contains("data")) {
                if (json["data"].is_string()) {
                    return json["data"];
                } else {
                    return json["data"].dump();
                }
            }
        } catch (const nlohmann::json::exception &) {
            // 捕获 parse_error 以及 type_error (例如 json 并不是 object 类型)
        }
        return "";
    }
};

}  // namespace zv::robot
