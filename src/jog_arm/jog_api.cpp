///////////////////////////////////////////////////////////////////////////////
//      Title     : motion_api.cpp
//      Project   : jog_arm
//      Created   : 3/27/2018
//      Author    : Andy Zelenak
//      Platforms : Ubuntu 64-bit
//      Copyright : Copyright© The University of Texas at Austin, 2014-2017. All rights reserved.
//
//          All files within this directory are subject to the following, unless an alternative
//          license is explicitly included within the text of each file.
//
//          This software and documentation constitute an unpublished work
//          and contain valuable trade secrets and proprietary information
//          belonging to the University. None of the foregoing material may be
//          copied or duplicated or disclosed without the express, written
//          permission of the University. THE UNIVERSITY EXPRESSLY DISCLAIMS ANY
//          AND ALL WARRANTIES CONCERNING THIS SOFTWARE AND DOCUMENTATION,
//          INCLUDING ANY WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//          PARTICULAR PURPOSE, AND WARRANTIES OF PERFORMANCE, AND ANY WARRANTY
//          THAT MIGHT OTHERWISE ARISE FROM COURSE OF DEALING OR USAGE OF TRADE.
//          NO WARRANTY IS EITHER EXPRESS OR IMPLIED WITH RESPECT TO THE USE OF
//          THE SOFTWARE OR DOCUMENTATION. Under no circumstances shall the
//          University be liable for incidental, special, indirect, direct or
//          consequential damages or loss of profits, interruption of business,
//          or related expenses which may arise from use of software or documentation,
//          including but not limited to those resulting from defects in software
//          and/or documentation, or loss or inaccuracy of data of any kind.
//
///////////////////////////////////////////////////////////////////////////////

// Provide a C++ interface for sending motion commands to the jog_arm server.

#include "jog_arm/jog_api.h"

//////////////////////////////////////////////////////////////////////////////////////////////////
// Publish cmds for a Cartesian motion to bring the robot to the target pose.
// Param vel_scale: scales the velocity components of outgoing Twist msgs. Should be 0<vel_scale<1
// Return true if successful.
//////////////////////////////////////////////////////////////////////////////////////////////////
bool jog_api::jacobian_move(const geometry_msgs::PoseStamped& target_pose, const std_msgs::Float64& tolerance, const double& vel_scale)
{
  // Velocity scaling should be between 0 and 1
  if ( 0.>vel_scale || 1.<vel_scale )
  {
    ROS_ERROR_STREAM("[jog_api::jacobian_move] Velocity scaling parameter should be between 0 and 1.");
    return false;
  }

  geometry_msgs::PoseStamped current_pose;
  current_pose = move_group_.getCurrentPose();

  // A structure to hold the result
  distance_and_twist distance_and_twist;
  distance_and_twist = calc_distance_and_twist(current_pose, target_pose, vel_scale);

  // Is the current pose close enough?
  while ( distance_and_twist.distance > tolerance.data && ros::ok() )
  {
    // Get current robot pose
    current_pose = move_group_.getCurrentPose();

    // Update distance and twist to target
    distance_and_twist = calc_distance_and_twist(current_pose, target_pose, vel_scale);

    // Publish the twist commands to move the robot
    jog_vel_pub_.publish(distance_and_twist.twist);
    ros::Duration(0.01).sleep();
  }

  return true;
}


////////////////////////////////////////////////////////////////////////////////
// Calculate Euclidean distance between 2 poses
// Returns a distance_and_twist structure.
// distance_and_twist.distance holds the linear distance to target pose
// distance_and_twist.twist: these components have been normalized between -1:1,
// they can serve as motion commands as if from a joystick.
////////////////////////////////////////////////////////////////////////////////
jog_api::distance_and_twist jog_api::calc_distance_and_twist(const geometry_msgs::PoseStamped &current_pose, const geometry_msgs::PoseStamped &target_pose, const double &vel_scale)
{
  distance_and_twist result;
  
  // Check frames on incoming PoseStampeds
  if ( current_pose.header.frame_id != target_pose.header.frame_id )
  {
    ROS_ERROR_STREAM("[arm_namespace::distance_and_twist] Incoming PoseStampeds tf frames do not match.");
    return result;
  }
  result.twist.header.frame_id = current_pose.header.frame_id;
  result.twist.header.stamp = ros::Time::now();

  // Current pose: convert from quat to RPY
  tf::Quaternion q(current_pose.pose.orientation.x, current_pose.pose.orientation.y, current_pose.pose.orientation.z, current_pose.pose.orientation.w);
  tf::Matrix3x3 m(q);
  double curr_r, curr_p, curr_y;
  m.getRPY(curr_r, curr_p, curr_y);

  // Target pose: convert from quat to RPY
  q = tf::Quaternion(target_pose.pose.orientation.x, target_pose.pose.orientation.y, target_pose.pose.orientation.z, target_pose.pose.orientation.w);
  m = tf::Matrix3x3(q);
  double targ_r, targ_p, targ_y;
  m.getRPY(targ_r, targ_p, targ_y);

  /////////////////////////////////////
  // Calculate the twist to target_pose
  /////////////////////////////////////
  result.twist.twist.linear.x = target_pose.pose.position.x - current_pose.pose.position.x;
  result.twist.twist.linear.y = target_pose.pose.position.y - current_pose.pose.position.y;
  result.twist.twist.linear.z = target_pose.pose.position.z - current_pose.pose.position.z;

  ///////////////////////////////////////////////////////////////////////////////
  // Normalize the twist to the target pose. Calculate distance while we're at it
  ///////////////////////////////////////////////////////////////////////////////
  double sos = pow(result.twist.twist.linear.x, 2.);
  sos += pow(result.twist.twist.linear.y, 2.);
  sos += pow(result.twist.twist.linear.z, 2.);
  // Ignore rot
  result.distance = pow( sos, 0.5);

  result.twist.twist.linear.x = vel_scale*result.twist.twist.linear.x/result.distance;
  result.twist.twist.linear.y = vel_scale*result.twist.twist.linear.y/result.distance;
  result.twist.twist.linear.z = vel_scale*result.twist.twist.linear.z/result.distance;

  // Ignore angle for now
  result.twist.twist.angular.x = 0.;
  result.twist.twist.angular.y = 0.;
  result.twist.twist.angular.z = 0.;

  return result;
}