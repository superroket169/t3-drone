// Task 1: two figure-eights around two poles, then back to the start point and land.

#include <chrono>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "geometry_msgs/msg/pose_stamped.hpp"
#include "mavros_msgs/msg/state.hpp"
#include "mavros_msgs/srv/command_bool.hpp"
#include "mavros_msgs/srv/command_tol.hpp"
#include "mavros_msgs/srv/set_mode.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/nav_sat_fix.hpp"

using namespace std::chrono_literals;
using geometry_msgs::msg::PoseStamped;
using mavros_msgs::msg::State;
using mavros_msgs::srv::CommandBool;
using mavros_msgs::srv::CommandTOL;
using mavros_msgs::srv::SetMode;
using sensor_msgs::msg::NavSatFix;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEarthRadius = 6371000.0;  // meters

// Local ENU position in meters: x east, y north, z up.
struct Point {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

enum class CoordFrame { Local, Gps };

enum class Phase { WaitStart, Arm, Takeoff, Climb, FollowRoute, Land, Done };

const char * phase_name(Phase phase) {
    switch (phase) {
        case Phase::WaitStart: return "WaitStart";
        case Phase::Arm: return "Arm";
        case Phase::Takeoff: return "Takeoff";
        case Phase::Climb: return "Climb";
        case Phase::FollowRoute: return "FollowRoute";
        case Phase::Land: return "Land";
        case Phase::Done: return "Done";
    }
    return "?";
}

Point to_point(const PoseStamped & pose) {
    return {pose.pose.position.x, pose.pose.position.y, pose.pose.position.z};
}

double horizontal_distance(const Point & a, const Point & b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

double distance(const Point & a, const Point & b) {
    return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z);
}

// East/north offset of (lat, lon) from (lat0, lon0), flat-earth approximation.
Point gps_offset(double lat, double lon, double lat0, double lon0) {
    const double to_rad = kPi / 180.0;
    Point p;
    p.x = (lon - lon0) * to_rad * kEarthRadius * std::cos(lat0 * to_rad);
    p.y = (lat - lat0) * to_rad * kEarthRadius;
    return p;
}

// Closed figure-eight made of two tangent circles, one around each pole.
// Circle a is flown counterclockwise, circle b clockwise. Both start at the
// midpoint between the poles, so the midpoint is at index 0 and index n.
std::vector<Point> figure_eight(const Point & a, const Point & b, int n, double z) {
    const double radius = horizontal_distance(a, b) / 2.0;
    const double heading = std::atan2(b.y - a.y, b.x - a.x);  // from a toward b
    std::vector<Point> loop;
    loop.reserve(2 * n);
    for (int i = 0; i < n; ++i) {
        const double angle = heading + 2.0 * kPi * i / n;
        loop.push_back({a.x + radius * std::cos(angle), a.y + radius * std::sin(angle), z});
    }
    for (int i = 0; i < n; ++i) {
        const double angle = heading + kPi - 2.0 * kPi * i / n;
        loop.push_back({b.x + radius * std::cos(angle), b.y + radius * std::sin(angle), z});
    }
    return loop;
}

// From the loop point nearest to start: `laps` times around the loop, back to
// that entry point, then above the start point.
std::vector<Point> build_route(const std::vector<Point> & loop, const Point & start, int laps, double z) {
    size_t entry = 0;
    for (size_t i = 1; i < loop.size(); ++i) {
        if (horizontal_distance(loop[i], start) < horizontal_distance(loop[entry], start)) {
            entry = i;
        }
    }
    std::vector<Point> route;
    for (int lap = 0; lap < laps; ++lap) {
        for (size_t j = 0; j < loop.size(); ++j) {
            route.push_back(loop[(entry + j) % loop.size()]);
        }
    }
    route.push_back(loop[entry]);
    route.push_back({start.x, start.y, z});
    return route;
}

}  // namespace

class Task1 : public rclcpp::Node {
public:
    Task1() : Node("task1") {
        read_parameters();

        auto qos = rclcpp::SensorDataQoS();
        state_sub_ = create_subscription<State>(
            "mavros/state", qos, [this](const State & msg) { state_ = msg; });
        pose_sub_ = create_subscription<PoseStamped>(
            "mavros/local_position/pose", qos, [this](const PoseStamped & msg) {
                pose_ = msg;
                have_pose_ = true;
            });
        fix_sub_ = create_subscription<NavSatFix>(
            "mavros/global_position/global", qos, [this](const NavSatFix & msg) {
                fix_ = msg;
                have_fix_ = msg.status.status >= sensor_msgs::msg::NavSatStatus::STATUS_FIX;
            });

        setpoint_pub_ = create_publisher<PoseStamped>("mavros/setpoint_position/local", 10);

        mode_client_ = create_client<SetMode>("mavros/set_mode");
        arm_client_ = create_client<CommandBool>("mavros/cmd/arming");
        takeoff_client_ = create_client<CommandTOL>("mavros/cmd/takeoff");

        timer_ = create_wall_timer(100ms, [this]() { step(); });
        RCLCPP_INFO(get_logger(), "Waiting for GUIDED mode to start");
    }

private:
    void read_parameters() {
        const auto frame = declare_parameter<std::string>("coord_frame", "local");
        if (frame == "local") {
            coord_frame_ = CoordFrame::Local;
        } else if (frame == "gps") {
            coord_frame_ = CoordFrame::Gps;
        } else {
            throw std::invalid_argument("coord_frame must be \"local\" or \"gps\", got: " + frame);
        }

        pole1_ = declare_parameter<std::vector<double>>("pole1", std::vector<double>{});
        pole2_ = declare_parameter<std::vector<double>>("pole2", std::vector<double>{});
        if (pole1_.size() != 2 || pole2_.size() != 2) {
            throw std::invalid_argument("pole1 and pole2 must each have 2 values");
        }

        altitude_ = declare_parameter<double>("altitude", 5.0);
        points_per_circle_ = declare_parameter<int>("points_per_circle", 16);
        reach_tolerance_ = declare_parameter<double>("reach_tolerance", 1.0);
        laps_ = declare_parameter<int>("laps", 2);
        if (points_per_circle_ < 4) {
            throw std::invalid_argument("points_per_circle must be at least 4");
        }
    }

    void step() {
        if (busy_) {
            return;  // a service reply is pending
        }
        if (needs_guided(phase_) && state_.mode != "GUIDED") {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
                "Paused: mode is %s, not GUIDED", state_.mode.c_str());
            return;
        }

        switch (phase_) {
            case Phase::WaitStart:
                if (ready_to_start()) {
                    start_mission();
                    set_phase(Phase::Arm);
                }
                break;
            case Phase::Arm:
                if (state_.armed) {
                    set_phase(Phase::Takeoff);
                } else {
                    request_arm();
                }
                break;
            case Phase::Takeoff:
                request_takeoff();
                break;
            case Phase::Climb:
                if (to_point(pose_).z > route_.front().z - reach_tolerance_) {
                    set_phase(Phase::FollowRoute);
                }
                break;
            case Phase::FollowRoute:
                follow_route();
                break;
            case Phase::Land:
                if (state_.mode == "LAND") {
                    set_phase(Phase::Done);
                } else {
                    request_mode("LAND");
                }
                break;
            case Phase::Done:
                break;
        }
    }

    static bool needs_guided(Phase phase) {
        return phase == Phase::Arm || phase == Phase::Takeoff ||
               phase == Phase::Climb || phase == Phase::FollowRoute;
    }

    bool ready_to_start() const {
        const bool position_ready = have_pose_ && (coord_frame_ == CoordFrame::Local || have_fix_);
        return state_.connected && state_.mode == "GUIDED" && position_ready;
    }

    void start_mission() {
        start_ = to_point(pose_);
        start_fix_ = fix_;
        const double z = start_.z + altitude_;
        const auto [a, b] = poles_local();
        route_ = build_route(figure_eight(a, b, points_per_circle_, z), start_, laps_, z);
        next_ = 0;
        RCLCPP_INFO(get_logger(), "Route: %zu waypoints, poles (%.1f, %.1f) and (%.1f, %.1f)",
            route_.size(), a.x, a.y, b.x, b.y);
    }

    // Pole positions in the local frame.
    std::pair<Point, Point> poles_local() const {
        auto convert = [this](const std::vector<double> & pole) {
            Point offset{pole[0], pole[1], 0.0};
            if (coord_frame_ == CoordFrame::Gps) {
                offset = gps_offset(pole[0], pole[1], start_fix_.latitude, start_fix_.longitude);
            }
            return Point{start_.x + offset.x, start_.y + offset.y, start_.z};
        };
        return {convert(pole1_), convert(pole2_)};
    }

    void follow_route() {
        const Point & target = route_[next_];
        send_setpoint(target);
        if (distance(to_point(pose_), target) > reach_tolerance_) {
            return;
        }
        ++next_;
        if (next_ == route_.size()) {
            set_phase(Phase::Land);
        }
    }

    void send_setpoint(const Point & target) {
        PoseStamped sp;
        sp.header.stamp = now();
        sp.header.frame_id = "map";
        sp.pose.position.x = target.x;
        sp.pose.position.y = target.y;
        sp.pose.position.z = target.z;
        sp.pose.orientation.w = 1.0;
        setpoint_pub_->publish(sp);
    }

    void set_phase(Phase phase) {
        RCLCPP_INFO(get_logger(), "%s -> %s", phase_name(phase_), phase_name(phase));
        phase_ = phase;
    }

    void request_mode(const std::string & mode) {
        if (!mode_client_->service_is_ready()) {
            return;
        }
        auto req = std::make_shared<SetMode::Request>();
        req->custom_mode = mode;
        busy_ = true;
        mode_client_->async_send_request(
            req, [this](rclcpp::Client<SetMode>::SharedFuture) { busy_ = false; });
    }

    void request_arm() {
        if (!arm_client_->service_is_ready()) {
            return;
        }
        // TODO: back off between retries when pre-arm checks keep failing
        auto req = std::make_shared<CommandBool::Request>();
        req->value = true;
        busy_ = true;
        arm_client_->async_send_request(
            req, [this](rclcpp::Client<CommandBool>::SharedFuture) { busy_ = false; });
    }

    void request_takeoff() {
        if (!takeoff_client_->service_is_ready()) {
            return;
        }
        auto req = std::make_shared<CommandTOL::Request>();
        req->altitude = altitude_;
        busy_ = true;
        takeoff_client_->async_send_request(
            req, [this](rclcpp::Client<CommandTOL>::SharedFuture f) {
                if (f.get()->success) {
                    set_phase(Phase::Climb);
                }
                busy_ = false;
            });
    }

    // Parameters
    CoordFrame coord_frame_ = CoordFrame::Local;
    std::vector<double> pole1_;
    std::vector<double> pole2_;
    double altitude_ = 0.0;
    int points_per_circle_ = 0;
    double reach_tolerance_ = 0.0;
    int laps_ = 0;

    // ROS interfaces
    rclcpp::Subscription<State>::SharedPtr state_sub_;
    rclcpp::Subscription<PoseStamped>::SharedPtr pose_sub_;
    rclcpp::Subscription<NavSatFix>::SharedPtr fix_sub_;
    rclcpp::Publisher<PoseStamped>::SharedPtr setpoint_pub_;
    rclcpp::Client<SetMode>::SharedPtr mode_client_;
    rclcpp::Client<CommandBool>::SharedPtr arm_client_;
    rclcpp::Client<CommandTOL>::SharedPtr takeoff_client_;
    rclcpp::TimerBase::SharedPtr timer_;

    // Latest data from MAVROS
    State state_;
    PoseStamped pose_;
    NavSatFix fix_;
    bool have_pose_ = false;
    bool have_fix_ = false;

    // Mission state
    Phase phase_ = Phase::WaitStart;
    bool busy_ = false;
    Point start_;
    NavSatFix start_fix_;
    std::vector<Point> route_;
    size_t next_ = 0;
};

int main(int argc, char ** argv) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<Task1>());
    rclcpp::shutdown();
    return 0;
}
