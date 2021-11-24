#ifndef path_tracking_pid__CONTOUR_FOLLOWER_HPP_
#define path_tracking_pid__CONTOUR_FOLLOWER_HPP_

#include <limits>
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include "./controller.hpp"
#include "nav2_core/controller.hpp"
#include "rclcpp/rclcpp.hpp"
#include "pluginlib/class_loader.hpp"
#include "pluginlib/class_list_macros.hpp"
#include <nav_msgs/msg/odometry.hpp>
#include <nav_msgs/msg/path.hpp>
#include "nav2_util/odometry_utils.hpp"
#include "nav2_util/geometry_utils.hpp"
#include "visualization_msgs/msg/marker.hpp"
#include "visualization_msgs/msg/marker_array.hpp"
#include "nav2_util/node_utils.hpp"
#include "nav2_core/exceptions.hpp"
#include <std_msgs/msg/bool.h>
#include <std_msgs/msg/float64.h>

#include <tf2/exceptions.h>
#include <tf2_ros/transform_listener.h>
#include <tf2_ros/buffer.h>

//-------- new ------------ 0
#include <path_tracking_pid/msg/pid_debug.hpp>
#include <path_tracking_pid/msg/pid_feedback.hpp>

#include <atomic>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/register/point.hpp>
#include <boost/thread/recursive_mutex.hpp>

#define MAP_PARALLEL_THRESH 0.2
constexpr double DT_MAX=1.5;

BOOST_GEOMETRY_REGISTER_POINT_2D(geometry_msgs::msg::Point, double, cs::cartesian, x, y)

enum init_vel_method_enum {Zero, InternalState, Odom};
//-------- new ------------ 1

namespace path_tracking_pid
{

/**
 * @class path_tracking_pid::PathTrackingPid
 * @brief Path tracking pid plugin for nav2
 */
class PathTrackingPid : public nav2_core::Controller
{
//-------- new ------------ 0
private:
  typedef boost::geometry::model::ring<geometry_msgs::msg::Point> polygon_t;

  inline polygon_t union_(polygon_t polygon1, polygon_t polygon2)
  {
    std::vector<polygon_t> output_vec;
    boost::geometry::union_(polygon1, polygon2, output_vec);
    return output_vec.at(0);  // Only first vector element is filled
  }
//-------- new ------------ 1

public:
  /**
   * @brief Constructor for path_tracking_pid::PathTrackingPid
   */
  PathTrackingPid() = default;

  /**
   * @brief Destrructor for path_tracking_pid::PathTrackingPid
   */
  ~PathTrackingPid() override = default;

  /**
   * @brief Configure controller state machine
   * @param parent WeakPtr to node
   * @param name Name of plugin
   * @param tf TF buffer
   * @param costmap_ros Costmap2DROS object of environment
   */
  void configure(
    const rclcpp_lifecycle::LifecycleNode::WeakPtr & parent,
    std::string name, const std::shared_ptr<tf2_ros::Buffer> & tf,
    const std::shared_ptr<nav2_costmap_2d::Costmap2DROS> & costmap_ros) override;

  /**
   * @brief Cleanup controller state machine
   */
  void cleanup() override;

  /**
   * @brief Activate controller state machine
   */
  void activate() override;

  /**
   * @brief Deactivate controller state machine
   */
  void deactivate() override;

  /**
   * @brief Compute the best command given the current pose and velocity, with possible debug information
   *
   * Same as above computeVelocityCommands, but with debug results.
   * If the results pointer is not null, additional information about the twists
   * evaluated will be in results after the call.
   *
   * @param pose      Current robot pose
   * @param velocity  Current robot velocity
   * @param goal_checker   Ptr to the goal checker for this task in case useful in computing commands
   * @return          Best command
   */
  geometry_msgs::msg::TwistStamped computeVelocityCommands(
    const geometry_msgs::msg::PoseStamped & pose,
    const geometry_msgs::msg::Twist & velocity,
    nav2_core::GoalChecker * /*goal_checker*/) override;

  /**
   * @brief nav2_core setPlan - Sets the global plan
   * @param path The global plan
   */
  void setPlan(const nav_msgs::msg::Path & path) override;

  /**
   * @brief Limits the maximum linear speed of the robot.
   * @param speed_limit expressed in absolute value (in m/s)
   * or in percentage from maximum robot speed.
   * @param percentage Setting speed limit in percentage if true
   * or in absolute values in false case.
   */
  void setSpeedLimit(const double & speed_limit, const bool & percentage) override;

  // ----------------------- new -------------------- 0
  rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr  collision_marker_pub_;
  rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr       marker_pub_;
  rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr                   path_pub_;
  rclcpp::Publisher<path_tracking_pid::msg::PidFeedback>::SharedPtr        feedback_pub_;
  rclcpp::Publisher<path_tracking_pid::msg::PidDebug>::SharedPtr           debug_pub_;

  /**
   * @brief Convert a tf2 Vector3 type to its equivalent geometry_msgs representation.
   * This function is a specialization of the toMsg template defined in tf2/convert.h.
   * @param in A tf2 Vector3 object.
   * @return The Vector3 converted to a geometry_msgs message type.
   */
  inline
  void toMsg(const tf2::Vector3 & in, geometry_msgs::msg::Point & out)
  {
    out.x = in.getX();
    out.y = in.getY();
    out.z = in.getZ();
  }

  inline
  bool isZero(const rclcpp::Time & time)
  {
    if (time.nanoseconds() == 0.0){
      return true;
    } else{
      return false;
    }
  }
  // ----------------------- new -------------------- 1

private:
  // ----------------------- new -------------------- 0
  path_tracking_pid::Controller pid_controller_;
  std::string map_frame_;
  std::string base_link_frame_;

  nav2_util::LifecycleNode::SharedPtr node_;

  bool initialized_ = false;

  void curOdomCallback(const nav_msgs::msg::Odometry& odom_msg);

  // void velMaxExternalCallback(const std_msgs::Float64& msg);

  uint8_t projectedCollisionCost();

  // Cancel flags (multi threaded, so atomic bools)
  std::atomic<bool> active_goal_{false};
  std::atomic<bool> cancel_requested_{false};
  std::atomic<bool> cancel_in_progress_{false};

  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_odom_;

  nav_msgs::msg::Odometry latest_odom_;
  rclcpp::Time prev_time_;
  builtin_interfaces::msg::Duration prev_dt_;

  // Controller logic
  bool controller_debug_enabled_ = false;

  nav_msgs::msg::Path received_path_;

  geometry_msgs::msg::TransformStamped tfCurPoseStamped_;

  PidConfig config_;

  std::vector<geometry_msgs::msg::PoseStamped> global_plan_;

  // Used for tricycle model
  bool use_tricycle_model_;
  std::string steered_wheel_frame_;
  geometry_msgs::msg::TransformStamped tf_base_to_steered_wheel_stamped_;

  // ----------------------- new --------------------- 1

protected:
  // ----------------------- new --------------------- 0

  // ----------------------- new --------------------- 1


  std::shared_ptr<tf2_ros::Buffer> tf_;
  std::string plugin_name_;
  std::shared_ptr<nav2_costmap_2d::Costmap2DROS> costmap_ros_;
  nav2_costmap_2d::Costmap2D * costmap_;
  rclcpp::Logger logger_ {rclcpp::get_logger("PathTrackingPid")};
};

}  // namespace path_tracking_pid

#endif  // path_tracking_pid__CONTOUR_FOLLOWER_HPP_